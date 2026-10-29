param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new($false)
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$testRoot = [IO.Path]::GetFullPath((Join-Path $root 'build/test'))
$work = Join-Path $testRoot ('linalg_guard_' + [guid]::NewGuid().ToString('N'))
$xl = Join-Path $root 'release/xl.exe'
$runner = Join-Path $root 'tools/run_extlib_c.ps1'
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$manifest = Get-Content -LiteralPath (Join-Path $root 'release/extlibs/linalg/xlang.package.json') -Raw | ConvertFrom-Json
$moduleFlags = @($manifest.artifact.xrtModules | ForEach-Object { "-D$_" })
$expected = [string]::Join("`n", (Get-Content -LiteralPath (Join-Path $PSScriptRoot 'linalg.expected.txt') -Encoding UTF8))

function Confirm-Output([string] $name, [string[]] $lines) {
    $actual = [string]::Join("`n", @($lines | ForEach-Object { $_ -replace "`r$", '' }))
    if ($actual -cne $expected) { throw "$name differs from fixed linalg example" }
}

Push-Location $root
try {
    & $gcc -std=c11 -Wall -Wextra -Werror -fsyntax-only '-Ilib/xrt' `
        '-Irelease/extlibs/linalg/include' @moduleFlags release/extlibs/linalg/linalg_native.c
    if ($LASTEXITCODE -ne 0) { throw 'linalg manifest macro compile failed' }

    & $runner -Package linalg -Source release/extlibs/linalg/test/test_linalg.c
    if ($LASTEXITCODE -ne 0) { throw 'linalg C boundaries failed' }
    foreach ($source in @(
        'test/cases/extlib_linalg.xl',
        'release/extlibs/linalg/test/boundaries.xl',
        'release/extlibs/linalg/test/allocations.xl'
    )) {
        & $xl run $source
        if ($LASTEXITCODE -ne 0) { throw "linalg XLang test failed: $source" }
    }

    $cLines = @(& $runner -Package linalg -Source release/examples/extlibs/linalg_c.c)
    if ($LASTEXITCODE -ne 0) { throw 'linalg C package example failed' }
    Confirm-Output 'C package example' $cLines
    $directLines = @(& $xl run release/examples/extlibs/linalg_c.c)
    if ($LASTEXITCODE -ne 0) { throw 'linalg direct C example failed' }
    Confirm-Output 'direct C example' $directLines
    $xlLines = @(& $xl run release/examples/extlibs/linalg.xl)
    if ($LASTEXITCODE -ne 0) { throw 'linalg XLang example failed' }
    Confirm-Output 'XLang example' $xlLines

    if (-not $SkipAot) {
        [IO.Directory]::CreateDirectory($work) | Out-Null
        $core = Join-Path $work 'linalg_core.exe'
        & $xl build release/extlibs/linalg/test/aot_core.xl -o $core
        if ($LASTEXITCODE -ne 0) { throw 'linalg core AOT build failed' }
        & $core
        if ($LASTEXITCODE -ne 0) { throw 'linalg core AOT run failed' }

        $example = Join-Path $work 'linalg_example.exe'
        & $xl build release/examples/extlibs/linalg.xl -o $example
        if ($LASTEXITCODE -ne 0) { throw 'linalg example AOT build failed' }
        $aotLines = @(& $example)
        if ($LASTEXITCODE -ne 0) { throw 'linalg example AOT run failed' }
        Confirm-Output 'XLang example AOT' $aotLines
        $cExample = Join-Path $work 'linalg_c_example.exe'
        & $xl build release/examples/extlibs/linalg_c.c -o $cExample
        if ($LASTEXITCODE -ne 0) { throw 'linalg C example AOT build failed' }
        $cAotLines = @(& $cExample)
        if ($LASTEXITCODE -ne 0) { throw 'linalg C example AOT run failed' }
        Confirm-Output 'C example AOT' $cAotLines
    }

    Write-Output $(if ($SkipAot) { 'linalg Windows C/XLang focused gate: PASS' }
        else { 'linalg Windows C/XLang and AOT gate: PASS' })
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
