param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ($Root -eq "") {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$manifestPath = Join-Path $Root "test\phase1_extlibs.json"
$caseRoot = Join-Path $Root "test\cases"
$exampleRoot = Join-Path $Root "release\examples\extlibs"
$exampleRunner = Join-Path $PSScriptRoot "run_xl_example.ps1"

if (!(Test-Path -LiteralPath $xl -PathType Leaf)) { throw "xl.exe is missing: $xl" }
if (!(Test-Path -LiteralPath $manifestPath -PathType Leaf)) { throw "phase-one manifest is missing: $manifestPath" }
if (!(Test-Path -LiteralPath $exampleRunner -PathType Leaf)) { throw "phase-one example runner is missing: $exampleRunner" }

$manifest = Get-Content -LiteralPath $manifestPath -Raw -Encoding UTF8 | ConvertFrom-Json
$count = 0
foreach ($pkg in @($manifest.packages)) {
    $casePath = Join-Path $caseRoot ([string]$pkg.case)
    $examplePath = Join-Path $exampleRoot ([string]$pkg.example)
    foreach ($path in @($casePath, $examplePath)) {
        if (!(Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing phase-one artifact for $($pkg.name): $path" }
    }

    Write-Host "[phase1] test $($pkg.name)"
    & $xl run $casePath
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    Write-Host "[phase1] example $($pkg.name)"
    # 每个范例在独立进程中运行，隔离前序用例和 C smoke 的进程状态。
    & powershell -NoProfile -ExecutionPolicy Bypass -File $exampleRunner -Xl $xl -Script $examplePath -WorkingDirectory "$Root\release"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    if ($null -ne $pkg.cSmoke -and [string]$pkg.cSmoke -ne "") {
        Write-Host "[phase1] C smoke $($pkg.name)"
        $cPackage = if ($null -ne $pkg.cPackage -and [string]$pkg.cPackage -ne "") { [string]$pkg.cPackage } else { [string]$pkg.name }
        & (Join-Path $PSScriptRoot "run_extlib_c.ps1") -Package $cPackage -Source ([string]$pkg.cSmoke)
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    }
    $count++
}

Write-Host "Phase-one extension library suite passed: $count packages."
