param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$testDir = Join-Path $Root "test\cases"
$testBat = Join-Path $Root "test.bat"
$minStdlibTestFiles = 21

$batText = [System.IO.File]::ReadAllText($testBat, [System.Text.Encoding]::UTF8)
$missing = New-Object System.Collections.Generic.List[string]
$tests = @(Get-ChildItem -Path $testDir -Filter "stdlib_*.xl" -File | Sort-Object Name)

if ( $tests.Count -lt $minStdlibTestFiles ) {
    $missing.Add("stdlib test baseline regressed: $($tests.Count) < $minStdlibTestFiles")
}

foreach ( $test in $tests ) {
    $relative = "test\cases\$($test.Name)"
    $slashRelative = $relative.Replace("\", "/")

    if ( ($batText -notmatch [regex]::Escape($relative)) -and ($batText -notmatch [regex]::Escape($slashRelative)) ) {
        $missing.Add($relative)
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
