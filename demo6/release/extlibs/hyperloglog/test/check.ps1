$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new($false)
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$xl = Join-Path $root 'release/xl.exe'
$runner = Join-Path $root 'tools/run_extlib_c.ps1'
$expected = [string]::Join("`n", (Get-Content -LiteralPath (Join-Path $PSScriptRoot 'hyperloglog.expected.txt') -Encoding UTF8))
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop).Source
$manifest = Get-Content -LiteralPath (Join-Path $root 'release/extlibs/hyperloglog/xlang.package.json') -Raw | ConvertFrom-Json
$moduleFlags = @($manifest.artifact.xrtModules | ForEach-Object { "-D$_" })

function Confirm-Output([string] $name, [string[]] $lines) {
    $actual = [string]::Join("`n", @($lines | ForEach-Object { $_ -replace "`r$", '' }))
    if ($actual -cne $expected) { throw "$name output differs from fixed HLL example" }
}

Push-Location $root
try {
    & $gcc -std=c11 -Wall -Wextra -Werror -fsyntax-only -Ilib/xrt `
        -Irelease/extlibs/hyperloglog/include @moduleFlags `
        release/extlibs/hyperloglog/hyperloglog_native.c
    if ($LASTEXITCODE -ne 0) { throw 'hyperloglog manifest macro compile failed' }
    & $gcc -std=c11 -Wall -Wextra -Werror -fsyntax-only -Ilib/xrt `
        release/extlibs/hyperloglog/test/abi_contract.c
    if ($LASTEXITCODE -ne 0) { throw 'hyperloglog C ABI contract failed' }

    foreach ($source in @('test_hyperloglog.c', 'test_threads.c')) {
        & $runner -Package hyperloglog -Source "release/extlibs/hyperloglog/test/$source"
        if ($LASTEXITCODE -ne 0) { throw "hyperloglog C test failed: $source" }
    }
    foreach ($source in @(
        'test/cases/extlib_hyperloglog.xl',
        'release/extlibs/hyperloglog/test/boundaries.xl',
        'release/extlibs/hyperloglog/test/named_type.xl'
    )) {
        & $xl run $source
        if ($LASTEXITCODE -ne 0) { throw "hyperloglog XLang test failed: $source" }
    }

    $xlLines = @(& $xl run release/examples/extlibs/hyperloglog.xl)
    if ($LASTEXITCODE -ne 0) { throw 'hyperloglog XLang example failed' }
    Confirm-Output 'XLang example' $xlLines
    $cLines = @(& $runner -Package hyperloglog `
        -Source release/extlibs/hyperloglog/examples/hyperloglog_c.c)
    if ($LASTEXITCODE -ne 0) { throw 'hyperloglog C package example failed' }
    Confirm-Output 'C package example' $cLines
    $direct = @(& $xl run release/extlibs/hyperloglog/examples/hyperloglog_c.c)
    if ($LASTEXITCODE -ne 0) { throw 'hyperloglog direct C example failed' }
    Confirm-Output 'direct C example' $direct

    Write-Output 'hyperloglog Windows C/XLang gate: PASS'
} finally {
    Pop-Location
}
