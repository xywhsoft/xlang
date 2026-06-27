param(
    [string]$Root = "",
    [int]$MinTextArtifacts = 0
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$tmpDir = Join-Path $Root "release\tmp"
$failures = New-Object System.Collections.Generic.List[string]
$strictUtf8 = New-Object System.Text.UTF8Encoding($false, $true)
$textExtensions = New-Object System.Collections.Generic.HashSet[string] ([System.StringComparer]::OrdinalIgnoreCase)
$files = @()
foreach ( $ext in @(".txt", ".json", ".xlir", ".c", ".h", ".def", ".log") ) {
    $textExtensions.Add($ext) | Out-Null
}

function Add-Failure($message)
{
    $script:failures.Add($message) | Out-Null
}

if ( Test-Path -LiteralPath $tmpDir -PathType Container ) {
    $files = @(
        Get-ChildItem -LiteralPath $tmpDir -File |
            Where-Object { $textExtensions.Contains($_.Extension) } |
            Sort-Object FullName
    )
} elseif ( $MinTextArtifacts -gt 0 ) {
    Add-Failure "release tmp directory missing: $tmpDir"
}

if ( $files.Count -lt $MinTextArtifacts ) {
    Add-Failure "release tmp text artifact baseline regressed: $($files.Count) < $MinTextArtifacts"
}

foreach ( $file in $files ) {
    try {
        $bytes = [System.IO.File]::ReadAllBytes($file.FullName)
        $text = $strictUtf8.GetString($bytes)
    } catch {
        Add-Failure "release tmp text artifact is not strict UTF-8: $($file.Name)"
        continue
    }

    if ( $text.IndexOf([char]0xFFFD) -ge 0 ) {
        Add-Failure "release tmp text artifact contains UTF-8 replacement character: $($file.Name)"
    }

    if ( $text -match "CategoryInfo|FullyQualifiedErrorId|NativeCommandError" ) {
        Add-Failure "release tmp text artifact contains PowerShell native error wrapper: $($file.Name)"
    }

    if ( $text.Contains("[JsonErr]") ) {
        Add-Failure "release tmp text artifact contains XRT JSON parser debug output: $($file.Name)"
    }

    if ( $text.Contains("code generation error") ) {
        Add-Failure "release tmp text artifact contains generic cgen diagnostic: $($file.Name)"
    }

    if ( $file.Extension.Equals(".c", [System.StringComparison]::OrdinalIgnoreCase) -and
         $text.Contains("({") ) {
        Add-Failure "release tmp generated C contains GNU statement expression: $($file.Name)"
    }
}

if ( $failures.Count -gt 0 ) {
    foreach ( $failure in $failures ) {
        Write-Host $failure
    }
    exit 1
}

Write-Host "Release tmp artifacts ok: $($files.Count) text files."
exit 0
