$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new($false)
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$xl = Join-Path $root 'release/xl.exe'
$runner = Join-Path $root 'tools/run_extlib_c.ps1'
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$manifest = Get-Content -LiteralPath (Join-Path $root 'release/extlibs/tabulate/xlang.package.json') -Raw | ConvertFrom-Json
$moduleFlags = @($manifest.artifact.xrtModules | ForEach-Object { "-D$_" })
$expected = [string]::Join("`n", (Get-Content -LiteralPath (Join-Path $PSScriptRoot 'tabulate.expected.txt') -Encoding UTF8))

function Confirm-Output([string] $name, [string[]] $lines) {
    $actual = [string]::Join("`n", @($lines | ForEach-Object { $_ -replace "`r$", '' }))
    if ($actual -cne $expected) { throw "$name differs from fixed tabulate example" }
}

Push-Location $root
try {
    & $gcc -std=c11 -Wall -Wextra -Werror -fsyntax-only '-Ilib/xrt' `
        '-Irelease/extlibs/tabulate/include' @moduleFlags `
        release/extlibs/tabulate/tabulate_native.c release/extlibs/tabulate/tabulate_render.c
    if ($LASTEXITCODE -ne 0) { throw 'tabulate manifest macro compile failed' }
    & $gcc -std=c11 -Wall -Wextra -Werror -fsyntax-only '-Ilib/xrt' `
        '-Irelease/extlibs/tabulate/include' release/extlibs/tabulate/test/abi_contract.c
    if ($LASTEXITCODE -ne 0) { throw 'tabulate C ABI contract failed' }

    foreach ($source in @('test_tabulate.c', 'test_threads.c')) {
        & $runner -Package tabulate -Source "release/extlibs/tabulate/test/$source"
        if ($LASTEXITCODE -ne 0) { throw "tabulate C test failed: $source" }
    }
    foreach ($source in @(
        'test/cases/extlib_tabulate.xl',
        'release/extlibs/tabulate/test/boundaries.xl',
        'release/extlibs/tabulate/test/allocations.xl'
    )) {
        & $xl run $source
        if ($LASTEXITCODE -ne 0) { throw "tabulate XLang test failed: $source" }
    }

    $previousPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $diagnostic = @(& $xl check release/extlibs/tabulate/test/private_native_negative.xl 2>&1)
        $negativeExit = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previousPreference
    }
    $message = [string]::Join("`n", $diagnostic)
    if ($negativeExit -eq 0 -or $message -notmatch 'appendCellNative' -or $message -notmatch 'access') {
        throw "tabulate native member access was not rejected: $message"
    }

    $cLines = @(& $runner -Package tabulate -Source release/extlibs/tabulate/examples/tabulate_c.c)
    if ($LASTEXITCODE -ne 0) { throw 'tabulate C package example failed' }
    Confirm-Output 'C package example' $cLines
    $directLines = @(& $xl run release/extlibs/tabulate/examples/tabulate_c.c)
    if ($LASTEXITCODE -ne 0) { throw 'tabulate direct C example failed' }
    Confirm-Output 'direct C example' $directLines
    $xlLines = @(& $xl run release/examples/extlibs/tabulate.xl)
    if ($LASTEXITCODE -ne 0) { throw 'tabulate XLang example failed' }
    Confirm-Output 'XLang example' $xlLines

    Write-Output 'tabulate Windows C/XLang gate: PASS'
} finally {
    Pop-Location
}
