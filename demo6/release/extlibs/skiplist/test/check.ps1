$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new($false)
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$xl = Join-Path $root 'release/xl.exe'
$runner = Join-Path $root 'tools/run_extlib_c.ps1'
$expected = [string]::Join("`n", (Get-Content -LiteralPath (Join-Path $PSScriptRoot 'skiplist.expected.txt') -Encoding UTF8))
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop).Source
$manifest = Get-Content -LiteralPath (Join-Path $root 'release/extlibs/skiplist/xlang.package.json') -Raw | ConvertFrom-Json
$moduleFlags = @($manifest.artifact.xrtModules | ForEach-Object { "-D$_" })

function Confirm-Output([string] $name, [string[]] $lines) {
    $actual = [string]::Join("`n", @($lines | ForEach-Object { $_ -replace "`r$", '' }))
    if ($actual -cne $expected) { throw "$name output differs from fixed skiplist example" }
}

Push-Location $root
try {
    & $gcc -std=c11 -Wall -Wextra -Werror -fsyntax-only -Ilib/xrt `
        -Irelease/extlibs/skiplist/include @moduleFlags `
        release/extlibs/skiplist/skiplist_native.c
    if ($LASTEXITCODE -ne 0) { throw 'skiplist manifest macro compile failed' }
    & $gcc -std=c11 -Wall -Wextra -Werror -fsyntax-only -Ilib/xrt `
        release/extlibs/skiplist/test/abi_contract.c
    if ($LASTEXITCODE -ne 0) { throw 'skiplist C ABI contract failed' }

    & $runner -Package skiplist -Source release/extlibs/skiplist/test/test_skiplist.c
    if ($LASTEXITCODE -ne 0) { throw 'skiplist C model and boundaries failed' }
    foreach ($source in @(
        'test/cases/extlib_skiplist.xl',
        'release/extlibs/skiplist/test/allocations.xl'
    )) {
        & $xl run $source
        if ($LASTEXITCODE -ne 0) { throw "skiplist XLang test failed: $source" }
    }

    $xlLines = @(& $xl run release/examples/extlibs/skiplist.xl)
    if ($LASTEXITCODE -ne 0) { throw 'skiplist XLang example failed' }
    Confirm-Output 'XLang example' $xlLines
    $cLines = @(& $runner -Package skiplist -Source release/examples/extlibs/skiplist_c.c)
    if ($LASTEXITCODE -ne 0) { throw 'skiplist C package example failed' }
    Confirm-Output 'C package example' $cLines
    $direct = @(& $xl run release/examples/extlibs/skiplist_c.c)
    if ($LASTEXITCODE -ne 0) { throw 'skiplist direct C example failed' }
    Confirm-Output 'direct C example' $direct

    Write-Output 'skiplist Windows C/XLang gate: PASS'
} finally {
    Pop-Location
}
