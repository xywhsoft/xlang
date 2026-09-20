param(
    [string]$Root = "",
    [int]$Retries = 60,
    [int]$DelayMs = 500
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$tmpDir = Join-Path $Root "release\tmp"
if ( ! (Test-Path -LiteralPath $tmpDir -PathType Container) ) {
    Write-Host "release tmp cleanup ok: directory absent."
    exit 0
}

$failures = New-Object System.Collections.Generic.List[string]
$resolvedTmpDir = (Resolve-Path -LiteralPath $tmpDir).Path
$items = @(Get-ChildItem -LiteralPath $tmpDir -Force)

foreach ( $item in $items ) {
    $resolvedItem = $item.FullName
    if ( ! $resolvedItem.StartsWith($resolvedTmpDir + "\", [System.StringComparison]::OrdinalIgnoreCase) ) {
        $failures.Add("cleanup target is outside release tmp: $resolvedItem") | Out-Null
        continue
    }
    $removed = $false
    for ( $attempt = 1; $attempt -le $Retries; $attempt++ ) {
        if ( ! (Test-Path -LiteralPath $resolvedItem) ) {
            $removed = $true
            break
        }

        try {
            Remove-Item -LiteralPath $resolvedItem -Recurse -Force -ErrorAction Stop
            $removed = $true
            break
        } catch {
            if ( $attempt -ge $Retries ) {
                $failures.Add(("${resolvedItem}: " + $_.Exception.Message)) | Out-Null
            } else {
                Start-Sleep -Milliseconds $DelayMs
            }
        }
    }

    if ( ! $removed ) {
        $failures.Add("${resolvedItem}: not removed") | Out-Null
    }
}

if ( $failures.Count -gt 0 ) {
    foreach ( $failure in $failures ) {
        Write-Host "release tmp cleanup failed: $failure"
    }
    exit 1
}

Write-Host "release tmp cleanup ok: $($items.Count) entries scanned."
