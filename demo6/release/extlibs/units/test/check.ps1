param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$testRoot = [IO.Path]::GetFullPath((Join-Path $root 'build/test'))
$work = Join-Path $testRoot ('units_guard_' + [guid]::NewGuid().ToString('N'))
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop).Source
$xl = Join-Path $root 'release/xl.exe'
$flags = @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
    '-Ilib/xrt', '-Irelease/extlibs/units/include')
$links = @('-lWs2_32', '-lAdvapi32', '-lIphlpapi', '-lm')

function Run([string]$name, [string]$program, [string[]]$arguments) {
    $previousPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $output = & $program @arguments 2>&1
    } finally {
        $ErrorActionPreference = $previousPreference
    }
    if ($LASTEXITCODE -ne 0) {
        throw "${name}: exit $LASTEXITCODE`n$($output -join "`n")"
    }
    Write-Host "[units] ${name}: PASS"
    return ($output -join "`n").Replace("`r`n", "`n").TrimEnd("`n")
}

Push-Location $root
try {
    [IO.Directory]::CreateDirectory($work) | Out-Null
    $null = Run 'ABI contract' $gcc ($flags + @('-fsyntax-only',
        'release/extlibs/units/test/abi_contract.c'))
    $runtime = Join-Path $work 'xrt_runtime.o'
    $null = Run 'XRT compile' $gcc ($flags + @('-c',
        'release/extlibs/units/test/xrt_runtime.c', '-o', $runtime))
    $native = Join-Path $work 'units_native.o'
    $null = Run 'native compile' $gcc ($flags + @('-c',
        'release/extlibs/units/units_native.c', '-o', $native))
    foreach ($case in @('test_units', 'test_threads')) {
        $target = Join-Path $work "$case.exe"
        $null = Run "$case compile" $gcc ($flags + @(
            "release/extlibs/units/test/$case.c", $native, $runtime) +
            $links + @('-o', $target))
        $null = Run $case $target @()
    }
    $example = Join-Path $work 'example_units.exe'
    $null = Run 'C example compile' $gcc ($flags + @(
        'release/extlibs/units/examples/units_c.c', $native, $runtime) +
        $links + @('-o', $example))
    $cOutput = Run 'C example' $example @()
    $directOutput = Run 'C example xl run' $xl @('run',
        'release/extlibs/units/examples/units_c.c')
    if ($directOutput -cne $cOutput) {
        throw 'direct C example output differs from GCC example'
    }

    $null = Run 'XLang central boundaries' $xl @('run', 'test/cases/extlib_units.xl')
    $null = Run 'XLang package boundaries' $xl @('run',
        'release/extlibs/units/test/boundaries.xl')
    $null = Run 'XLang allocation failures' $xl @('run',
        'release/extlibs/units/test/allocations.xl')
    $xlOutput = Run 'XLang example' $xl @('run',
        'release/examples/extlibs/units.xl')
    if ($cOutput -cne $xlOutput) {
        throw "C/XLang examples differ:`nC:`n$cOutput`nXL:`n$xlOutput"
    }
    Write-Host '[units] C/XLang parity: PASS'

    if (-not $SkipAot) {
        $aot = Join-Path $work 'units_core.exe'
        $null = Run 'XLang core build' $xl @('build',
            'test/cases/extlib_units.xl', '-o', $aot)
        $null = Run 'XLang core AOT' $aot @()
        $exampleAot = Join-Path $work 'units_example.exe'
        $null = Run 'XLang example AOT build' $xl @('build',
            'release/examples/extlibs/units.xl', '-o', $exampleAot)
        $aotOutput = Run 'XLang example AOT' $exampleAot @()
        if ($aotOutput -cne $cOutput) {
            throw 'XLang AOT example output differs from C'
        }
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
