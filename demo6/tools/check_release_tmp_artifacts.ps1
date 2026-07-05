param(
    [string]$Root = "",
    [int]$MinTextArtifacts = 0
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$artifactDir = Join-Path $Root "build\test"
$failures = New-Object System.Collections.Generic.List[string]
$strictUtf8 = New-Object System.Text.UTF8Encoding($false, $true)
$textExtensions = New-Object System.Collections.Generic.HashSet[string] ([System.StringComparer]::OrdinalIgnoreCase)
$files = @()
foreach ( $ext in @(".txt", ".json", ".xlir", ".c", ".h", ".def", ".log", ".out", ".ast") ) {
    $textExtensions.Add($ext) | Out-Null
}

function Add-Failure($message)
{
    $script:failures.Add($message) | Out-Null
}

if ( Test-Path -LiteralPath $artifactDir -PathType Container ) {
    $files = @(
        Get-ChildItem -LiteralPath $artifactDir -Recurse -File |
            Where-Object { $textExtensions.Contains($_.Extension) } |
            Sort-Object FullName
    )
} elseif ( $MinTextArtifacts -gt 0 ) {
    Add-Failure "test artifact directory missing: $artifactDir"
}

if ( $files.Count -lt $MinTextArtifacts ) {
    Add-Failure "test text artifact baseline regressed: $($files.Count) < $MinTextArtifacts"
}

foreach ( $file in $files ) {
    try {
        $bytes = [System.IO.File]::ReadAllBytes($file.FullName)
        $text = $strictUtf8.GetString($bytes)
    } catch {
        Add-Failure "test text artifact is not strict UTF-8: $($file.FullName)"
        continue
    }

    if ( $text.IndexOf([char]0xFFFD) -ge 0 ) {
        Add-Failure "test text artifact contains UTF-8 replacement character: $($file.FullName)"
    }

    if ( $text -match "CategoryInfo|FullyQualifiedErrorId|NativeCommandError" ) {
        Add-Failure "test text artifact contains PowerShell native error wrapper: $($file.FullName)"
    }

    if ( $text.Contains("[JsonErr]") ) {
        Add-Failure "test text artifact contains XRT JSON parser debug output: $($file.FullName)"
    }

    if ( $text.Contains("code generation error") ) {
        Add-Failure "test text artifact contains generic cgen diagnostic: $($file.FullName)"
    }

    if ( $file.Extension.Equals(".c", [System.StringComparison]::OrdinalIgnoreCase) -and
         $text.Contains("({") ) {
        Add-Failure "test generated C contains GNU statement expression: $($file.FullName)"
    }
}

if ( $failures.Count -gt 0 ) {
    foreach ( $failure in $failures ) {
        Write-Host $failure
    }
    exit 1
}

Write-Host "Test artifacts ok: $($files.Count) text files."
exit 0
