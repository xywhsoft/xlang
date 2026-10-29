param(
    [string]$Root = "",
    [string]$Package = ""
)

$ErrorActionPreference = "Stop"

if ($Root -eq "") {
    $Root = Split-Path -Parent $PSScriptRoot
}

$rootPath = (Resolve-Path -LiteralPath $Root).Path
$xl = Join-Path $rootPath "release\xl.exe"
$manifestPath = Join-Path $rootPath "test\platform_extlibs.json"
$exampleRoot = Join-Path $rootPath "release\examples\extlibs"

if (!(Test-Path -LiteralPath $xl -PathType Leaf)) { throw "xl.exe is missing: $xl" }
$manifest = Get-Content -LiteralPath $manifestPath -Raw -Encoding UTF8 | ConvertFrom-Json

$selected = @($manifest.packages | Where-Object { $Package -eq "" -or $_.name -eq $Package })
if ($Package -ne "" -and $selected.Count -eq 0) { throw "Unknown platform package: $Package" }
foreach ($item in $selected) {
    if ($item.platform -eq "windows-desktop" -and $env:OS -ne "Windows_NT") {
        Write-Host "[platform] skip $($item.name): Windows desktop required"
        continue
    }
    $examplePath = Join-Path $exampleRoot ([string]$item.example)
    if (!(Test-Path -LiteralPath $examplePath -PathType Leaf)) { throw "Platform example is missing: $examplePath" }
    Write-Host "[platform] $($item.name): $($item.notes)"
    if ($item.case) {
        $casePath = Join-Path $rootPath ("test\cases\" + [string]$item.case)
        if (!(Test-Path -LiteralPath $casePath -PathType Leaf)) { throw "Platform case is missing: $casePath" }
        & $xl run $casePath
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    }
    $escapedRoot = (Join-Path $rootPath "release").Replace("'", "''")
    $escapedXl = $xl.Replace("'", "''")
    $escapedExample = $examplePath.Replace("'", "''")
    & powershell -NoProfile -ExecutionPolicy Bypass -Command "Set-Location -LiteralPath '$escapedRoot'; & '$escapedXl' run '$escapedExample'; exit `$LASTEXITCODE"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    if ($null -ne $item.cSmoke -and [string]$item.cSmoke -ne "") {
        $cSmoke = [string]$item.cSmoke
        Write-Host "[platform] C smoke $($item.name)"
        & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $rootPath "tools\run_extlib_c.ps1") -Package $item.name -Source $cSmoke
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    }
}

Write-Host "Platform extension suite passed."
