param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ($Root -eq "") {
    $Root = Split-Path -Parent $PSScriptRoot
}

$Root = (Resolve-Path -LiteralPath $Root).Path
$ManifestPath = Join-Path $Root "test\featured_extlibs.json"
$Xl = Join-Path $Root "release\xl.exe"
$CaseDir = Join-Path $Root "test\cases"
$ExampleDir = Join-Path $Root "release\examples\extlibs"

if (!(Test-Path -LiteralPath $Xl -PathType Leaf)) {
    throw "xl.exe not found: $Xl"
}

$manifest = Get-Content -LiteralPath $ManifestPath -Raw -Encoding UTF8 | ConvertFrom-Json
$seenCases = @{}

function Invoke-XlangCase([string]$caseName) {
    if ($seenCases.ContainsKey($caseName)) {
        return
    }
    $seenCases[$caseName] = $true
    $path = Join-Path $CaseDir $caseName
    if (!(Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Featured extension test is missing: $path"
    }
    Write-Host "[featured:case] $caseName"
    & $Xl run $path
    if ($LASTEXITCODE -ne 0) {
        throw "Featured extension test failed: $caseName (exit $LASTEXITCODE)"
    }
}

function Invoke-CSource([string]$packageName, [string]$sourceName, [string]$kind) {
    $source = Join-Path $Root $sourceName
    if (!(Test-Path -LiteralPath $source -PathType Leaf)) {
        throw "Featured extension $kind source is missing: $source"
    }
    Write-Host "[featured:$kind] $packageName"
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root "tools\run_extlib_c.ps1") -Package $packageName -Source $source
    if ($LASTEXITCODE -ne 0) {
        throw "Featured extension $kind failed: $packageName (exit $LASTEXITCODE)"
    }
}

foreach ($package in @($manifest.packages)) {
    if (!(Test-Path -LiteralPath (Join-Path $Root "release\extlibs\$($package.name)\xlang.package.json") -PathType Leaf)) {
        throw "Featured extension package manifest is missing: $($package.name)"
    }
    Invoke-XlangCase ([string]$package.case)
}

foreach ($caseName in @($manifest.sharedCases)) {
    Invoke-XlangCase ([string]$caseName)
}

foreach ($package in @($manifest.packages)) {
    $example = Join-Path $ExampleDir ([string]$package.example)
    if (!(Test-Path -LiteralPath $example -PathType Leaf)) {
        throw "Featured extension example is missing: $example"
    }
    Write-Host "[featured:example] $($package.example)"
    & $Xl run $example
    if ($LASTEXITCODE -ne 0) {
        throw "Featured extension example failed: $($package.example) (exit $LASTEXITCODE)"
    }
}

foreach ($package in @($manifest.packages)) {
    if ($null -eq $package.cSmoke -or [string]$package.cSmoke -eq "") {
        continue
    }
    Invoke-CSource ([string]$package.name) ([string]$package.cSmoke) "C"
}

foreach ($package in @($manifest.packages)) {
    foreach ($cExample in @($package.cExamples)) {
        if ($null -ne $cExample -and [string]$cExample -ne "") {
            Invoke-CSource ([string]$package.name) ([string]$cExample) "C-example"
        }
    }
}

Write-Host "Featured extension RC suite passed: $($manifest.packages.Count) packages."
