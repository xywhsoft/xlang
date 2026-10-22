param([string]$Root = "", [switch]$SkipAot)

$ErrorActionPreference = "Stop"
if ($Root -eq "") { $Root = Split-Path -Parent $PSScriptRoot }
$Root = (Resolve-Path -LiteralPath $Root).Path
$xl = Join-Path $Root "release\xl.exe"
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop).Source
$work = Join-Path $Root "build\test\serial_guard"
$utf8 = [Text.UTF8Encoding]::new($false)
$checks = [Collections.Generic.List[object]]::new()
$binaries = @("release/xl.exe", "release/xl.dll", "release/xlc.dll", "lib/xrt/xrt.h")
$before = @{}
foreach ($file in $binaries) { $before[$file] = (Get-FileHash -LiteralPath (Join-Path $Root $file)).Hash }
$sourceHashes = @{}
$sourceFiles = @(Get-ChildItem -LiteralPath (Join-Path $Root "release/extlibs/serial") -Recurse -File |
    Where-Object { $_.Extension -in @(".c", ".h", ".xl", ".json") } | ForEach-Object { $_.FullName })
$sourceFiles += @("tools/check_extlib_serial.ps1", "test/cases/extlib_serial.xl",
    "release/examples/extlibs/serial.xl", "release/examples/extlibs/serial_c.c") |
    ForEach-Object { Join-Path $Root $_ }
