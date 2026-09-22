param([string]$Root = "")

$ErrorActionPreference = "Stop"
if ( $Root -eq "" ) { $Root = Split-Path -Parent $PSScriptRoot }
$Root = (Resolve-Path -LiteralPath $Root).Path
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop).Source
$xl = Join-Path $Root "release\xl.exe"
$work = Join-Path $Root "build\test\mqtt_guard"
$utf8 = [Text.UTF8Encoding]::new($false)
$checks = [Collections.Generic.List[object]]::new()

if ( ! (Test-Path -LiteralPath $xl -PathType Leaf) ) {
    throw "xl.exe not found: $xl"
}
[IO.Directory]::CreateDirectory($work) | Out-Null

$package = "release/extlibs/mqtt"
$packageSources = @(
    "$package/mqtt_frame.c",
    "$package/mqtt_build.c",
    "$package/mqtt_parse.c",
    "$package/mqtt_net.c",
    "$package/mqtt_native.c"
)
$required = @(
    "tools/check_extlib_mqtt.ps1",
    "$package/xlang.package.json",
    "$package/include/xrt_mqtt.h",
    "$package/include/xrt_mqtt_net.h",
    "$package/include/mqtt_native.h",
    "$package/test/mqtt_runtime.c",
    "$package/test/mqtt_fixture.c",
    "$package/test/mqtt_fixture.h",
    "$package/test/test_mqtt.c",
    "$package/test/test_mqtt_alloc.c",
    "$package/test/test_mqtt_net.c",
    "$package/test/mqtt_boundaries.xl",
    "$package/test/mqtt_net_test.xl"
) + $packageSources
$productFiles = @("release/xl.exe", "release/xl.dll", "release/xlc.dll", "lib/xrt/xrt.h")
$inputHashes = @{}
$productHashes = @{}
foreach ( $relative in $required ) {
    $path = Join-Path $Root $relative
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) { throw "MQTT guard input missing: $relative" }
    $inputHashes[$relative] = (Get-FileHash -LiteralPath $path).Hash
}
foreach ( $relative in $productFiles ) {
    $productHashes[$relative] = (Get-FileHash -LiteralPath (Join-Path $Root $relative)).Hash
}

function Invoke-MqttProcess([string]$Name, [string]$Program, [string[]]$Arguments, [int]$TimeoutMs = 60000)
{
    $process = [Diagnostics.Process]::new()
    $process.StartInfo.FileName = $Program
    $process.StartInfo.WorkingDirectory = $Root
    $process.StartInfo.UseShellExecute = $false
    $process.StartInfo.CreateNoWindow = $true
    $process.StartInfo.RedirectStandardOutput = $true
    $process.StartInfo.RedirectStandardError = $true
    $process.StartInfo.StandardOutputEncoding = $utf8
    $process.StartInfo.StandardErrorEncoding = $utf8
    $process.StartInfo.Arguments = ($Arguments | ForEach-Object { '"' + $_ + '"' }) -join ' '
    try {
        if ( ! $process.Start() ) { throw "unable to start MQTT check: $Name" }
        $stdoutTask = $process.StandardOutput.ReadToEndAsync()
        $stderrTask = $process.StandardError.ReadToEndAsync()
        if ( ! $process.WaitForExit($TimeoutMs) ) {
            $process.Kill()
            $process.WaitForExit()
            throw "MQTT check timed out: $Name"
        }
        $stdout = $stdoutTask.GetAwaiter().GetResult().Replace("`r`n", "`n").TrimEnd("`n")
        $stderr = $stderrTask.GetAwaiter().GetResult().Replace("`r`n", "`n").TrimEnd("`n")
        $checks.Add([ordered]@{ Name=$Name; ExitCode=$process.ExitCode; Stdout=$stdout; Stderr=$stderr }) | Out-Null
        if ( $process.ExitCode -ne 0 ) { throw "MQTT check failed: $Name`n$stderr`n$stdout" }
        Write-Host "[mqtt] ${Name}: PASS"
        return $stdout
    } finally {
        $process.Dispose()
    }
}

