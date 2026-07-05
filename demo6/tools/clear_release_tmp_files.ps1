param(
    [string]$Root = "",
    [int]$Retries = 60,
    [int]$DelayMs = 500
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$tmpDir = Join-Path $Root "build\test"
if ( ! (Test-Path -LiteralPath $tmpDir -PathType Container) ) {
    Write-Host "release tmp cleanup ok: directory absent."
    exit 0
}

$failures = New-Object System.Collections.Generic.List[string]
$files = @(Get-ChildItem -LiteralPath $tmpDir -File -Force)

foreach ( $file in $files ) {
    $removed = $false
    for ( $attempt = 1; $attempt -le $Retries; $attempt++ ) {
        if ( ! (Test-Path -LiteralPath $file.FullName -PathType Leaf) ) {
            $removed = $true
            break
        }

        try {
            Remove-Item -LiteralPath $file.FullName -Force -ErrorAction Stop
            $removed = $true
            break
        } catch {
            if ( $attempt -ge $Retries ) {
                $failures.Add(("$($file.FullName): " + $_.Exception.Message)) | Out-Null
            } else {
                Start-Sleep -Milliseconds $DelayMs
            }
        }
    }

    if ( ! $removed ) {
        $failures.Add("$($file.FullName): not removed") | Out-Null
    }
}

if ( $failures.Count -gt 0 ) {
    foreach ( $failure in $failures ) {
        Write-Host "release tmp cleanup failed: $failure"
    }
    exit 1
}

Write-Host "release tmp cleanup ok: $($files.Count) files scanned."
