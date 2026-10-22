param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$xl = Join-Path $root 'release/xl.exe'
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop).Source
$runner = Join-Path $root 'tools/run_extlib_c.ps1'
$workRoot = Join-Path $root 'build/test'
$work = Join-Path $workRoot ("fuzzy_guard_$([guid]::NewGuid().ToString('N'))")

function Run([string]$label, [string]$program, [string[]]$arguments) {
    $previous = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $output = @(& $program @arguments 2>&1 | ForEach-Object { $_.ToString() })
        $code = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previous
    }
    if ($code -ne 0) {
        throw "${label}: exit $code`n$($output -join "`n")"
    }
    Write-Host "[fuzzy] ${label}: PASS"
    return ($output -join "`n").Replace("`r", '').TrimEnd("`n")
}

Push-Location $root
try {
    $null = Run 'strict C syntax' $gcc @(
        '-std=c11', '-Wall', '-Wextra', '-Werror', '-fsyntax-only',
        '-Ilib/xrt', '-Irelease/extlibs/fuzzy',
        'release/extlibs/fuzzy/fuzzy_native.c',
        'release/extlibs/fuzzy/test/abi_contract.c',
        'release/extlibs/fuzzy/test/test_fuzzy.c',
        'release/extlibs/fuzzy/test/test_unterminated.c',
        'release/extlibs/fuzzy/test/test_threads.c',
        'release/extlibs/fuzzy/examples/fuzzy_c.c')
    foreach ($test in @('test_fuzzy', 'test_unterminated', 'test_threads')) {
        $null = Run $test $runner @('fuzzy', "release/extlibs/fuzzy/test/$test.c")
    }
    $cOutput = Run 'C example' $runner @('fuzzy',
        'release/extlibs/fuzzy/examples/fuzzy_c.c')
    $directOutput = Run 'single-file C example' $xl @('run',
        'release/extlibs/fuzzy/examples/fuzzy_c.c')
    $null = Run 'XLang boundaries' $xl @('run',
        'release/extlibs/fuzzy/test/boundaries.xl')
    $null = Run 'XLang allocations' $xl @('run',
        'release/extlibs/fuzzy/test/allocations.xl')
    $null = Run 'XLang central' $xl @('run', 'test/cases/extlib_fuzzy.xl')
    $xlOutput = Run 'XLang example' $xl @('run',
        'release/examples/extlibs/fuzzy.xl')
    if ($cOutput -cne $directOutput -or $cOutput -cne $xlOutput) {
        throw '[fuzzy] C runner, single-file C, and XLang example output differs'
    }
    Write-Host '[fuzzy] C/XLang example parity: PASS'
    if (-not $SkipAot) {
        [IO.Directory]::CreateDirectory($work) | Out-Null
        $aot = Join-Path $work 'core.exe'
        $null = Run 'core AOT build' $xl @('build',
            'test/cases/extlib_fuzzy.xl', '-o', $aot)
        if ((Run 'core AOT' $aot @()) -cne '') {
            throw '[fuzzy] core AOT produced unexpected output'
        }
        $example = Join-Path $work 'example.exe'
        $null = Run 'example AOT build' $xl @('build',
            'release/examples/extlibs/fuzzy.xl', '-o', $example)
        if ((Run 'example AOT' $example @()) -cne $cOutput) {
            throw '[fuzzy] example AOT and C output differs'
        }
        Write-Host '[fuzzy] AOT/C example parity: PASS'
    }
} finally {
    Pop-Location
    $resolvedRoot = [IO.Path]::GetFullPath($workRoot).TrimEnd('\') + '\'
    $resolvedWork = [IO.Path]::GetFullPath($work)
    if (-not $resolvedWork.StartsWith($resolvedRoot, [StringComparison]::OrdinalIgnoreCase) -or
        -not [IO.Path]::GetFileName($resolvedWork).StartsWith('fuzzy_guard_')) {
        throw "Fuzzy test directory escapes build/test: $resolvedWork"
    }
    if (Test-Path -LiteralPath $resolvedWork -PathType Container) {
        Remove-Item -LiteralPath $resolvedWork -Recurse -Force
    }
}
