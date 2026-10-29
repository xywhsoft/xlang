param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$testRoot = [IO.Path]::GetFullPath((Join-Path $root 'build/test'))
$work = Join-Path $testRoot ('cbor_guard_' + [guid]::NewGuid().ToString('N'))
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop).Source
$xl = Join-Path $root 'release/xl.exe'
$flags = @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-Ilib/xrt',
    '-Irelease/extlibs/cbor')
$links = @('-lWs2_32', '-lAdvapi32', '-lIphlpapi', '-lm')

function Run([string]$name, [string]$program, [string[]]$arguments) {
    $output = & $program @arguments 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "${name}: exit $LASTEXITCODE`n$($output -join "`n")"
    }
    Write-Host "[cbor] ${name}: PASS"
    return ($output -join "`n").Replace("`r`n", "`n").TrimEnd("`n")
}

Push-Location $root
try {
    [IO.Directory]::CreateDirectory($work) | Out-Null
    $null = Run 'manifest C syntax' $gcc ($flags + @(
        '-DXRT_MODULE_BUFFER', '-DXRT_MODULE_BUFFER_HEX',
        '-DXRT_MODULE_CODEC_HEX', '-DXRT_MODULE_STRING',
        '-DXRT_MODULE_TIME_TEXT', '-DXRT_MODULE_UNICODE',
        '-DXRT_MODULE_VALUE_CONTAINER', '-fsyntax-only',
        'release/extlibs/cbor/cbor_native.c'))
    $runtime = Join-Path $work 'xrt_runtime.o'
    $null = Run 'XRT compile' $gcc ($flags + @('-c',
        'release/extlibs/cbor/test/xrt_runtime.c', '-o', $runtime))
    foreach ($case in @('test_cbor', 'test_boundaries', 'test_allocations')) {
        $target = Join-Path $work "$case.exe"
        $null = Run "$case compile" $gcc ($flags + @(
            'release/extlibs/cbor/cbor_native.c',
            "release/extlibs/cbor/test/$case.c", $runtime) +
            $links + @('-o', $target))
        $null = Run $case $target @()
    }
    $example = Join-Path $work 'cbor_example.exe'
    $null = Run 'C example compile' $gcc ($flags + @(
        'release/extlibs/cbor/examples/cbor_c.c', $runtime) +
        $links + @('-o', $example))
    $cOutput = Run 'C example' $example @()
    $expectedPath = Join-Path $root 'release/extlibs/cbor/examples/basic.expected.txt'
    $expected = [IO.File]::ReadAllText($expectedPath).Replace("`r`n", "`n").TrimEnd("`n")
    if ($cOutput -cne $expected) {
        throw 'C CBOR example differs from fixed expectation'
    }

    Push-Location (Join-Path $root 'release')
    try {
        $null = Run 'XLang boundaries' $xl @('run', 'extlibs/cbor/test/boundaries.xl')
        $null = Run 'XLang allocation failures' $xl @('run', 'extlibs/cbor/test/allocations.xl')
        $xlOutput = Run 'XLang example' $xl @('run', 'extlibs/cbor/examples/cbor.xl')
        if ($cOutput -cne $xlOutput) {
            throw "C/XLang CBOR examples differ:`nC:`n$cOutput`nXL:`n$xlOutput"
        }
        Write-Host '[cbor] C/XLang parity: PASS'
        $runOutput = Run 'C example via xl run' $xl @('run', 'extlibs/cbor/examples/cbor_c.c')
        if ($runOutput -cne $cOutput) {
            throw 'xl run CBOR C example differs from GCC'
        }
        $null = Run 'Public XLang example' $xl @('run', 'examples/extlibs/cbor.xl')
        if (-not $SkipAot) {
            $core = Join-Path $work 'cbor_xl_core.exe'
            $null = Run 'XLang core build' $xl @('build',
                'extlibs/cbor/test/aot_core.xl', '-o', $core)
            $null = Run 'XLang core AOT' $core @()
            $target = Join-Path $work 'cbor_xl_test.exe'
            $null = Run 'XLang boundaries build' $xl @('build',
                'extlibs/cbor/test/boundaries.xl', '-o', $target)
            $null = Run 'XLang boundaries AOT' $target @()
            $exampleAot = Join-Path $work 'cbor_xl_example.exe'
            $null = Run 'XLang example build' $xl @('build',
                'extlibs/cbor/examples/cbor.xl', '-o', $exampleAot)
            $aotOutput = Run 'XLang example AOT' $exampleAot @()
            if ($aotOutput -cne $cOutput) {
                throw 'XLang AOT CBOR example differs from C'
            }
        }
    } finally {
        Pop-Location
    }
} finally {
    Pop-Location
    $safeWork = [IO.Path]::GetFullPath($work)
    if (-not $safeWork.StartsWith($testRoot + [IO.Path]::DirectorySeparatorChar,
        [StringComparison]::OrdinalIgnoreCase)) {
        throw "Unsafe test cleanup path: $safeWork"
    }
    if (Test-Path -LiteralPath $safeWork) {
        Remove-Item -LiteralPath $safeWork -Recurse -Force
    }
}
