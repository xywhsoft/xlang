param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$testSuiteDir = Join-Path $Root "test\suites"
$entryWiringFiles = @(
    "test.bat",
    "test_all.bat",
    "test_tutorial.bat",
    "test_dll.bat",
    "build_dll.bat",
    "release\examples\feature_suite\run_feature_suite.bat"
)

$minEntryFixtures = 45
$minPackageFixtures = 55
$failures = New-Object System.Collections.Generic.List[string]

function Add-Failure($message)
{
    $script:failures.Add($message) | Out-Null
}

function Read-Utf8Text($path)
{
    return [System.IO.File]::ReadAllText($path, [System.Text.Encoding]::UTF8)
}

function Get-TestSuiteWiringFiles()
{
    $files = New-Object System.Collections.Generic.List[string]

    if ( Test-Path -LiteralPath $script:testSuiteDir -PathType Container ) {
        $rootPath = (Resolve-Path -LiteralPath $script:Root).Path
        $suiteFiles = @(Get-ChildItem -LiteralPath $script:testSuiteDir -File -Filter "*.bat" | Sort-Object Name)
        foreach ( $suite in $suiteFiles ) {
            $files.Add($suite.FullName.Substring($rootPath.Length + 1)) | Out-Null
        }
    }

    return @($files)
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

function Test-SameNameXlangCompanion($asset)
{
    if ( $asset.Extension -ne ".c" ) {
        return $false
    }

    $xlPath = [System.IO.Path]::ChangeExtension($asset.FullName, ".xl")
    return (Test-Path -LiteralPath $xlPath -PathType Leaf)
}

$entryWiringFiles += Get-TestSuiteWiringFiles
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
$entryAssetRoots = @("test\cases", "release\examples")

foreach ( $assetRoot in $entryAssetRoots ) {
    $fullRoot = Join-Path $Root $assetRoot
    if ( ! (Test-Path -LiteralPath $fullRoot -PathType Container) ) {
        Add-Failure "asset directory missing: $assetRoot"
        continue
    }

    $assets = @(Get-ChildItem -LiteralPath $fullRoot -File -Recurse | Where-Object {
        $relative = $_.FullName.Substring($rootPath.Length + 1).Replace("\", "/")
        ($_.Extension -in ".c", ".h")
    } | Sort-Object FullName)
    foreach ( $asset in $assets ) {
        $relative = $asset.FullName.Substring($rootPath.Length + 1).Replace("\", "/")
        if ( Test-SameNameXlangCompanion $asset ) {
            continue
        }
        if ( ! (Test-DirectReference $entryText $relative) ) {
            Add-Failure "C fixture is not wired: $relative"
        }
    }
}

$referenced = New-Object System.Collections.Generic.HashSet[string]
$matches = [regex]::Matches($entryText, '(test[\\/]+cases|release[\\/]+examples)[\\/]+([^"''\s>]+\.(?:c|h))')
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

$packageDirs = @(
    (Join-Path $Root "release\extlibs"),
    (Join-Path $Root "test\fixtures")
)
$packageDeclaredSources = @{}

foreach ( $packageDir in $packageDirs ) {
    if ( ! (Test-Path -LiteralPath $packageDir -PathType Container) ) {
        continue
    }

    $jsonFiles = @(Get-ChildItem -LiteralPath $packageDir -File -Recurse -Filter "*.json" | Sort-Object FullName)
    $xlpFiles = @(Get-ChildItem -LiteralPath $packageDir -File -Recurse -Filter "*.xlp" | Sort-Object FullName)

    function Add-PackageDeclaredSource($sourcePath)
    {
        $sourceFullPath = [System.IO.Path]::GetFullPath($sourcePath)
        $sourceKey = $sourceFullPath.ToLowerInvariant()

        if ( $packageDeclaredSources.ContainsKey($sourceKey) ) {
            return
        }

        $packageDeclaredSources[$sourceKey] = $true

        if ( ! (Test-Path -LiteralPath $sourceFullPath -PathType Leaf) ) {
            return
        }

        $sourceText = Read-Utf8Text $sourceFullPath
        $includeMatches = [regex]::Matches($sourceText, '(?m)^\s*#\s*include\s+"([^"]+[.]c)"')
        foreach ( $includeMatch in $includeMatches ) {
            $includeName = $includeMatch.Groups[1].Value
            $includePath = Join-Path ([System.IO.Path]::GetDirectoryName($sourceFullPath)) $includeName
            Add-PackageDeclaredSource $includePath
        }
    }

    foreach ( $json in $jsonFiles ) {
        $text = Read-Utf8Text $json.FullName

        $sourceMatches = [regex]::Matches($text, '"cSources"\s*:\s*\[(?<items>[^\]]*)\]')
        foreach ( $sourceMatch in $sourceMatches ) {
            $items = $sourceMatch.Groups["items"].Value
            $itemMatches = [regex]::Matches($items, '"([^"]+\.c)"')
            foreach ( $itemMatch in $itemMatches ) {
                $source = $itemMatch.Groups[1].Value
                $sourcePath = Join-Path $json.DirectoryName $source
                Add-PackageDeclaredSource $sourcePath

                if ( ! (Test-Path -LiteralPath $sourcePath -PathType Leaf) ) {
                    $relJson = $json.FullName.Substring($rootPath.Length + 1).Replace("\", "/")
                    Add-Failure "package cSources missing file: $relJson -> $source"
                }
            }
        }
    }

    foreach ( $xlp in $xlpFiles ) {
        $text = Read-Utf8Text $xlp.FullName

        $sourceMatches = [regex]::Matches($text, '"cSources"\s*:\s*\[(?<items>[^\]]*)\]')
        foreach ( $sourceMatch in $sourceMatches ) {
            $items = $sourceMatch.Groups["items"].Value
            $itemMatches = [regex]::Matches($items, '"([^"]+\.c)"')
            foreach ( $itemMatch in $itemMatches ) {
                $source = $itemMatch.Groups[1].Value
                $sourcePath = Join-Path $xlp.DirectoryName $source
                Add-PackageDeclaredSource $sourcePath

                if ( ! (Test-Path -LiteralPath $sourcePath -PathType Leaf) ) {
                    $relXlp = $xlp.FullName.Substring($rootPath.Length + 1).Replace("\", "/")
                    Add-Failure "xlp cSources missing file: $relXlp -> $source"
                }
            }
        }
    }

    $packageAssets = @(Get-ChildItem -LiteralPath $packageDir -File -Recurse | Where-Object { $_.Extension -eq ".c" } | Sort-Object FullName)
    foreach ( $asset in $packageAssets ) {
        $relative = $asset.FullName.Substring($rootPath.Length + 1).Replace("\", "/")
        $assetFullPath = [System.IO.Path]::GetFullPath($asset.FullName)
        if ( ! $packageDeclaredSources.ContainsKey($assetFullPath.ToLowerInvariant()) -and
            ! (Test-DirectReference $entryText $relative) ) {
            Add-Failure "package C source is neither declared by metadata nor wired as a test: $relative"
        }
    }
}

$entryCount = @(Get-ChildItem -Path (Join-Path $Root "test\cases"), (Join-Path $Root "release\examples") -File -Recurse | Where-Object {
    $relative = $_.FullName.Substring($rootPath.Length + 1).Replace("\", "/")
    ($_.Extension -in ".c", ".h")
}).Count
$packageCount = 0
foreach ( $packageDir in $packageDirs ) {
    if ( Test-Path -LiteralPath $packageDir -PathType Container ) {
        $packageCount += @(Get-ChildItem -LiteralPath $packageDir -File -Recurse | Where-Object { $_.Extension -eq ".c" }).Count
    }
}

if ( $entryCount -lt $minEntryFixtures ) {
    Add-Failure "entry C fixture baseline regressed: $entryCount < $minEntryFixtures"
}

if ( $packageCount -lt $minPackageFixtures ) {
    Add-Failure "package C fixture baseline regressed: $packageCount < $minPackageFixtures"
}

if ( $failures.Count -gt 0 ) {
    foreach ( $failure in $failures ) {
        Write-Host $failure
    }
    exit 1
}

Write-Host "C fixture wiring ok: $entryCount entry fixtures, $packageCount package fixtures."
exit 0
