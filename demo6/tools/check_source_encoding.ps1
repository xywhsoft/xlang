param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$scanExtensions = @(
    ".c",
    ".h",
    ".l",
    ".y",
    ".xl",
    ".md",
    ".ps1",
    ".bat",
    ".json"
)
$skipDirectoryNames = @(
    "release"
)
$minScannedTextFiles = 842
$failures = New-Object System.Collections.Generic.List[string]
$strictUtf8 = New-Object System.Text.UTF8Encoding($false, $true)
$looseUtf8 = [System.Text.Encoding]::UTF8

function Join-CodePoints($codes)
{
    $chars = New-Object System.Collections.Generic.List[char]
    foreach ( $code in $codes ) {
        $chars.Add([char]$code) | Out-Null
    }
    return -join $chars
}

$badTokens = @(
    [string][char]0xfffd,
    (Join-CodePoints @(0x951f, 0x65a4, 0x62f7))
)

function Add-Failure($message)
{
    $script:failures.Add($message) | Out-Null
}

function Should-SkipFile($file)
{
    $relative = $file.FullName.Substring($Root.Length).TrimStart("\", "/")
    foreach ( $name in $skipDirectoryNames ) {
        if ( $relative -eq $name -or
             $relative.StartsWith($name + "\", [System.StringComparison]::OrdinalIgnoreCase) -or
             $relative.StartsWith($name + "/", [System.StringComparison]::OrdinalIgnoreCase) ) {
            return $true
        }
    }
    return $false
}

if ( ! (Test-Path -LiteralPath $Root -PathType Container) ) {
    Write-Host "root directory missing: $Root"
    exit 1
}

$files = @(
    Get-ChildItem -LiteralPath $Root -Recurse -File | Where-Object {
        ($scanExtensions -contains $_.Extension) -and
        ! (Should-SkipFile $_)
    } | Sort-Object FullName
)

foreach ( $file in $files ) {
    $bytes = [System.IO.File]::ReadAllBytes($file.FullName)
    $text = $null

    try {
        $text = $strictUtf8.GetString($bytes)
    } catch {
        Add-Failure "invalid UTF-8: $($file.FullName)"
        $text = $looseUtf8.GetString($bytes)
    }

    $lines = $text -split "`r?`n"
    for ( $i = 0; $i -lt $lines.Count; ++$i ) {
        foreach ( $token in $badTokens ) {
            if ( $lines[$i].Contains($token) ) {
                Add-Failure ("mojibake token in {0}:{1}: {2}" -f $file.FullName, ($i + 1), $lines[$i].Trim())
                break
            }
        }
    }
}

if ( $files.Count -lt $minScannedTextFiles ) {
    Add-Failure "source encoding scan baseline regressed: $($files.Count) < $minScannedTextFiles"
}

if ( $failures.Count -gt 0 ) {
    foreach ( $failure in $failures ) {
        Write-Host $failure
    }
    exit 1
}

Write-Host "Source encoding ok: $($files.Count) text files."
exit 0
