param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$testDir = Join-Path $Root "test\cases"
$wiringGuard = Join-Path $Root "tools\check_test_case_wiring.ps1"
$minStdlibTestFiles = 21

$missing = New-Object System.Collections.Generic.List[string]
$tests = @(Get-ChildItem -Path $testDir -Filter "stdlib_*.xl" -File | Sort-Object Name)

if ( $tests.Count -lt $minStdlibTestFiles ) {
    $missing.Add("stdlib test baseline regressed: $($tests.Count) < $minStdlibTestFiles")
}

if ( ! (Test-Path -LiteralPath $wiringGuard -PathType Leaf) ) {
    $missing.Add("global test case wiring guard is missing")
} else {
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $wiringGuard -Root $Root
    if ( $LASTEXITCODE -ne 0 ) {
        $missing.Add("global test case wiring guard failed")
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
