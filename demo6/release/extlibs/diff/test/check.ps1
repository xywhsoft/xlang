param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new($false)
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$xl = Join-Path $root 'release/xl.exe'
$runner = Join-Path $root 'tools/run_extlib_c.ps1'
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$manifest = Get-Content -LiteralPath (Join-Path $root 'release/extlibs/diff/xlang.package.json') -Raw | ConvertFrom-Json
$moduleFlags = @($manifest.artifact.xrtModules | ForEach-Object { "-D$_" })
$expected = [string]::Join("`n", (Get-Content -LiteralPath (Join-Path $PSScriptRoot 'diff.expected.txt') -Encoding UTF8))
$testRoot = [IO.Path]::GetFullPath((Join-Path $root 'build/test'))
$work = [IO.Path]::GetFullPath((Join-Path $testRoot ('diff_guard_' + [guid]::NewGuid().ToString('N'))))

function Confirm-Output([string] $name, [string[]] $lines) {
    $actual = [string]::Join("`n", @($lines | ForEach-Object { $_ -replace "`r$", '' }))
    if ($actual -cne $expected) { throw "$name differs from fixed diff example" }
}

Push-Location $root
try {
    & $gcc -std=c11 -Wall -Wextra -Werror -fsyntax-only '-Ilib/xrt' `
        '-Irelease/extlibs/diff/include' @moduleFlags release/extlibs/diff/diff_native.c
    if ($LASTEXITCODE -ne 0) { throw 'diff manifest macro compile failed' }

    foreach ($source in @(
        'release/extlibs/diff/test/test_diff.c',
        'release/extlibs/diff/test/test_unterminated.c',
        'release/extlibs/diff/test/test_threads.c'
    )) {
        & $runner -Package diff -Source $source
        if ($LASTEXITCODE -ne 0) { throw "diff C test failed: $source" }
    }
    foreach ($source in @('test/cases/extlib_diff.xl', 'release/extlibs/diff/test/boundaries.xl')) {
        & $xl run $source
        if ($LASTEXITCODE -ne 0) { throw "diff XLang test failed: $source" }
    }

    $cLines = @(& $runner -Package diff -Source release/extlibs/diff/examples/diff_c.c)
    if ($LASTEXITCODE -ne 0) { throw 'diff C package example failed' }
    Confirm-Output 'C package example' $cLines
    $directLines = @(& $xl run release/extlibs/diff/examples/diff_c.c)
    if ($LASTEXITCODE -ne 0) { throw 'diff direct C example failed' }
    Confirm-Output 'direct C example' $directLines
    $xlLines = @(& $xl run release/examples/extlibs/diff.xl)
    if ($LASTEXITCODE -ne 0) { throw 'diff XLang example failed' }
    Confirm-Output 'XLang example' $xlLines

    if (-not $SkipAot) {
        [IO.Directory]::CreateDirectory($work) | Out-Null
        $example = Join-Path $work 'diff_example.exe'
        & $xl build release/examples/extlibs/diff.xl -o $example
        if ($LASTEXITCODE -ne 0) { throw 'diff XLang example AOT build failed' }
        $aotLines = @(& $example)
        if ($LASTEXITCODE -ne 0) { throw 'diff XLang example AOT failed' }
        Confirm-Output 'XLang example AOT' $aotLines

        $boundary = Join-Path $work 'diff_boundaries.exe'
        & $xl build release/extlibs/diff/test/boundaries.xl -o $boundary
        if ($LASTEXITCODE -ne 0) { throw 'diff XLang boundaries AOT build failed' }
        $boundaryLines = @(& $boundary)
        if ($LASTEXITCODE -ne 0) { throw 'diff XLang boundaries AOT failed' }
        if ([string]::Join("`n", @($boundaryLines | ForEach-Object { $_ -replace "`r$", '' })) -cne
            'diff boundaries: PASS') {
            throw 'diff XLang boundaries AOT output changed'
        }
    }

    if ($SkipAot) {
        Write-Output 'diff Windows C/XLang focused gate: PASS'
    } else {
        Write-Output 'diff Windows C/XLang AOT gate: PASS'
    }
} finally {
    Pop-Location
    if (-not $work.StartsWith($testRoot + [IO.Path]::DirectorySeparatorChar,
        [StringComparison]::OrdinalIgnoreCase)) {
        throw "Unsafe diff cleanup path: $work"
    }
    if (Test-Path -LiteralPath $work) {
        Remove-Item -LiteralPath $work -Recurse -Force
    }
}
