param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$testDir = Join-Path $Root "test\cases"
$testBat = Join-Path $Root "test.bat"

$batText = [System.IO.File]::ReadAllText($testBat, [System.Text.Encoding]::UTF8)
$missing = New-Object System.Collections.Generic.List[string]
$tests = Get-ChildItem -Path $testDir -Filter "stdlib_*.xl" -File | Sort-Object Name

foreach ( $test in $tests ) {
    if ( $batText -notmatch [regex]::Escape($test.Name) ) {
        $missing.Add($test.Name)
    }
}

if ( $missing.Count -ne 0 ) {
    Write-Host "Standard library test wiring check failed."
    foreach ( $item in $missing ) {
        Write-Host "  $item"
    }
    exit 1
}

Write-Host "Standard library test wiring ok: $($tests.Count) files."
exit 0