foreach ($file in $sourceFiles) {
    $relative = $file.Substring($Root.Length + 1).Replace('\', '/')
    $sourceHashes[$relative] = (Get-FileHash -LiteralPath $file).Hash
}
[IO.Directory]::CreateDirectory($work) | Out-Null



# 每个检查独立进程、固定工作目录和超时；不触碰共享构建对象或宿主内存策略。
function Run-Check([string]$Name, [string]$Program, [string[]]$Arguments, [bool]$ExpectFailure = $false) {
    $process = [Diagnostics.Process]::new()
    $process.StartInfo.FileName = $Program
    $process.StartInfo.WorkingDirectory = $Root
    $process.StartInfo.UseShellExecute = $false
    $process.StartInfo.CreateNoWindow = $true
    $process.StartInfo.RedirectStandardOutput = $true
    $process.StartInfo.RedirectStandardError = $true
    $process.StartInfo.StandardOutputEncoding = $utf8
    $process.StartInfo.StandardErrorEncoding = $utf8
    # 参数都是本脚本构造的独立路径/选项，不包含引号或末尾反斜线。
    $process.StartInfo.Arguments = ($Arguments | ForEach-Object { '"' + $_ + '"' }) -join ' '
    try {
        if (!$process.Start()) { throw "Unable to start $Name" }
        $stdout = $process.StandardOutput.ReadToEndAsync()
        $stderr = $process.StandardError.ReadToEndAsync()
        if (!$process.WaitForExit(60000)) { $process.Kill(); $process.WaitForExit(); throw "$Name timed out" }
        $text = $stdout.GetAwaiter().GetResult().Replace("`r`n", "`n").TrimEnd("`n")
        $errors = $stderr.GetAwaiter().GetResult()
        $checks.Add([ordered]@{ name = $Name; exitCode = $process.ExitCode; stdout = $text; stderr = $errors })
        if ($ExpectFailure) {
            if ($process.ExitCode -eq 0) { throw "$Name unexpectedly passed" }
            Write-Host "[serial] ${Name}: rejected as expected"
            return "$errors`n$text"
        }
        if ($process.ExitCode -ne 0) { throw "$Name failed: $errors`n$text" }
        Write-Host "[serial] ${Name}: PASS"
        return $text
    } finally { $process.Dispose() }
}



$package = "release/extlibs/serial"
$native = "$package/serial_native.c"
$flags = @("-std=c11", "-Wall", "-Wextra", "-Werror", "-Ilib/xrt")
$runtime = Join-Path $work "runtime.o"
$nativeObject = Join-Path $work "serial.o"
$links = @($nativeObject, $runtime, "-lWs2_32", "-lIPHLPAPI", "-lShell32")
$outcome = "failed"
try {
    $null = Run-Check "gcc-runtime" $gcc ($flags + @("-c", "$package/test/serial_runtime.c", "-o", $runtime))
    $null = Run-Check "gcc-native" $gcc ($flags + @("-c", $native, "-o", $nativeObject))
    foreach ($test in @("test_serial", "test_serial_alloc")) {
        $exe = Join-Path $work "$test.exe"
        $null = Run-Check "gcc-$test" $gcc ($flags + @("$package/test/$test.c") + $links + @("-o", $exe))
        $text = Run-Check $test $exe @()
        if ($test -eq "test_serial" -and $text -ne "serial C boundaries: PASS") { throw "C boundary marker missing" }
        if ($test -eq "test_serial_alloc" -and $text -notmatch '^serial allocation checks: rejected=[1-9][0-9]* completed=[1-9][0-9]*$') {
            throw "Allocation checks must exercise failure and success paths"
        }
    }

    # 比较由编译器生成的全部串口 extern 原型与公开 C 头，不靠两侧手写相同原型自证。
    $generated = Join-Path $work "module.c"
    $null = Run-Check "cgen" $xl @("cgen", "test/cases/extlib_serial.xl", "--backend", "xlir", "--emit", $generated)
    $declarations = @(Get-Content -LiteralPath $generated -Encoding UTF8 | Where-Object { $_ -match '^extern .*\bxserial_[a-z_]+\(' })
    if ($declarations.Count -ne 11) { throw "Serial generated API inventory changed; review the complete ABI" }
    $abiCheck = Join-Path $work "api.c"
    [IO.File]::WriteAllText($abiCheck, "#include `"serial.h`"`n" + ($declarations -join "`n") + "`n", $utf8)
    $null = Run-Check "generated-c-prototypes" $gcc ($flags + @("-I$package/include", "-fsyntax-only", $abiCheck))

    $text = Run-Check "c-jit" $xl @("run", "$package/test/test_serial.c", $native, "--inc-path", "lib/xrt")
    if ($text -ne "serial C boundaries: PASS") { throw "C JIT marker missing" }
    $text = Run-Check "xlang-jit" $xl @("run", "test/cases/extlib_serial.xl")
    if ($text -ne "serial xlang boundaries: PASS") { throw "xlang JIT marker missing" }
    foreach ($case in @("text", "bytes")) {
        $name = "private_${case}_negative"
        $expected = if ($case -eq "text") { "writeNative" } else { "writeBytesNative" }
        $diagnostic = Run-Check "xlang-$name" $xl @("check", "$package/test/$name.xl") $true
        if (!$diagnostic.Contains($expected) -or !$diagnostic.Contains("access")) {
            throw "$name did not report private access"
        }
    }
    $null = Run-Check "xlang-nul-port" $xl @("run", "$package/test/nul_port.xl")
    # 无实体串口时只验证设备范例的单文件入口可构建，收发由 Linux PTY 门禁执行。
    $deviceExample = Join-Path $work "device_direct.exe"
    $null = Run-Check "device-c-direct-build" $xl @("build", "$package/examples/device_c.c", "-o", $deviceExample)
    if (!$SkipAot) {
        $aot = Join-Path $work "xlang.exe"
        $null = Run-Check "xlang-build" $xl @("build", "test/cases/extlib_serial.xl", "-o", $aot)
        $text = Run-Check "xlang-aot" $aot @()
        if ($text -ne "serial xlang boundaries: PASS") { throw "xlang AOT marker missing" }
    }

    $exampleXl = Run-Check "example-xlang" $xl @("run", "release/examples/extlibs/serial.xl")
    $exampleC = Run-Check "example-c" $xl @("run", "release/examples/extlibs/serial_c.c")
    $exampleExe = Join-Path $work "example.exe"
    $null = Run-Check "example-gcc-build" $gcc ($flags + @("release/examples/extlibs/serial_c.c", $runtime, "-o", $exampleExe, "-lWs2_32", "-lIPHLPAPI", "-lShell32"))
    $exampleGcc = Run-Check "example-gcc" $exampleExe @()
    if (!$SkipAot) {
        $cAot = Join-Path $work "c-example.exe"
        $null = Run-Check "example-c-build" $xl @("build", "release/examples/extlibs/serial_c.c", "-o", $cAot)
        $exampleAot = Run-Check "example-c-aot" $cAot @()
        if ($exampleXl -cne $exampleAot) { throw "C AOT example output differs" }
    }
    if ($exampleXl -cne $exampleC -or $exampleXl -cne $exampleGcc -or
        ($exampleXl -split "`n").Count -ne 13) {
        throw "C/xlang example output differs"
    }
    foreach ($file in $binaries) {
        if ((Get-FileHash -LiteralPath (Join-Path $Root $file)).Hash -ne $before[$file]) {
            throw "Build changed during verification: $file; rerun on one fixed build"
        }
    }
    foreach ($file in $sourceHashes.Keys) {
        if ((Get-FileHash -LiteralPath (Join-Path $Root $file)).Hash -ne $sourceHashes[$file]) {
            throw "Source changed during verification: $file; rerun after the edit"
        }
    }
    $outcome = "passed"
} finally {
    $report = [ordered]@{
        date = (Get-Date).ToString("o"); package = "serial"; outcome = $outcome
        scope = "Windows x64 loopback, C ABI, ownership, threads, allocation, JIT/AOT; no physical device"
        artifacts = $before; sources = $sourceHashes; checks = @($checks.ToArray())
    }
    [IO.File]::WriteAllText((Join-Path $work "results.json"), ($report | ConvertTo-Json -Depth 8), $utf8)
}
Write-Host "Serial verification passed. Physical device validation remains separate."
