$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new($false)
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$xl = Join-Path $root 'release/xl.exe'
$runner = Join-Path $root 'tools/run_extlib_c.ps1'
$expected = [string]::Join("`n", (Get-Content -LiteralPath (Join-Path $PSScriptRoot 'rtree.expected.txt') -Encoding UTF8))
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop).Source
$manifest = Get-Content -LiteralPath (Join-Path $root 'release/extlibs/rtree/xlang.package.json') -Raw | ConvertFrom-Json
$moduleFlags = @($manifest.artifact.xrtModules | ForEach-Object { "-D$_" })

function Confirm-Output([string] $name, [string[]] $lines) {
    $actual = [string]::Join("`n", @($lines | ForEach-Object { $_ -replace "`r$", '' }))
    if ($actual -cne $expected) { throw "$name output differs from fixed R-tree example" }
}

Push-Location $root
try {
    & $gcc -std=c11 -Wall -Wextra -Werror -fsyntax-only -Ilib/xrt `
        -Irelease/extlibs/rtree/include @moduleFlags release/extlibs/rtree/rtree_native.c
    if ($LASTEXITCODE -ne 0) { throw 'rtree manifest macro compile failed' }
    & $gcc -std=c11 -Wall -Wextra -Werror -fsyntax-only -Ilib/xrt `
        release/extlibs/rtree/test/abi_contract.c
    if ($LASTEXITCODE -ne 0) { throw 'rtree C ABI contract failed' }

    & $runner -Package rtree -Source release/extlibs/rtree/test/test_rtree.c
    if ($LASTEXITCODE -ne 0) { throw 'rtree C oracle and boundaries failed' }
    foreach ($source in @(
        'test/cases/extlib_rtree.xl',
        'release/extlibs/rtree/test/boundaries.xl',
        'release/extlibs/rtree/test/allocations.xl'
    )) {
        & $xl run $source
        if ($LASTEXITCODE -ne 0) { throw "rtree XLang test failed: $source" }
    }

    $xlLines = @(& $xl run release/examples/extlibs/rtree.xl)
    if ($LASTEXITCODE -ne 0) { throw 'rtree XLang example failed' }
    Confirm-Output 'XLang example' $xlLines
    $cLines = @(& $runner -Package rtree -Source release/examples/extlibs/rtree_c.c)
    if ($LASTEXITCODE -ne 0) { throw 'rtree C package example failed' }
    Confirm-Output 'C package example' $cLines
    $direct = @(& $xl run release/examples/extlibs/rtree_c.c)
    if ($LASTEXITCODE -ne 0) { throw 'rtree direct C example failed' }
    Confirm-Output 'direct C example' $direct

    Write-Output 'rtree Windows C/XLang gate: PASS'
} finally {
    Pop-Location
}
