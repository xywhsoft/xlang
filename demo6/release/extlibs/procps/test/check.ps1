$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new($false)
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$xl = Join-Path $root 'release/xl.exe'
$runner = Join-Path $root 'tools/run_extlib_c.ps1'
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$manifest = Get-Content -LiteralPath (Join-Path $root 'release/extlibs/procps/xlang.package.json') -Raw | ConvertFrom-Json
$moduleFlags = @($manifest.artifact.xrtModules | ForEach-Object { "-D$_" })
$expected = [string]::Join("`n", (Get-Content -LiteralPath (Join-Path $PSScriptRoot 'procps.expected.txt') -Encoding UTF8))

function Confirm-Output([string] $name, [string[]] $lines) {
    $actual = [string]::Join("`n", @($lines | ForEach-Object { $_ -replace "`r$", '' }))
    if ($actual -cne $expected) { throw "$name differs from fixed procps example" }
}

Push-Location $root
try {
    & $gcc -std=c11 -Wall -Wextra -Werror -x c -fsyntax-only '-Ilib/xrt' `
        release/extlibs/procps/include/procps_native.h
    if ($LASTEXITCODE -ne 0) { throw 'procps public header compile failed' }
    & $gcc -std=c11 -Wall -Wextra -Werror -fsyntax-only '-Ilib/xrt' `
        '-Irelease/extlibs/procps/include' @moduleFlags release/extlibs/procps/procps_native.c
    if ($LASTEXITCODE -ne 0) { throw 'procps manifest macro compile failed' }

    & $runner -Package procps -Source release/extlibs/procps/test/test_procps.c
    if ($LASTEXITCODE -ne 0) { throw 'procps C boundaries failed' }
    foreach ($source in @('test/cases/extlib_procps.xl', 'release/extlibs/procps/test/boundaries.xl')) {
        & $xl run $source
        if ($LASTEXITCODE -ne 0) { throw "procps XLang test failed: $source" }
    }

    $cLines = @(& $runner -Package procps -Source release/extlibs/procps/examples/procps_c.c)
    if ($LASTEXITCODE -ne 0) { throw 'procps C package example failed' }
    Confirm-Output 'C package example' $cLines
    $directLines = @(& $xl run release/extlibs/procps/examples/procps_c.c)
    if ($LASTEXITCODE -ne 0) { throw 'procps direct C example failed' }
    Confirm-Output 'direct C example' $directLines
    $xlLines = @(& $xl run release/examples/extlibs/procps.xl)
    if ($LASTEXITCODE -ne 0) { throw 'procps XLang example failed' }
    Confirm-Output 'XLang example' $xlLines

    Write-Output 'procps Windows C/XLang gate: PASS'
} finally {
    Pop-Location
}
