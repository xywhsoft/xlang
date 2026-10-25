param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$testRoot = [IO.Path]::GetFullPath((Join-Path $root 'build/test'))
$work = Join-Path $testRoot ('progress_guard_' + [guid]::NewGuid().ToString('N'))
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop).Source
$xl = Join-Path $root 'release/xl.exe'
$flags = @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
    '-DXRT_MODULE_MEMORY_DEBUG',
    '-Ilib/xrt', '-Irelease/extlibs/progress/include')
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
    Write-Host "[progress] ${name}: PASS"
    return ($output -join "`n").Replace("`r`n", "`n").TrimEnd("`n")
}

Push-Location $root
try {
    [IO.Directory]::CreateDirectory($work) | Out-Null
    $null = Run 'ABI contract' $gcc ($flags + @('-fsyntax-only',
        'release/extlibs/progress/test/abi_contract.c'))
    $runtime = Join-Path $work 'xrt_runtime_debug.o'
    $native = Join-Path $work 'progress_native.o'
    $null = Run 'XRT compile' $gcc ($flags + @('-c',
        'release/extlibs/progress/test/xrt_runtime.c', '-o', $runtime))
    $null = Run 'native compile' $gcc ($flags + @('-c',
        'release/extlibs/progress/progress_native.c', '-o', $native))
    $test = Join-Path $work 'test_progress.exe'
    $null = Run 'C boundaries compile' $gcc ($flags + @(
        'release/extlibs/progress/test/test_progress.c', $native, $runtime) +
        $links + @('-o', $test))
    $null = Run 'C boundaries' $test @()
    $example = Join-Path $work 'example_progress.exe'
    $null = Run 'C example compile' $gcc ($flags + @(
        'release/examples/extlibs/progress_c.c', $runtime) +
        $links + @('-o', $example))
    $cOutput = Run 'C example' $example @()
    $directC = Run 'C example xl run' $xl @('run',
        'release/examples/extlibs/progress_c.c')
    if ($directC -cne $cOutput) {
        throw 'C example xl run differs from GCC'
    }

    $null = Run 'XLang central boundaries' $xl @('run', 'test/cases/extlib_progress.xl')
    $null = Run 'XLang package boundaries' $xl @('run',
        'release/extlibs/progress/test/boundaries.xl')
    $null = Run 'XLang allocation failures' $xl @('run',
        'release/extlibs/progress/test/allocation_oom.xl')
    $previousPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $negative = & $xl check 'release/extlibs/progress/test/private_native_negative.xl' 2>&1
    } finally {
        $ErrorActionPreference = $previousPreference
    }
    if ($LASTEXITCODE -eq 0 -or
        ($negative -join "`n") -notmatch 'setPrefixNative' -or
        ($negative -join "`n") -notmatch 'access') {
        throw "private native entry should be rejected:`n$($negative -join "`n")"
    }
    Write-Host '[progress] private native negative: PASS'
    $xlOutput = Run 'XLang example' $xl @('run',
        'release/examples/extlibs/progress.xl')
    $expected = [IO.File]::ReadAllText((Join-Path $PSScriptRoot 'progress.expected.txt')).
        Replace("`r`n", "`n").TrimEnd("`n")
    if ($cOutput -cne $expected -or $xlOutput -cne $expected) {
        throw "C/XLang progress examples differ from the fixed expected output"
    }
    Write-Host '[progress] C/XLang fixed output: PASS'

    if (-not $SkipAot) {
        $aot = Join-Path $work 'progress_boundaries.exe'
        $null = Run 'XLang boundaries build' $xl @('build',
            'release/extlibs/progress/test/boundaries.xl', '-o', $aot)
        $null = Run 'XLang boundaries AOT' $aot @()
        $allocationAot = Join-Path $work 'progress_allocation_oom.exe'
        $null = Run 'XLang allocation failures build' $xl @('build',
            'release/extlibs/progress/test/allocation_oom.xl', '-o', $allocationAot)
        $null = Run 'XLang allocation failures AOT' $allocationAot @()
        $exampleAot = Join-Path $work 'progress_example.exe'
        $null = Run 'XLang example build' $xl @('build',
            'release/examples/extlibs/progress.xl', '-o', $exampleAot)
        $aotOutput = Run 'XLang example AOT' $exampleAot @()
        if ($aotOutput -cne $expected) {
            throw "AOT progress example differs from the fixed expected output"
        }
        Write-Host '[progress] AOT fixed output: PASS'
        $cExampleAot = Join-Path $work 'progress_c_example.exe'
        $null = Run 'C example AOT build' $xl @('build',
            'release/examples/extlibs/progress_c.c', '-o', $cExampleAot)
        if ((Run 'C example AOT' $cExampleAot @()) -cne $expected) {
            throw 'C example AOT differs from the fixed expected output'
        }
    }
    Write-Host $(if ($SkipAot) { '[progress] Windows C/XLang quick gate: PASS (AOT skipped)' }
        else { '[progress] Windows C/XLang/AOT gate: PASS' })
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
