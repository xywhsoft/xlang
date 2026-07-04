param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$caseDir = Join-Path $Root "test\cases"
$testSuiteDir = Join-Path $Root "test\suites"
$minJsonFixtures = 5
$wiringFiles = @(
    "test.bat",
    "test_all.bat",
    "test_tutorial.bat",
    "test_dll.bat",
    "release\examples\feature_suite\run_feature_suite.bat"
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

function Get-TestSuiteWiringFiles()
{
    $files = New-Object System.Collections.Generic.List[string]

    if ( Test-Path -LiteralPath $script:testSuiteDir -PathType Container ) {
        $rootPath = (Resolve-Path -LiteralPath $script:Root).Path
        $suiteFiles = @(Get-ChildItem -LiteralPath $script:testSuiteDir -File -Filter "*.bat" | Sort-Object Name)
        foreach ( $suite in $suiteFiles ) {
            $files.Add($suite.FullName.Substring($rootPath.Length + 1)) | Out-Null
        }
    }

    return @($files)
}

function Normalize-PathText($path)
{
    return $path.Replace("\", "/")
}

function Test-DirectReference($text, $relative)
{
    $slashPath = $relative.Replace("\", "/")
    $backslashPath = $relative.Replace("/", "\")

    if ( $text -match [regex]::Escape($slashPath) ) {
        return $true
    }
    if ( $text -match [regex]::Escape($backslashPath) ) {
        return $true
    }
    return $false
}

if ( ! (Test-Path -LiteralPath $caseDir -PathType Container) ) {
    Write-Host "test cases directory missing: $caseDir"
    exit 1
}

$wiringFiles += Get-TestSuiteWiringFiles

$textParts = New-Object System.Collections.Generic.List[string]
foreach ( $relative in $wiringFiles ) {
    $path = Join-Path $Root $relative
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        Add-Failure "wiring file missing: $relative"
        continue
    }
    $textParts.Add((Read-Utf8Text $path)) | Out-Null
}

$caseSources = @(Get-ChildItem -LiteralPath $caseDir -Filter "*.xl" -File -Recurse | Sort-Object FullName)
foreach ( $source in $caseSources ) {
    $textParts.Add((Read-Utf8Text $source.FullName)) | Out-Null
}

$allText = [string]::Join("`n", $textParts)
$rootPath = (Resolve-Path -LiteralPath $Root).Path
$dataFiles = @(Get-ChildItem -LiteralPath $caseDir -File -Recurse | Where-Object { $_.Name.EndsWith(".json") } | Sort-Object FullName)

if ( $dataFiles.Count -lt $minJsonFixtures ) {
    Add-Failure "test data fixture baseline regressed: $($dataFiles.Count) < $minJsonFixtures"
}

foreach ( $data in $dataFiles ) {
    $relative = Normalize-PathText ($data.FullName.Substring($rootPath.Length + 1))
    $caseRelative = Normalize-PathText ($data.FullName.Substring($caseDir.Length + 1))

    try {
        $null = Read-Utf8Text $data.FullName | ConvertFrom-Json
    } catch {
        Add-Failure "test data JSON is invalid: $relative"
        continue
    }

    if ( Test-DirectReference $allText $relative ) {
        continue
    }

    if ( $caseRelative.EndsWith(".xapi.json") ) {
        $importBase = $caseRelative.Substring(0, $caseRelative.Length - ".xapi.json".Length)
        if ( $allText -match ('import\s+"{0}"' -f [regex]::Escape($importBase)) ) {
            continue
        }
    }

    Add-Failure "test data fixture is not wired: $relative"
}

$referenced = New-Object System.Collections.Generic.HashSet[string]
$matches = [regex]::Matches($allText, 'test[\\/]+cases[\\/]+([^"''\s>]+\.(?:xapi\.)?json)')
foreach ( $match in $matches ) {
    $relative = Normalize-PathText ("test/cases/" + $match.Groups[1].Value)
    if ( $relative.Contains("%") -or $relative.Contains("*") ) {
        continue
    }
    $referenced.Add($relative) | Out-Null
}

foreach ( $relative in ($referenced | Sort-Object) ) {
    $path = Join-Path $Root ($relative.Replace("/", "\"))
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        Add-Failure "wired test data file is missing: $relative"
    }
}

if ( $failures.Count -gt 0 ) {
    foreach ( $failure in $failures ) {
        Write-Host $failure
    }
    exit 1
}

Write-Host "Test data wiring ok: $($dataFiles.Count) JSON fixtures."
exit 0
