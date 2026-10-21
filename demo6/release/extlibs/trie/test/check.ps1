$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new($false)
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$xl = Join-Path $root 'release/xl.exe'
$runner = Join-Path $root 'tools/run_extlib_c.ps1'
$expected = [string]::Join("`n", (Get-Content -LiteralPath (Join-Path $PSScriptRoot 'trie.expected.txt') -Encoding UTF8))
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop).Source
$manifest = Get-Content -LiteralPath (Join-Path $root 'release/extlibs/trie/xlang.package.json') -Raw | ConvertFrom-Json
$moduleFlags = @($manifest.artifact.xrtModules | ForEach-Object { "-D$_" })

function Confirm-Output([string] $name, [string[]] $lines) {
    $actual = [string]::Join("`n", @($lines | ForEach-Object { $_ -replace "`r$", '' }))
    if ($actual -cne $expected) { throw "$name output differs from fixed trie example" }
}

Push-Location $root
try {
    & $gcc -std=c11 -Wall -Wextra -Werror -fsyntax-only -Ilib/xrt `
        -Irelease/extlibs/trie/include @moduleFlags release/extlibs/trie/trie_native.c
    if ($LASTEXITCODE -ne 0) { throw 'trie manifest macro compile failed' }
    & $gcc -std=c11 -Wall -Wextra -Werror -fsyntax-only -Ilib/xrt `
        release/extlibs/trie/test/abi_contract.c
    if ($LASTEXITCODE -ne 0) { throw 'trie C ABI contract failed' }

    & $runner -Package trie -Source release/extlibs/trie/test/test_trie.c
    if ($LASTEXITCODE -ne 0) { throw 'trie C model and boundaries failed' }
    foreach ($source in @(
        'test/cases/extlib_trie.xl',
        'release/extlibs/trie/test/boundaries.xl',
        'release/extlibs/trie/test/allocations.xl'
    )) {
        & $xl run $source
        if ($LASTEXITCODE -ne 0) { throw "trie XLang test failed: $source" }
    }

    $xlLines = @(& $xl run release/examples/extlibs/trie.xl)
    if ($LASTEXITCODE -ne 0) { throw 'trie XLang example failed' }
    Confirm-Output 'XLang example' $xlLines
    $cLines = @(& $runner -Package trie -Source release/examples/extlibs/trie_c.c)
    if ($LASTEXITCODE -ne 0) { throw 'trie C package example failed' }
    Confirm-Output 'C package example' $cLines
    $direct = @(& $xl run release/examples/extlibs/trie_c.c)
    if ($LASTEXITCODE -ne 0) { throw 'trie direct C example failed' }
    Confirm-Output 'direct C example' $direct

    Write-Output 'trie Windows C/XLang gate: PASS'
} finally {
    Pop-Location
}
