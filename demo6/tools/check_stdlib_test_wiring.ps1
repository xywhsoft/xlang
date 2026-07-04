param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$testDir = Join-Path $Root "test\cases"
$testBat = Join-Path $Root "test.bat"
$testSuiteDir = Join-Path $Root "test\suites"
$minStdlibTestFiles = 21

function Read-TestRegressionText()
{
    $parts = New-Object System.Collections.Generic.List[string]
    $parts.Add([System.IO.File]::ReadAllText($script:testBat, [System.Text.Encoding]::UTF8)) | Out-Null

    if ( Test-Path -LiteralPath $script:testSuiteDir -PathType Container ) {
        $suiteFiles = @(Get-ChildItem -LiteralPath $script:testSuiteDir -File -Filter "*.bat" | Sort-Object Name)
        foreach ( $suite in $suiteFiles ) {
            $parts.Add([System.IO.File]::ReadAllText($suite.FullName, [System.Text.Encoding]::UTF8)) | Out-Null
        }
    }

    return [string]::Join("`n", $parts)
}

$batText = Read-TestRegressionText
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
