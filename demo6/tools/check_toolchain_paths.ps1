param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$srcDir = Join-Path $Root "src"
$allowedSource = "src/backend/xl6_toolchain.h"
$allowedBatch = "tools/xl6_toolchain_env.bat"
$failures = New-Object System.Collections.Generic.List[string]

function Add-Failure($message)
{
    $script:failures.Add($message) | Out-Null
}

function Read-Utf8Text($path)
{
    return [System.IO.File]::ReadAllText($path, [System.Text.Encoding]::UTF8)
}

function Relative-From-Root($path)
{
    $rootPath = (Resolve-Path -LiteralPath $Root).Path
    return $path.Substring($rootPath.Length + 1).Replace("\", "/")
}

if ( ! (Test-Path -LiteralPath $srcDir -PathType Container) ) {
    Write-Host "src directory missing: $srcDir"
    exit 1
}

$sourceFiles = @(Get-ChildItem -LiteralPath $srcDir -Recurse -File -Include "*.c", "*.h" | Sort-Object FullName)

foreach ( $file in $sourceFiles ) {
    $relative = Relative-From-Root $file.FullName
    $text = Read-Utf8Text $file.FullName

    if ( $relative -eq $allowedSource ) {
        continue
    }

    if ( $text -match '(?i)demo5|[.][.][\\/]+demo5|[.][.][\\/]+[.][.][\\/]+xrt[\\/]release' ) {
        Add-Failure "source toolchain path/reference must stay in ${allowedSource}: ${relative}"
    }
}

$batchFiles = @(
    Get-ChildItem -LiteralPath $Root -File -Filter "*.bat"
    Get-ChildItem -LiteralPath (Join-Path $Root "tools") -File -Filter "*.bat"
) | Sort-Object FullName

foreach ( $file in $batchFiles ) {
    $relative = Relative-From-Root $file.FullName
    $text = Read-Utf8Text $file.FullName

    if ( $relative -eq $allowedBatch ) {
        continue
    }

    if ( $text -match '(?i)demo5|[.][.][\\/]+demo5|[.][.][\\/]+[.][.][\\/]+xrt[\\/]release' ) {
        Add-Failure "batch toolchain path/reference must stay in ${allowedBatch}: ${relative}"
    }
}

if ( $failures.Count -gt 0 ) {
    foreach ( $failure in $failures ) {
        Write-Host $failure
    }
    exit 1
}

Write-Host "Toolchain path guard ok: $($sourceFiles.Count) source headers/files, $($batchFiles.Count) batch files."
exit 0
