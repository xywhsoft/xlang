param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$xl = Join-Path $root 'release/xl.exe'
$shell = (Get-Process -Id $PID).Path
$runner = Join-Path $root 'tools/run_extlib_c.ps1'
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$python = (Get-Command python -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$flags = @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
    '-DXRT_MODULE_ONCE', '-DXRT_MODULE_DIR_TEMP',
    '-Ilib/xrt', '-Ilib/xruntime', '-Irelease/extlibs/lzma/include')
$links = @('-lws2_32', '-ladvapi32', '-liphlpapi', '-lm')
$workParent = Join-Path $root 'build/test/lzma_guard'
$work = Join-Path $workParent ('run_' + [guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($work) | Out-Null
$inputs = @('release/xl.exe', 'release/xl.dll', 'release/xlc.dll',
    'lib/xrt/xrt.h', 'lib/xruntime/xruntime.h', 'release/include/xrt_decl.h',
    'release/include/xruntime_decl.h', 'release/include/xl_extlib_abi.h',
    'test/support/extlib_codec_runtime.c', 'test/support/extlib_native_codec_file_rollback.h',
    'tools/run_extlib_c.ps1',
    'tools/check_extlib_native_globals.ps1', 'tools/check_extlib_native_globals.sh',
    'tools/extlib_smoke_process.ps1', 'tools/xlir_native_process.ps1',
    'test/cases/extlib_lzma.xl', 'test/cases/extlib_lzma_progress.xl',
    'release/examples/extlibs/lzma.xl') | ForEach-Object { Join-Path $root $_ }
$inputs += @(Get-ChildItem (Join-Path $root 'release/extlibs/lzma') -Recurse -File |
    ForEach-Object FullName)
$hashes = @{}
foreach ($path in $inputs) { $hashes[$path] = (Get-FileHash -LiteralPath $path).Hash }
$passed = $false
$checks = [Collections.Generic.List[object]]::new()
. (Join-Path $root 'tools/extlib_smoke_process.ps1')
foreach ($path in @($gcc, $python, $shell)) { $hashes[$path] = (Get-FileHash -LiteralPath $path).Hash }

function Run([string]$name, [string]$program, [string[]]$arguments, [int]$expectedExit = 0) {
    if ($null -eq $arguments) { $arguments = @() }
    $result = Invoke-ExtlibSmokeProcess $program $arguments $root 240 @{LC_ALL='C'}
    $checks.Add([ordered]@{name=$name;program=$program;arguments=$arguments;expectedExit=$expectedExit;result=$result})
    [IO.File]::AppendAllText((Join-Path $work 'commands.log'),
        "${name}: $program $($arguments -join ' ')`nstatus=$($result.status) exit=$($result.exitCode)`nstdout:`n$($result.stdout)`nstderr:`n$($result.stderr)`n")
    if ($result.status -notin @('pass','fail') -or $result.exitCode -ne $expectedExit) {
        throw "${name}: $($result.status) exit=$($result.exitCode)`n$($result.stdout)`n$($result.stderr)"
    }
    Write-Host "[lzma] ${name}: PASS"
    return ($result.stdout + $result.stderr).Replace("`r`n", "`n").TrimEnd("`n")
}

function Run-C([string]$name, [string]$source) {
    return Run $name $shell @('-NoProfile', '-ExecutionPolicy', 'Bypass',
        '-File', $runner, '-Package', 'lzma', '-Source', $source)
}

Push-Location $root
try {
    $null = Run 'GCC version' $gcc @('--version')
    $cc1 = Run 'GCC cc1 path' $gcc @('-print-prog-name=cc1')
    $hashes[$cc1] = (Get-FileHash -LiteralPath $cc1).Hash
    $null = Run 'Python version' $python @('--version')
    $hashes | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $work 'inputs.before.json') -Encoding UTF8
    foreach ($source in @(
        'release/extlibs/lzma/xrt_lzma.c',
        'release/extlibs/lzma/xrt_lzma_common.c',
        'release/extlibs/lzma/xrt_lzma_encode.c',
        'release/extlibs/lzma/xrt_lzma_decode.c',
        'release/extlibs/lzma/lzma_native.c',
        'release/extlibs/lzma/test/test_lzma.c',
        'release/extlibs/lzma/test/test_allocations.c',
        'release/extlibs/lzma/test/test_native_options.c',
        'release/extlibs/lzma/examples/basic_c.c',
        'release/extlibs/lzma/examples/file_transaction_c.c',
        'release/extlibs/lzma/test/file_rollback_c.c'
    )) {
        $null = Run "strict GCC $source" $gcc ($flags + @('-fsyntax-only', $source))
    }
    foreach ($order in @(@(), @('-DLZMA_CORE_FIRST'))) {
        $null = Run 'GCC native header order' $gcc ($flags + $order + @('-fsyntax-only',
            'release/extlibs/lzma/test/native_header.c'))
    }

    foreach ($kind in @('compressor', 'decompressor')) {
        $source = "release/extlibs/lzma/test/private_${kind}_negative.xl"
        $diagnostic = Run "private $kind must reject" $xl @('check', $source) -expectedExit 1
        if ($diagnostic -notmatch 'updateNative' -or $diagnostic -notmatch 'access') {
            throw "private member was not rejected as expected: $source`n$diagnostic"
        }
        Write-Host "[lzma] private ${kind}: PASS"
    }

    $null = Run-C 'C boundaries' 'release/extlibs/lzma/test/test_lzma.c'
    $package = 'release/extlibs/lzma'
    $manifest = Get-Content "$package/xlang.package.json" -Raw | ConvertFrom-Json
    $sources = @($manifest.artifact.cSources | ForEach-Object { "$package/$_" })
    # 运行库与 SDK 每次门禁只编译一次，测试共享本次输入生成的对象文件。
    $runtime = Join-Path $work 'runtime.o'
    $null = Run 'GCC runtime build' $gcc ($flags + @('-c',
        'test/support/extlib_codec_runtime.c', '-o', $runtime))
    $objects = @($runtime)
    foreach ($source in $sources) {
        $object = Join-Path $work (([IO.Path]::GetFileNameWithoutExtension($source)) + '.o')
        $sourceFlags = if ($source -match '/third_party/') { @('-std=c11', '-O2') } else { $flags }
        $null = Run "GCC native build $source" $gcc ($sourceFlags + @('-c', $source, '-o', $object))
        $objects += $object
    }
    foreach ($name in @('test_lzma', 'test_allocations', 'test_native_options', 'file_rollback_c')) {
        $binary = Join-Path $work "$name.exe"
        $null = Run "GCC $name build" $gcc ($flags + @("$package/test/$name.c") +
            $objects + $links + @('-o', $binary))
        $null = Run "GCC $name" $binary @()
    }
    $null = Run-C 'TCC native options' "$package/test/test_native_options.c"
    $null = Run-C 'TCC open rollback' "$package/test/file_rollback_c.c"
    $interopBinary = Join-Path $work 'interop.exe'
    $null = Run 'GCC XZ interop build' $gcc ($flags + @("$package/test/interop.c") +
        $objects + $links + @('-o', $interopBinary))
    $null = Run 'standard XZ interop' $python @(
        '-I', '-O', 'release/extlibs/lzma/test/interop.py', $interopBinary)
    foreach ($source in @(
        'release/extlibs/lzma/test/boundaries.xl',
        'release/extlibs/lzma/test/file_ops.xl',
        'release/extlibs/lzma/test/allocations.xl',
        'release/extlibs/lzma/test/text_transfer.xl',
        'release/extlibs/lzma/test/native_options.xl',
        'test/cases/extlib_lzma_progress.xl',
        'test/cases/extlib_lzma.xl'
    )) {
        $null = Run "XLang $source" $xl @('run', $source)
    }
    $fullOutput = Run 'XLang full example' $xl @('run',
        'release/examples/extlibs/lzma.xl')

    $cOutput = Run-C 'C example' 'release/extlibs/lzma/examples/basic_c.c'
    $directSource = 'release/extlibs/lzma/examples/basic_c.c'
    $directOutput = Run 'TCC C single-file example' $xl @('run', $directSource)
    $directBinary = Join-Path $work 'lzma_c_single_file.exe'
    $null = Run 'TCC C single-file build' $xl @('build', $directSource,
        '-o', $directBinary)
    $directAotOutput = Run 'TCC C single-file executable' $directBinary @()
    $xlOutput = Run 'XLang example' $xl @('run',
        'release/extlibs/lzma/examples/basic.xl')
    if ($cOutput -cne $xlOutput -or $cOutput -cne $directOutput -or
        $cOutput -cne $directAotOutput) {
        throw "C/XLang example output differs:`nC: $cOutput`nDirect: $directOutput`nXL: $xlOutput"
    }
    Write-Host '[lzma] C/XLang example parity: PASS'
    $golden = [IO.File]::ReadAllText((Join-Path $root "$package/examples/file_transaction.expected.txt")).Replace("`r`n", "`n").TrimEnd("`n")
    $transactionC = Join-Path $work 'file_transaction_c.exe'
    $null = Run 'GCC transaction example build' $gcc ($flags + @("$package/examples/file_transaction_c.c") +
        $objects + $links + @('-o', $transactionC))
    $output = Run 'GCC transaction example' $transactionC @()
    if ($output -cne $golden) { throw 'C transaction example differs from golden' }
    foreach ($source in @("$package/examples/file_transaction_c.c", "$package/examples/file_transaction.xl")) {
        $output = Run "transaction example $source" $xl @('run', $source)
        if ($output -cne $golden) { throw 'transaction example differs from golden' }
        if (-not $SkipAot) {
            $exe = Join-Path $work ([IO.Path]::GetFileName($source) + '.exe')
            $null = Run "$source AOT build" $xl @('build', $source, '-o', $exe)
            $output = Run "$source AOT" $exe @()
            if ($output -cne $golden) { throw 'AOT transaction example differs from golden' }
        }
    }

    if (-not $SkipAot) {
        $textTarget = Join-Path $work 'text_transfer.exe'
        $null = Run 'text transfer AOT build' $xl @('build',
            'release/extlibs/lzma/test/text_transfer.xl', '-o', $textTarget)
        $null = Run 'text transfer AOT run' $textTarget @()
        $target = Join-Path $work 'aot_core.exe'
        $null = Run 'core AOT build' $xl @('build',
            'release/extlibs/lzma/test/aot_core.xl', '-o', $target)
        $null = Run 'core AOT run' $target @()
        $centralTarget = Join-Path $work 'aot_central.exe'
        $null = Run 'central AOT build' $xl @('build',
            'test/cases/extlib_lzma.xl', '-o', $centralTarget)
        $null = Run 'central AOT run' $centralTarget @()
        foreach ($name in @('boundaries', 'file_ops', 'allocations', 'native_options')) {
            $source = "release/extlibs/lzma/test/$name.xl"
            $exe = Join-Path $work "aot_$name.exe"
            $null = Run "$name AOT build" $xl @('build', $source, '-o', $exe)
            $null = Run "$name AOT run" $exe @()
        }
        $progressExe = Join-Path $work 'aot_progress.exe'
        $null = Run 'progress AOT build' $xl @('build',
            'test/cases/extlib_lzma_progress.xl', '-o', $progressExe)
        $null = Run 'progress AOT run' $progressExe @()
        $fullExe = Join-Path $work 'aot_full_example.exe'
        $null = Run 'full example AOT build' $xl @('build',
            'release/examples/extlibs/lzma.xl', '-o', $fullExe)
        $fullAot = Run 'full example AOT run' $fullExe @()
        if ($fullAot -cne $fullOutput) { throw 'full example JIT/AOT output differs' }
        $basicExe = Join-Path $work 'aot_basic_example.exe'
        $null = Run 'basic example AOT build' $xl @('build',
            'release/extlibs/lzma/examples/basic.xl', '-o', $basicExe)
        $basicAot = Run 'basic example AOT run' $basicExe @()
        if ($basicAot -cne $cOutput) { throw 'basic example C/AOT output differs' }
        $rollback = Join-Path $work 'file_rollback_aot.exe'
        $null = Run 'C open rollback AOT build' $xl (@('build', "$package/test/file_rollback_c.c") +
            $sources + @('--inc-path', 'lib/xrt', '--inc-path', 'lib/xruntime', '-o', $rollback))
        $null = Run 'C open rollback AOT' $rollback @()
    }
    $lifecycle = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File',
        (Join-Path $root 'tools/check_extlib_native_globals.ps1'), '-Root', $root, '-Package', 'lzma')
    if ($SkipAot) { $lifecycle += '-SkipAot' }
    # 两项都执行并保留原始失败；不能用一个阻断掩盖另一个。
    $boundaries = [Collections.Generic.List[string]]::new()
    try { $null = Run 'closed transaction rollback' (Join-Path $work 'file_rollback_c.exe') @('--closed') }
    catch { $boundaries.Add($_.Exception.Message) }
    try { $null = Run 'top-level lifecycle' $shell $lifecycle }
    catch { $boundaries.Add($_.Exception.Message) }
    if ($boundaries.Count) { throw ($boundaries -join "`n") }
    $passed = $true
    Write-Host 'lzma Windows package gate: PASS'
} catch {
    [IO.File]::WriteAllText((Join-Path $work 'failure.txt'), $_.Exception.Message, [Text.UTF8Encoding]::new($false))
    throw
} finally {
    Pop-Location
    $unchanged = $true
    foreach ($path in $hashes.Keys) {
        if (-not (Test-Path -LiteralPath $path) -or (Get-FileHash -LiteralPath $path).Hash -cne $hashes[$path]) {
            $unchanged = $false
            Write-Warning "lzma validation input changed: $path"
        }
    }
    $passed = $passed -and $unchanged
    [ordered]@{passed=$passed;unchanged=$unchanged;inputs=$hashes;checks=$checks} |
        ConvertTo-Json -Depth 9 | Set-Content ($work + '.report.json') -Encoding UTF8
    $parentPath = [IO.Path]::GetFullPath($workParent).TrimEnd('\') + '\'
    $workPath = [IO.Path]::GetFullPath($work)
    if (-not $workPath.StartsWith($parentPath,
        [StringComparison]::OrdinalIgnoreCase)) {
        throw "lzma temporary work path escaped build/test: $workPath"
    }
    if ($passed) {
        # 本目录只有本次门禁的平面构建产物；失败时保留命令、诊断与指纹。
        Get-ChildItem -LiteralPath $workPath -File | ForEach-Object {
            Remove-Item -LiteralPath $_.FullName -Force
        }
        Remove-Item -LiteralPath $workPath -Force
    } else {
        Write-Warning "lzma failed; artifacts retained: $workPath"
    }
    if (-not $unchanged) { throw 'lzma validation inputs changed' }
}
