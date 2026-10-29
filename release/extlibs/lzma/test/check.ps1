param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$xl = Join-Path $root 'release/xl.exe'
$runner = Join-Path $root 'tools/run_extlib_c.ps1'
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop).Source
$workParent = Join-Path $root 'build/test/lzma_guard'
$work = Join-Path $workParent ('run_' + [guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($work) | Out-Null

function Run([string]$name, [string]$program, [string[]]$arguments) {
    $output = & $program @arguments 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "${name}: exit $LASTEXITCODE`n$($output -join "`n")"
    }
    Write-Host "[lzma] ${name}: PASS"
    return ($output -join "`n").Replace("`r`n", "`n").TrimEnd("`n")
}

function Run-C([string]$name, [string]$source) {
    $output = & $runner -Package lzma -Source $source 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "${name}: exit $LASTEXITCODE`n$($output -join "`n")"
    }
    Write-Host "[lzma] ${name}: PASS"
    return ($output -join "`n").Replace("`r`n", "`n").TrimEnd("`n")
}

Push-Location $root
try {
    foreach ($source in @(
        'release/extlibs/lzma/xrt_lzma.c',
        'release/extlibs/lzma/xrt_lzma_common.c',
        'release/extlibs/lzma/xrt_lzma_encode.c',
        'release/extlibs/lzma/xrt_lzma_decode.c',
        'release/extlibs/lzma/lzma_native.c',
        'release/extlibs/lzma/test/test_lzma.c',
        'release/extlibs/lzma/test/test_allocations.c',
        'release/extlibs/lzma/examples/basic_c.c'
    )) {
        $null = Run "strict GCC $source" 'gcc' @(
            '-std=c11', '-Wall', '-Wextra', '-Werror',
            '-DXRT_MODULE_ONCE', '-fsyntax-only',
            '-Ilib/xrt', '-Ilib/xruntime',
            '-Irelease/extlibs/lzma/include', $source)
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
    $allocBinary = Join-Path $work 'test_allocations.exe'
    $null = Run 'GCC allocation build' $gcc (@(
        '-std=c11', '-O2', '-DXRT_MODULE_ONCE',
        '-Ilib/xrt', '-Ilib/xruntime', '-Irelease/extlibs/lzma/include',
        'test/support/extlib_codec_runtime.c',
        'release/extlibs/lzma/test/test_allocations.c') + $sources + @(
        '-lws2_32', '-ladvapi32', '-liphlpapi', '-lm', '-o', $allocBinary))
    $null = Run 'GCC allocation failures' $allocBinary @()
    $interopBinary = Join-Path $work 'interop.exe'
    $null = Run 'GCC XZ interop build' $gcc (@(
        '-std=c11', '-O2', '-DXRT_MODULE_ONCE',
        '-Ilib/xrt', '-Ilib/xruntime', '-Irelease/extlibs/lzma/include',
        'test/support/extlib_codec_runtime.c',
        'release/extlibs/lzma/test/interop.c') + $sources + @(
        '-lws2_32', '-ladvapi32', '-liphlpapi', '-lm', '-o', $interopBinary))
    $null = Run 'standard XZ interop' 'python' @(
        '-I', 'release/extlibs/lzma/test/interop.py', $interopBinary)
    foreach ($source in @(
        'release/extlibs/lzma/test/boundaries.xl',
        'release/extlibs/lzma/test/file_ops.xl',
        'release/extlibs/lzma/test/allocations.xl',
        'release/extlibs/lzma/test/text_transfer.xl',
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
        foreach ($name in @('boundaries', 'file_ops', 'allocations')) {
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
    Write-Host 'lzma Windows package gate: PASS'
} finally {
    Pop-Location
    $parentPath = [IO.Path]::GetFullPath($workParent).TrimEnd('\') + '\'
    $workPath = [IO.Path]::GetFullPath($work)
    if (-not $workPath.StartsWith($parentPath,
        [StringComparison]::OrdinalIgnoreCase)) {
        throw "lzma temporary work path escaped build/test: $workPath"
    }
    Remove-Item -LiteralPath $workPath -Recurse -Force
}
