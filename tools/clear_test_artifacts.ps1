param(
    [string]$Target = "build\test",
    [int]$Retries = 60,
    [int]$DelayMs = 500
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$trimChars = [char[]]@('\', '/')

function Get-FullPath([string]$Path)
{
    if ( [System.IO.Path]::IsPathRooted($Path) ) {
        return [System.IO.Path]::GetFullPath($Path)
    }

    return [System.IO.Path]::GetFullPath((Join-Path $repoRoot $Path))
}

$targetFull = Get-FullPath $Target
$allowedFull = Get-FullPath "build\test"
$targetNorm = $targetFull.TrimEnd($trimChars)
$allowedNorm = $allowedFull.TrimEnd($trimChars)

if ( ! [string]::Equals($targetNorm, $allowedNorm, [System.StringComparison]::OrdinalIgnoreCase) ) {
    Write-Host "test artifact cleanup refused: $targetFull is not $allowedFull"
    exit 1
}

if ( ! (Test-Path -LiteralPath $targetFull -PathType Container) ) {
    New-Item -ItemType Directory -Path $targetFull -Force | Out-Null
    Write-Host "test artifact cleanup ok: directory created."
    exit 0
}

$failures = New-Object System.Collections.Generic.List[string]
$items = @(Get-ChildItem -LiteralPath $targetFull -Force)

foreach ( $item in $items ) {
    $removed = $false

    for ( $attempt = 1; $attempt -le $Retries; $attempt++ ) {
        if ( ! (Test-Path -LiteralPath $item.FullName) ) {
            $removed = $true
            break
        }

        try {
            Remove-Item -LiteralPath $item.FullName -Recurse -Force -ErrorAction Stop
            $removed = $true
            break
        } catch {
            if ( $attempt -ge $Retries ) {
                $failures.Add(("$($item.FullName): " + $_.Exception.Message)) | Out-Null
            } else {
                Start-Sleep -Milliseconds $DelayMs
            }
        }
    }

    if ( ! $removed ) {
        $failures.Add("$($item.FullName): not removed") | Out-Null
    }
}

if ( $failures.Count -gt 0 ) {
    foreach ( $failure in $failures ) {
        Write-Host "test artifact cleanup failed: $failure"
    }
    exit 1
}

Write-Host "test artifact cleanup ok: $($items.Count) entries removed."
exit 0
