param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$scanRoots = @(
    "examples",
    "test\cases"
)

$allowed = New-Object System.Collections.Generic.HashSet[string]
$allowed.Add("examples/stdlib_print.xl") | Out-Null
$allowed.Add("test/cases/stdlib_print.xl") | Out-Null

$failures = New-Object System.Collections.Generic.List[string]
$successPrintPattern = '(?<![A-Za-z0-9_])print\s*\(\s*["''][^"'']*\bok\b[^"'']*["'']'

function Add-Failure($message)
{
    $script:failures.Add($message) | Out-Null
}

function Get-RelativePath($path)
{
    $rootFull = [System.IO.Path]::GetFullPath($Root).TrimEnd("\", "/")
    $pathFull = [System.IO.Path]::GetFullPath($path)
    $prefix = $rootFull + [System.IO.Path]::DirectorySeparatorChar
    if ( $pathFull.StartsWith($prefix, [System.StringComparison]::OrdinalIgnoreCase) ) {
        $relative = $pathFull.Substring($prefix.Length)
    } else {
        $relative = $pathFull
    }
    return $relative.Replace("\", "/")
}

foreach ( $rootItem in $scanRoots ) {
    $dir = Join-Path $Root $rootItem
    if ( ! (Test-Path -LiteralPath $dir -PathType Container) ) {
        Add-Failure "missing xlang output marker scan root: $rootItem"
        continue
    }

    $files = @(Get-ChildItem -LiteralPath $dir -Recurse -File -Filter "*.xl" | Sort-Object FullName)
    foreach ( $file in $files ) {
        $relative = Get-RelativePath $file.FullName
        if ( $allowed.Contains($relative) ) {
            continue
        }

        $matches = @(Select-String -LiteralPath $file.FullName -Pattern $successPrintPattern -AllMatches)
        foreach ( $match in $matches ) {
            Add-Failure ("success marker should use println: {0}:{1}: {2}" -f $relative, $match.LineNumber, $match.Line.Trim())
        }
    }
}

if ( $failures.Count -gt 0 ) {
    foreach ( $failure in $failures ) {
        Write-Host $failure
    }
    exit 1
}

Write-Host "Xlang output markers ok."
exit 0
