param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$xl = Join-Path $root 'release/xl.exe'
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
    'test/support/extlib_codec_runtime.c', 'tools/run_extlib_c.ps1',
    'tools/check_extlib_native_globals.ps1', 'tools/check_extlib_native_globals.sh',
    'tools/extlib_smoke_process.ps1', 'tools/xlir_native_process.ps1',
    'test/cases/extlib_lzma.xl', 'test/cases/extlib_lzma_progress.xl',
    'release/examples/extlibs/lzma.xl') | ForEach-Object { Join-Path $root $_ }
$inputs += @(Get-ChildItem (Join-Path $root 'release/extlibs/lzma') -Recurse -File |
    ForEach-Object FullName)
$hashes = @{}
foreach ($path in $inputs) { $hashes[$path] = (Get-FileHash -LiteralPath $path).Hash }
$passed = $false

function Run([string]$name, [string]$program, [string[]]$arguments) {
    $previousAction = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        $output = @(& $program @arguments 2>&1 | ForEach-Object { $_.ToString() })
        $code = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previousAction
    }
    [IO.File]::AppendAllText((Join-Path $work 'commands.log'),
        "${name}: $program $($arguments -join ' ')`nexit=$code`n$($output -join "`n")`n")
    if ($code -ne 0) {
        throw "${name}: exit $code`n$($output -join "`n")"
    }
    Write-Host "[lzma] ${name}: PASS"
    return ($output -join "`n").Replace("`r`n", "`n").TrimEnd("`n")
}

function Run-C([string]$name, [string]$source) {
    return Run $name 'powershell' @('-NoProfile', '-ExecutionPolicy', 'Bypass',
        '-File', $runner, '-Package', 'lzma', '-Source', $source)
}

Push-Location $root
try {
    $hashes | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $work 'inputs.before.json')
    $null = Run 'GCC version' $gcc @('--version')
    foreach ($source in @(
        'release/extlibs/lzma/xrt_lzma.c',
        'release/extlibs/lzma/xrt_lzma_common.c',
        'release/extlibs/lzma/xrt_lzma_encode.c',
        'release/extlibs/lzma/xrt_lzma_decode.c',
        'release/extlibs/lzma/lzma_native.c',
        'release/extlibs/lzma/test/test_lzma.c',
        'release/extlibs/lzma/test/test_allocations.c',
        'release/extlibs/lzma/test/test_native_options.c',
        'release/extlibs/lzma/examples/basic_c.c'
    )) {
        $null = Run "strict GCC $source" $gcc ($flags + @('-fsyntax-only', $source))
    }
    foreach ($order in @(@(), @('-DLZMA_CORE_FIRST'))) {
        $null = Run 'GCC native header order' $gcc ($flags + $order + @('-fsyntax-only',
            'release/extlibs/lzma/test/native_header.c'))
    }

    foreach ($kind in @('compressor', 'decompressor')) {
        $source = "release/extlibs/lzma/test/private_${kind}_negative.xl"
        $previousAction = $ErrorActionPreference
        try {
            $ErrorActionPreference = 'Continue'
            $diagnostic = & $xl check $source 2>&1
            $diagnosticExit = $LASTEXITCODE
        } finally {
            $ErrorActionPreference = $previousAction
        }
        [IO.File]::AppendAllText((Join-Path $work 'commands.log'),
            "private ${kind}: exit=$diagnosticExit`n$($diagnostic -join "`n")`n")
        if ($diagnosticExit -eq 0 -or
            ($diagnostic -join "`n") -notmatch 'updateNative' -or
            ($diagnostic -join "`n") -notmatch 'access') {
            throw "private member was not rejected as expected: $source`n$($diagnostic -join "`n")"
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
    foreach ($name in @('test_lzma', 'test_allocations', 'test_native_options')) {
        $binary = Join-Path $work "$name.exe"
        $null = Run "GCC $name build" $gcc ($flags + @("$package/test/$name.c") +
            $objects + $links + @('-o', $binary))
        $null = Run "GCC $name" $binary @()
    }
    $null = Run-C 'TCC native options' "$package/test/test_native_options.c"
    $interopBinary = Join-Path $work 'interop.exe'
    $null = Run 'GCC XZ interop build' $gcc ($flags + @("$package/test/interop.c") +
        $objects + $links + @('-o', $interopBinary))
    $null = Run 'standard XZ interop' $python @(
        '-I', 'release/extlibs/lzma/test/interop.py', $interopBinary)
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
    }
    $lifecycle = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File',
        (Join-Path $root 'tools/check_extlib_native_globals.ps1'), '-Root', $root, '-Package', 'lzma')
    if ($SkipAot) { $lifecycle += '-SkipAot' }
    $null = Run 'top-level lifecycle' (Get-Process -Id $PID).Path $lifecycle
    foreach ($path in $inputs) {
        if ((Get-FileHash -LiteralPath $path).Hash -cne $hashes[$path]) {
            throw "lzma validation input changed: $path"
        }
    }
    $passed = $true
    Write-Host 'lzma Windows package gate: PASS'
} finally {
    Pop-Location
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
}
