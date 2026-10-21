param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$xl = Join-Path $root 'release/xl.exe'
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop).Source
$runner = Join-Path $root 'tools/run_extlib_c.ps1'
$aot = Join-Path $root ("build/test/fuzzy_core_$([guid]::NewGuid().ToString('N')).exe")

function Run([string]$label, [string]$program, [string[]]$arguments) {
    $output = @(& $program @arguments 2>&1 | ForEach-Object { $_.ToString() })
    if ($LASTEXITCODE -ne 0) {
        throw "${label}: exit $LASTEXITCODE`n$($output -join "`n")"
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
    $null = Run 'XLang central' $xl @('run', 'test/cases/extlib_fuzzy.xl')
    $xlOutput = Run 'XLang example' $xl @('run',
        'release/examples/extlibs/fuzzy.xl')
    if ($cOutput -cne $directOutput -or $cOutput -cne $xlOutput) {
        throw '[fuzzy] C runner, single-file C, and XLang example output differs'
    }
    Write-Host '[fuzzy] C/XLang example parity: PASS'
    if (-not $SkipAot) {
        [IO.Directory]::CreateDirectory((Split-Path -Parent $aot)) | Out-Null
        $null = Run 'core AOT build' $xl @('build',
            'test/cases/extlib_fuzzy.xl', '-o', $aot)
        $null = Run 'core AOT' $aot @()
    }
} finally {
    Pop-Location
    if (Test-Path -LiteralPath $aot -PathType Leaf) {
        Remove-Item -LiteralPath $aot -Force
    }
}
