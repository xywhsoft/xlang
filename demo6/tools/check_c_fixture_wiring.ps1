param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$entryWiringFiles = @(
    "test.bat",
    "test_all.bat",
    "test_tutorial.bat",
    "test_dll.bat",
    "build_dll.bat",
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

$entryTextParts = New-Object System.Collections.Generic.List[string]
foreach ( $relative in $entryWiringFiles ) {
    $path = Join-Path $Root $relative
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        Add-Failure "wiring file missing: $relative"
        continue
    }
    $entryTextParts.Add((Read-Utf8Text $path)) | Out-Null
}

$entryText = [string]::Join("`n", $entryTextParts)
$rootPath = (Resolve-Path -LiteralPath $Root).Path
$entryAssetRoots = @("test\cases", "examples")

foreach ( $assetRoot in $entryAssetRoots ) {
    $fullRoot = Join-Path $Root $assetRoot
    if ( ! (Test-Path -LiteralPath $fullRoot -PathType Container) ) {
        Add-Failure "asset directory missing: $assetRoot"
        continue
    }

    $assets = @(Get-ChildItem -LiteralPath $fullRoot -File -Recurse | Where-Object { $_.Extension -in ".c", ".h" } | Sort-Object FullName)
    foreach ( $asset in $assets ) {
        $relative = $asset.FullName.Substring($rootPath.Length + 1).Replace("\", "/")
        if ( ! (Test-DirectReference $entryText $relative) ) {
            Add-Failure "C fixture is not wired: $relative"
        }
    }
}

$referenced = New-Object System.Collections.Generic.HashSet[string]
$matches = [regex]::Matches($entryText, '(test[\\/]+cases|examples)[\\/]+([^"''\s>]+\.(?:c|h))')
foreach ( $match in $matches ) {
    $relative = ($match.Groups[1].Value + "/" + $match.Groups[2].Value).Replace("\", "/")
    if ( $relative.Contains("%") -or $relative.Contains("*") ) {
        continue
    }
    $referenced.Add($relative) | Out-Null
}

foreach ( $relative in ($referenced | Sort-Object) ) {
    $path = Join-Path $Root ($relative.Replace("/", "\"))
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        Add-Failure "wired C fixture file is missing: $relative"
    }
}

$packageDir = Join-Path $Root "packages"
if ( Test-Path -LiteralPath $packageDir -PathType Container ) {
    $jsonFiles = @(Get-ChildItem -LiteralPath $packageDir -File -Recurse -Filter "*.json" | Sort-Object FullName)
    $packageTextParts = New-Object System.Collections.Generic.List[string]

    foreach ( $json in $jsonFiles ) {
        $text = Read-Utf8Text $json.FullName
        $packageTextParts.Add($text) | Out-Null

        $sourceMatches = [regex]::Matches($text, '"cSources"\s*:\s*\[(?<items>[^\]]*)\]')
        foreach ( $sourceMatch in $sourceMatches ) {
            $items = $sourceMatch.Groups["items"].Value
            $itemMatches = [regex]::Matches($items, '"([^"]+\.(?:c|h))"')
            foreach ( $itemMatch in $itemMatches ) {
                $source = $itemMatch.Groups[1].Value
                $sourcePath = Join-Path $json.DirectoryName $source
                if ( ! (Test-Path -LiteralPath $sourcePath -PathType Leaf) ) {
                    $relJson = $json.FullName.Substring($rootPath.Length + 1).Replace("\", "/")
                    Add-Failure "package cSources missing file: $relJson -> $source"
                }
            }
        }
    }

    $packageText = [string]::Join("`n", $packageTextParts)
    $packageAssets = @(Get-ChildItem -LiteralPath $packageDir -File -Recurse | Where-Object { $_.Extension -in ".c", ".h" } | Sort-Object FullName)
    foreach ( $asset in $packageAssets ) {
        if ( $packageText -notmatch [regex]::Escape($asset.Name) ) {
            $relative = $asset.FullName.Substring($rootPath.Length + 1).Replace("\", "/")
            Add-Failure "package C fixture is not declared by package metadata: $relative"
        }
    }
}

if ( $failures.Count -gt 0 ) {
    foreach ( $failure in $failures ) {
        Write-Host $failure
    }
    exit 1
}

$entryCount = @(Get-ChildItem -Path (Join-Path $Root "test\cases"), (Join-Path $Root "examples") -File -Recurse | Where-Object { $_.Extension -in ".c", ".h" }).Count
$packageCount = 0
if ( Test-Path -LiteralPath $packageDir -PathType Container ) {
    $packageCount = @(Get-ChildItem -LiteralPath $packageDir -File -Recurse | Where-Object { $_.Extension -in ".c", ".h" }).Count
}

Write-Host "C fixture wiring ok: $entryCount entry fixtures, $packageCount package fixtures."
exit 0
