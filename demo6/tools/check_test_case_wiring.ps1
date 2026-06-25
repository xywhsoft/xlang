param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$caseDir = Join-Path $Root "test\cases"
$wiringFiles = @(
    "test.bat",
    "test_tutorial.bat",
    "examples\feature_suite\run_feature_suite.bat"
)

$failures = New-Object System.Collections.Generic.List[string]

function Add-Failure($message)
{
    $script:failures.Add($message) | Out-Null
}

function Read-Utf8Text($path)
{
    return [System.IO.File]::ReadAllText($path, [System.Text.Encoding]::UTF8)
}

if ( ! (Test-Path -LiteralPath $caseDir -PathType Container) ) {
    Write-Host "test cases directory missing: $caseDir"
    exit 1
}

$textParts = New-Object System.Collections.Generic.List[string]
foreach ( $relative in $wiringFiles ) {
    $path = Join-Path $Root $relative
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        Add-Failure "wiring file missing: $relative"
        continue
    }
    $textParts.Add((Read-Utf8Text $path)) | Out-Null
}

$wiringText = [string]::Join("`n", $textParts)
$cases = @(Get-ChildItem -LiteralPath $caseDir -Filter "*.xl" -File | Sort-Object Name)

foreach ( $case in $cases ) {
    if ( $wiringText -notmatch [regex]::Escape($case.Name) ) {
        Add-Failure "test case is not wired: test/cases/$($case.Name)"
    }
}

$referenced = New-Object System.Collections.Generic.HashSet[string]
$matches = [regex]::Matches($wiringText, 'test[\\/]+cases[\\/]+([^"''\s>]+\.xl)')
foreach ( $match in $matches ) {
    $name = [System.IO.Path]::GetFileName($match.Groups[1].Value)
    if ( $name.Length -gt 0 ) {
        $referenced.Add($name) | Out-Null
    }
}

foreach ( $name in ($referenced | Sort-Object) ) {
    $path = Join-Path $caseDir $name
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        Add-Failure "wired test case file is missing: test/cases/$name"
    }
}

if ( $failures.Count -gt 0 ) {
    foreach ( $failure in $failures ) {
        Write-Host $failure
    }
    exit 1
}

Write-Host "Test case wiring ok: $($cases.Count) files."
exit 0