$flags = @("-std=c11", "-Wall", "-Wextra", "-Werror", "-Ilib/xrt", "-I$package", "-I$package/include")
$links = @("-lWs2_32", "-lIPHLPAPI", "-lShell32")
$runtimeObject = Join-Path $work "mqtt_runtime.o"
$outcome = "failed"
$oldPackagePath = [Environment]::GetEnvironmentVariable("XLANG_PACKAGE_PATH", "Process")
try {
    $null = Invoke-MqttProcess "gcc-runtime" $gcc ($flags + @("-c", "$package/test/mqtt_runtime.c", "-o", $runtimeObject))

    foreach ( $test in @("test_mqtt", "test_mqtt_alloc", "test_mqtt_net") ) {
        $exe = Join-Path $work "$test.exe"
        $testSources = @("$package/test/$test.c")
        if ( $test -eq "test_mqtt_net" ) { $testSources += "$package/test/mqtt_fixture.c" }
        $null = Invoke-MqttProcess "gcc-$test" $gcc ($flags + $testSources + $packageSources + @($runtimeObject) + $links + @("-o", $exe))
        $text = Invoke-MqttProcess $test $exe @()
        if ( $test -eq "test_mqtt" -and $text -ne "mqtt C codec boundaries: PASS" ) {
            throw "MQTT C codec marker missing"
        }
        if ( $test -eq "test_mqtt_alloc" -and $text -notmatch '^mqtt allocation checks: rejected=[1-9][0-9]* completed=[1-9][0-9]*$' ) {
            throw "MQTT allocation check did not exercise failure and success paths"
        }
        if ( $test -eq "test_mqtt_net" -and $text -ne "mqtt C TCP boundaries: PASS" ) {
            throw "MQTT C TCP marker missing"
        }
    }

    [Environment]::SetEnvironmentVariable("XLANG_PACKAGE_PATH", "release\extlibs;test\fixtures", "Process")
    $codecSource = "$package/test/mqtt_boundaries.xl"
    $netSource = "$package/test/mqtt_net_test.xl"
    $fixtureSource = "$package/test/mqtt_fixture.c"
    $text = Invoke-MqttProcess "xlang-codec-jit" $xl @("run", $codecSource)
    if ( $text -ne "mqtt xlang codec boundaries: PASS" ) { throw "MQTT xlang codec marker missing" }
    $text = Invoke-MqttProcess "xlang-net-jit" $xl @("run", $netSource, $fixtureSource)
    if ( $text -ne "mqtt xlang TCP boundaries: PASS" ) { throw "MQTT xlang TCP marker missing" }

    $codecExe = Join-Path $work "mqtt_xlang.exe"
    $netExe = Join-Path $work "mqtt_net_xlang.exe"
    $null = Invoke-MqttProcess "xlang-codec-build" $xl @("build", $codecSource, "-o", $codecExe)
    $text = Invoke-MqttProcess "xlang-codec-aot" $codecExe @()
    if ( $text -ne "mqtt xlang codec boundaries: PASS" ) { throw "MQTT xlang codec AOT marker missing" }
    $null = Invoke-MqttProcess "xlang-net-build" $xl @("build", $netSource, $fixtureSource, "-o", $netExe)
    $text = Invoke-MqttProcess "xlang-net-aot" $netExe @()
    if ( $text -ne "mqtt xlang TCP boundaries: PASS" ) { throw "MQTT xlang TCP AOT marker missing" }

    foreach ( $relative in $required ) {
        if ( (Get-FileHash -LiteralPath (Join-Path $Root $relative)).Hash -ne $inputHashes[$relative] ) {
            throw "MQTT source changed during verification: $relative"
        }
    }
    foreach ( $relative in $productFiles ) {
        if ( (Get-FileHash -LiteralPath (Join-Path $Root $relative)).Hash -ne $productHashes[$relative] ) {
            throw "release product changed during MQTT verification: $relative"
        }
    }
    $outcome = "passed"
} finally {
    [Environment]::SetEnvironmentVariable("XLANG_PACKAGE_PATH", $oldPackagePath, "Process")
    $report = [ordered]@{
        Date=(Get-Date).ToString("o")
        Package="mqtt"
        Outcome=$outcome
        Scope="MQTT 3.1.1 codec, allocation failure, local TCP framing, xlang JIT/AOT; no public broker or session engine"
        Inputs=$inputHashes
        Products=$productHashes
        Checks=@($checks.ToArray())
    }
    [IO.File]::WriteAllText((Join-Path $work "results.json"), ($report | ConvertTo-Json -Depth 8), $utf8)
}

Write-Host "MQTT native and xlang boundary verification passed."
