param(
    [string]$Root = "",
    [string]$Pattern = "extlib_*.xl",
    [string]$StartAt = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$caseDir = Join-Path $Root "test\cases"

if ( ! (Test-Path -LiteralPath $xl -PathType Leaf) ) {
    Write-Host "xl executable missing: $xl"
    exit 1
}

# Wires: test/cases/extlib_*.xl
$cases = @(Get-ChildItem -LiteralPath $caseDir -File -Filter $Pattern | Sort-Object Name)
if ( $cases.Count -eq 0 ) {
    Write-Host "no extension-library tests matched: $Pattern"
    exit 1
}

$started = ($StartAt -eq "")
$executed = 0
foreach ( $case in $cases ) {
    if ( ! $started ) {
        if ( $case.Name -ne $StartAt ) {
            continue
        }
        $started = $true
    }
    Write-Host "Running extension-library test: $($case.Name)"
    & $xl run $case.FullName
    if ( $LASTEXITCODE -ne 0 ) {
        Write-Host "extension-library test failed: $($case.Name)"
        exit $LASTEXITCODE
    }
    $executed++
}
if ( ! $started ) {
    Write-Host "extension-library start test not found: $StartAt"
    exit 1
}

$packageTests = @(
    Get-ChildItem -LiteralPath (Join-Path $Root "release\extlibs") -File -Recurse -Filter "*.xl" |
        Where-Object {
            $_.FullName.IndexOf("\test\", [System.StringComparison]::OrdinalIgnoreCase) -ge 0
        } |
        Sort-Object FullName
)
foreach ( $case in $packageTests ) {
    Write-Host "Checking package-local extension test: $($case.FullName)"
    & $xl check $case.FullName
    if ( $LASTEXITCODE -ne 0 ) {
        Write-Host "package-local extension test failed: $($case.FullName)"
        exit $LASTEXITCODE
    }
}

Write-Host "Extension-library tests ok: $executed runtime files, $($packageTests.Count) package-local check files."
exit 0
