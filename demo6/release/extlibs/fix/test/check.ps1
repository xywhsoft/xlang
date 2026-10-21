param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$testRoot = [IO.Path]::GetFullPath((Join-Path $root 'build/test'))
$work = Join-Path $testRoot ('fix_guard_' + [guid]::NewGuid().ToString('N'))
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop).Source
$xl = Join-Path $root 'release/xl.exe'
$flags = @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-Ilib/xrt',
    '-Irelease/extlibs/fix')
$links = @('-lWs2_32', '-lAdvapi32', '-lIphlpapi', '-lm')

function Run([string]$name, [string]$program, [string[]]$arguments) {
    $previousPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $output = & $program @arguments 2>&1
        $code = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previousPreference
    }
    if ($code -ne 0) {
        throw "${name}: exit $code`n$($output -join "`n")"
    }
    Write-Host "[fix] ${name}: PASS"
    return ($output -join "`n").Replace("`r`n", "`n").TrimEnd("`n")
}

Push-Location $root
try {
    [IO.Directory]::CreateDirectory($work) | Out-Null
    $null = Run 'manifest C syntax' $gcc ($flags + @(
        '-DXRT_MODULE_BUFFER', '-DXRT_MODULE_VALUE_CONTAINER',
        '-DXRT_MODULE_NUMBER_FLOAT', '-fsyntax-only',
        'release/extlibs/fix/fix_native.c'))
    $runtime = Join-Path $work 'xrt_runtime.o'
    $null = Run 'XRT compile' $gcc ($flags + @('-c',
        'release/extlibs/fix/test/xrt_runtime.c', '-o', $runtime))
    $native = 'release/extlibs/fix/fix_native.c'
    $test = Join-Path $work 'fix_test.exe'
    $null = Run 'C test compile' $gcc ($flags + @($native,
        'release/extlibs/fix/test/test_fix.c', $runtime) +
        $links + @('-o', $test))
    $null = Run 'C test' $test @()

    $example = Join-Path $work 'fix_example.exe'
    $null = Run 'C example compile' $gcc ($flags + @(
        'release/extlibs/fix/examples/fix_c.c', $runtime) +
        $links + @('-o', $example))
    $cOutput = Run 'C example' $example @()

    Push-Location (Join-Path $root 'release')
    try {
        $null = Run 'XLang existing test' $xl @('run', '../test/cases/extlib_fix.xl')
        $null = Run 'XLang boundaries' $xl @('run', 'extlibs/fix/test/boundaries.xl')
        $null = Run 'XLang allocation failures' $xl @('run', 'extlibs/fix/test/allocations.xl')
        $xlOutput = Run 'XLang example' $xl @('run', 'examples/extlibs/fix.xl')
        if ($cOutput -cne $xlOutput) {
            throw 'C/XLang FIX example outputs differ'
        }
        Write-Host '[fix] C/XLang parity: PASS'
        $python = Get-Command python -CommandType Application -ErrorAction Stop |
            Select-Object -First 1 -ExpandProperty Source
        $null = Run 'independent FIX frame' $python @(
            'extlibs/fix/test/check_frame.py', $cOutput)
        $runOutput = Run 'C example via xl run' $xl @('run', 'extlibs/fix/examples/fix_c.c')
        if ($runOutput -cne $cOutput) {
            throw 'xl run FIX C example differs from GCC'
        }

        if (-not $SkipAot) {
            $core = Join-Path $work 'fix_xl_core.exe'
            $null = Run 'XLang core build' $xl @('build',
                'extlibs/fix/test/aot_core.xl', '-o', $core)
            $null = Run 'XLang core AOT' $core @()
            $target = Join-Path $work 'fix_xl_test.exe'
            $null = Run 'XLang boundaries build' $xl @('build',
                'extlibs/fix/test/boundaries.xl', '-o', $target)
            $null = Run 'XLang boundaries AOT' $target @()
            $exampleAot = Join-Path $work 'fix_xl_example.exe'
            $null = Run 'XLang example build' $xl @('build',
                'examples/extlibs/fix.xl', '-o', $exampleAot)
            $aotOutput = Run 'XLang example AOT' $exampleAot @()
            if ($aotOutput -cne $cOutput) {
                throw 'XLang AOT FIX example differs from C'
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
