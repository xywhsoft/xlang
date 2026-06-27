param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$toolsDir = Join-Path $Root "tools"
$toolsReadme = Join-Path $toolsDir "README.md"
$testBat = Join-Path $Root "test.bat"
$rc0Guard = Join-Path $toolsDir "rc0_guard.ps1"
$minCheckScripts = 25
$failures = New-Object System.Collections.Generic.List[string]

function Add-Failure($message)
{
    $script:failures.Add($message) | Out-Null
}

function Read-Utf8Text($path)
{
    return [System.IO.File]::ReadAllText($path, [System.Text.Encoding]::UTF8)
}

function Normalize-ToolPath($path)
{
    return $path.Replace("/", "\").ToLowerInvariant()
}

if ( ! (Test-Path -LiteralPath $toolsDir -PathType Container) ) {
    Write-Host "tools directory missing: $toolsDir"
    exit 1
}

if ( ! (Test-Path -LiteralPath $toolsReadme -PathType Leaf) ) {
    Write-Host "tools README missing: $toolsReadme"
    exit 1
}

if ( ! (Test-Path -LiteralPath $testBat -PathType Leaf) ) {
    Write-Host "test.bat missing: $testBat"
    exit 1
}

if ( ! (Test-Path -LiteralPath $rc0Guard -PathType Leaf) ) {
    Write-Host "rc0 guard missing: $rc0Guard"
    exit 1
}

$testText = Read-Utf8Text $testBat
$rc0Text = Read-Utf8Text $rc0Guard
$toolsReadmeText = Read-Utf8Text $toolsReadme
$checkScripts = @(Get-ChildItem -LiteralPath $toolsDir -Filter "check_*.ps1" -File | Sort-Object Name)
$testWiredScripts = New-Object System.Collections.Generic.HashSet[string]
$rc0WiredScripts = New-Object System.Collections.Generic.HashSet[string]
$fullRegressionOnly = New-Object System.Collections.Generic.HashSet[string]
$fullRegressionOnly.Add((Normalize-ToolPath "tools\check_stdlib_examples.ps1")) | Out-Null

if ( $checkScripts.Count -lt $minCheckScripts ) {
    Add-Failure "check script baseline regressed: $($checkScripts.Count) < $minCheckScripts"
}

foreach ( $match in [regex]::Matches($testText, 'tools[\\/](check_[^"''\s\^<>|&]+\.ps1)') ) {
    $relative = Normalize-ToolPath ("tools\" + $match.Groups[1].Value)
    $testWiredScripts.Add($relative) | Out-Null

    $path = Join-Path $Root $relative
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        Add-Failure "test.bat references missing check script: $relative"
    }
}

foreach ( $match in [regex]::Matches($rc0Text, 'check_[A-Za-z0-9_]+\.ps1') ) {
    $relative = Normalize-ToolPath ("tools\" + $match.Value)
    $rc0WiredScripts.Add($relative) | Out-Null

    $path = Join-Path $Root $relative
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        Add-Failure "rc0_guard.ps1 references missing check script: $relative"
    }
}

foreach ( $script in $checkScripts ) {
    $relative = Normalize-ToolPath ("tools\" + $script.Name)

    if ( ! $toolsReadmeText.Contains($script.Name) ) {
        Add-Failure "tools README does not list check script: $relative"
    }

    if ( ! $testWiredScripts.Contains($relative) ) {
        Add-Failure "check script is not wired into test.bat: $relative"
    }

    if ( ! $rc0WiredScripts.Contains($relative) -and ! $fullRegressionOnly.Contains($relative) ) {
        Add-Failure "check script is not wired into rc0_guard.ps1 or a full-regression-only exception: $relative"
    }
}

foreach ( $relative in $fullRegressionOnly ) {
    $path = Join-Path $Root $relative
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        Add-Failure "full-regression-only check script is missing: $relative"
    }
    if ( ! $testWiredScripts.Contains($relative) ) {
        Add-Failure "full-regression-only check script is not wired into test.bat: $relative"
    }
}

if ( ! $toolsReadmeText.Contains("xl6_toolchain_env.bat") ) {
    Add-Failure "tools README does not mention tools\xl6_toolchain_env.bat"
}

if ( ! $toolsReadmeText.Contains("rc0_guard.ps1") ) {
    Add-Failure "tools README does not mention tools\rc0_guard.ps1"
}

if ( $failures.Count -gt 0 ) {
    foreach ( $failure in $failures ) {
        Write-Host $failure
    }
    exit 1
}

Write-Host "Tool wiring ok: $($checkScripts.Count) check scripts."
exit 0
