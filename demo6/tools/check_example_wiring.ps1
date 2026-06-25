param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$exampleDir = Join-Path $Root "examples"
$wiringFiles = @(
    "test.bat",
    "test_all.bat",
    "test_tutorial.bat",
    "test_feature_suite.bat",
    "tools\check_stdlib_examples.ps1",
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

function Test-WildcardCovered($relative)
{
    if ( $relative -like "examples/feature_suite/*.xl" ) {
        return $true
    }
    if ( $relative -like "examples/tutorial/*.xl" ) {
        return $true
    }
    if ( $relative -like "examples/stdlib_*.xl" ) {
        return $true
    }
    return $false
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

if ( ! (Test-Path -LiteralPath $exampleDir -PathType Container) ) {
    Write-Host "examples directory missing: $exampleDir"
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
$examples = @(Get-ChildItem -LiteralPath $exampleDir -Filter "*.xl" -File -Recurse | Sort-Object FullName)
$rootPath = (Resolve-Path -LiteralPath $Root).Path

foreach ( $example in $examples ) {
    $relative = $example.FullName.Substring($rootPath.Length + 1).Replace("\", "/")

    if ( Test-WildcardCovered $relative ) {
        continue
    }

    if ( ! (Test-DirectReference $wiringText $relative) ) {
        Add-Failure "example is not wired: $relative"
    }
}

$referenced = New-Object System.Collections.Generic.HashSet[string]
$matches = [regex]::Matches($wiringText, 'examples[\\/]+([^"''\s>]+\.xl)')
foreach ( $match in $matches ) {
    $relative = ("examples/" + $match.Groups[1].Value).Replace("\", "/")
    if ( $relative.Contains("%") -or $relative.Contains("*") ) {
        continue
    }
    $referenced.Add($relative) | Out-Null
}

foreach ( $relative in ($referenced | Sort-Object) ) {
    $path = Join-Path $Root ($relative.Replace("/", "\"))
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        Add-Failure "wired example file is missing: $relative"
    }
}

if ( $failures.Count -gt 0 ) {
    foreach ( $failure in $failures ) {
        Write-Host $failure
    }
    exit 1
}

Write-Host "Example wiring ok: $($examples.Count) files."
exit 0
