$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new($false)
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$xl = Join-Path $root 'release/xl.exe'
$xlExample = Join-Path $root 'release/examples/extlibs/money.xl'
$cExample = Join-Path $root 'release/extlibs/money/examples/money.c'
$expected = Get-Content -LiteralPath (Join-Path $root 'release/extlibs/money/examples/money.expected.txt') -Encoding UTF8
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop).Source
$manifest = Get-Content -LiteralPath (Join-Path $root 'release/extlibs/money/xlang.package.json') -Raw | ConvertFrom-Json
$moduleFlags = @($manifest.artifact.xrtModules | ForEach-Object { "-D$_" })

Push-Location $root
try {
    & $gcc -std=c11 -Wall -Wextra -Werror -fsyntax-only -Ilib/xrt `
        -Irelease/extlibs/money/include @moduleFlags `
        release/extlibs/money/money_native.c
    if ($LASTEXITCODE -ne 0) { throw 'money manifest macro compile failed' }
} finally {
    Pop-Location
}

$xlLines = @(& $xl run $xlExample)
if ($LASTEXITCODE -ne 0) { throw 'money XLang example failed' }
$cLines = @(& (Join-Path $root 'tools/run_extlib_c.ps1') -Package money -Source $cExample)
if ($LASTEXITCODE -ne 0) { throw 'money C example failed' }

$expectedText = [string]::Join("`n", $expected)
$xlText = [string]::Join("`n", @($xlLines | ForEach-Object { $_ -replace "`r$", '' }))
$cText = [string]::Join("`n", @($cLines | ForEach-Object { $_ -replace "`r$", '' }))
if ($xlText -cne $expectedText -or $cText -cne $expectedText) {
    throw "money C/XLang output mismatch`nXLang:`n$xlText`nC:`n$cText"
}
Write-Output 'money C/XLang examples: PASS'
