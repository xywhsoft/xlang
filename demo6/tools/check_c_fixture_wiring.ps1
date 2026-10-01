param(
    [string]$Root = "",
    [switch]$CoreOnly
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$testSuiteDir = Join-Path $Root "test\suites"
$entryWiringFiles = @(
    "test.bat",
    "test_all.bat",
    "test_linux.sh",
    "test_tutorial.bat",
    "test_dll.bat",
    "build_dll.bat",
    "test\featured_extlibs.json",
    "test\phase1_extlibs.json",
    "test\platform_extlibs.json",
    "tools\run_featured_extlibs_rc.ps1",
    "tools\run_phase1_extlibs.ps1",
    "tools\run_platform_extlibs.ps1",
    "tools\run_xlir_parameterized_guards.ps1",
    "tools\build_xlir_public_source_driver.ps1",
    "release\examples\extlibs\run_extlibs.bat",
    "release\examples\feature_suite\run_feature_suite.bat"
)

$minEntryFixtures = 45
$minPackageFixtures = if ( $CoreOnly ) { 10 } else { 55 }
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

function Get-CheckScriptWiringFiles()
{
    $files = New-Object System.Collections.Generic.List[string]
    $toolDir = Join-Path $script:Root "tools"

    if ( Test-Path -LiteralPath $toolDir -PathType Container ) {
        $rootPath = (Resolve-Path -LiteralPath $script:Root).Path
        foreach ( $scriptFile in @(Get-ChildItem -LiteralPath $toolDir -File -Filter "check_*.ps1" |
            Sort-Object Name) ) {
            $files.Add($scriptFile.FullName.Substring($rootPath.Length + 1)) | Out-Null
        }
    }

    return @($files)
}

function Get-ManualCjsWiringFiles()
{
    $files = New-Object System.Collections.Generic.List[string]
    $toolDir = Join-Path $script:Root "tools"

    if ( Test-Path -LiteralPath $toolDir -PathType Container ) {
        $rootPath = (Resolve-Path -LiteralPath $script:Root).Path
        foreach ( $scriptFile in @(Get-ChildItem -LiteralPath $toolDir -File -Filter "check_*.cjs" |
            Sort-Object Name) ) {
            $files.Add($scriptFile.FullName.Substring($rootPath.Length + 1)) | Out-Null
        }
    }

    return @($files)
}

function Get-LiteralCReferencePaths($text)
{
    $paths = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
    # Expand the fixed literal matrices once, not once per C/H asset. This is
    # the same finite matching contract; arbitrary interpolations do not count.
    foreach ($loop in [regex]::Matches($text,
        '(?s)foreach\s*\(\s*\$(?<variable>[A-Za-z_][A-Za-z0-9_]*)\s+in\s+@\((?<items>[^)]*)\)')) {
        $variable = $loop.Groups['variable'].Value
        $items = @([regex]::Matches($loop.Groups['items'].Value, '["''](?<item>[A-Za-z0-9_-]+)["'']'))
        if (!$items.Count) { continue }
        $templates = [regex]::Matches($text,
            '"(?<path>[^"\r\n]*\$' + [regex]::Escape($variable) + '(?![A-Za-z0-9_])[^"\r\n]*[.]c)"')
        foreach ($template in $templates) {
            foreach ($item in $items) {
                $expanded = $template.Groups['path'].Value.Replace('$' + $variable, $item.Groups['item'].Value)
                [void]$paths.Add($expanded.Replace('\', '/'))
            }
        }
    }
    return ,$paths
}

function Test-DirectReference($text, $relative, $literalPaths)
{
    $slashPath = $relative.Replace("\", "/")
    $backslashPath = $relative.Replace("/", "\")

    if ( $text -match [regex]::Escape($slashPath) ) {
        return $true
    }
    if ( $text -match [regex]::Escape($backslashPath) ) {
        return $true
    }

    # Some table-driven runners store fixture stems and append .c when they
    # assemble a compiler command.  The directory-qualified stem is still an
    # explicit reference and must count as wiring.
    $slashStem = $slashPath -replace '\.[^./]+$', ''
    $backslashStem = $backslashPath -replace '\.[^.\\]+$', ''
    if ( $text -match [regex]::Escape($slashStem) -or
         $text -match [regex]::Escape($backslashStem) ) {
        return $true
    }
    $leafStem = [System.IO.Path]::GetFileNameWithoutExtension($relative)
    if ( $text -match ('(?<![A-Za-z0-9_])' + [regex]::Escape($leafStem) + '(?![A-Za-z0-9_])') ) {
        return $true
    }
    return $literalPaths.Contains($slashPath)
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
$entryWiringFiles += Get-CheckScriptWiringFiles
$entryWiringFiles += Get-ManualCjsWiringFiles
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
$literalCPaths = Get-LiteralCReferencePaths $entryText
$rootPath = (Resolve-Path -LiteralPath $Root).Path
$entryAssetRoots = @("test\cases", "release\examples")
$entryAssets = New-Object System.Collections.Generic.List[object]

foreach ( $assetRoot in $entryAssetRoots ) {
    $fullRoot = Join-Path $Root $assetRoot
    if ( ! (Test-Path -LiteralPath $fullRoot -PathType Container) ) {
        Add-Failure "asset directory missing: $assetRoot"
        continue
    }

    $assets = @(Get-ChildItem -LiteralPath $fullRoot -File -Recurse | Where-Object {
        $relative = $_.FullName.Substring($rootPath.Length + 1).Replace("\", "/")
        ($_.Extension -in ".c", ".h") -and
            (! $CoreOnly -or ! $relative.StartsWith("release/examples/extlibs/"))
    } | Sort-Object FullName)
    foreach ( $asset in $assets ) {
        $entryAssets.Add($asset) | Out-Null
    }
}

$assetByRelative = @{}
$wiredAssets = New-Object System.Collections.Generic.HashSet[string] ([System.StringComparer]::OrdinalIgnoreCase)
foreach ( $asset in $entryAssets ) {
    $relative = $asset.FullName.Substring($rootPath.Length + 1).Replace("\", "/")
    $assetByRelative[$relative] = $asset
    if ( (Test-SameNameXlangCompanion $asset) -or (Test-DirectReference $entryText $relative $literalCPaths) ) {
        $wiredAssets.Add($relative) | Out-Null
    }
}

# A directly compiled fixture owns its quoted local C/H includes.  Resolve the
# include graph transitively so shared assertion headers and included contract
# fragments are not required to appear redundantly in a batch command.
do {
    $addedInclude = $false
    foreach ( $relative in @($wiredAssets) ) {
        $asset = $assetByRelative[$relative]
        if ( $null -eq $asset ) {
            continue
        }
        $text = Read-Utf8Text $asset.FullName
        foreach ( $includeMatch in [regex]::Matches($text, '"([^"]+\.(?:c|h))"') ) {
            $includeName = $includeMatch.Groups[1].Value.Replace("/", "\")
            $candidates = @(
                [System.IO.Path]::GetFullPath((Join-Path $asset.DirectoryName $includeName)),
                [System.IO.Path]::GetFullPath((Join-Path $Root $includeName))
            )
            foreach ( $candidate in $candidates ) {
                if ( ! $candidate.StartsWith($rootPath + "\", [System.StringComparison]::OrdinalIgnoreCase) ) {
                    continue
                }
                $includedRelative = $candidate.Substring($rootPath.Length + 1).Replace("\", "/")
                if ( $assetByRelative.ContainsKey($includedRelative) -and $wiredAssets.Add($includedRelative) ) {
                    $addedInclude = $true
                }
            }
        }
    }
} while ( $addedInclude )

foreach ( $asset in $entryAssets ) {
    $relative = $asset.FullName.Substring($rootPath.Length + 1).Replace("\", "/")
    if ( ! $wiredAssets.Contains($relative) ) {
        Add-Failure "C fixture is not wired: $relative"
    }
}

$referenced = New-Object System.Collections.Generic.HashSet[string]
$matches = [regex]::Matches($entryText, '(test[\\/]+cases|release[\\/]+examples)[\\/]+([^"''\s>,;]+\.(?:c|h))')
foreach ( $match in $matches ) {
    $rawTail = $match.Groups[2].Value
    if ( $rawTail.Contains('\s+') -or $rawTail.Contains('\.') -or
         $rawTail.Contains('[') -or $rawTail.Contains(']') ) {
        continue
    }
    $relative = ($match.Groups[1].Value + "/" + $match.Groups[2].Value).Replace("\", "/")
    if ( $relative.Contains("%") -or $relative.Contains("$") -or
         $relative.Contains("*") -or $relative.Contains("{") -or
         $relative.Contains("}") ) {
        continue
    }
    $referenced.Add($relative) | Out-Null
}

foreach ( $relative in ($referenced | Sort-Object) ) {
    if ( $CoreOnly -and $relative.StartsWith("release/examples/extlibs/") ) {
        continue
    }
    $path = Join-Path $Root ($relative.Replace("/", "\"))
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        Add-Failure "wired C fixture file is missing: $relative"
    }
}

$packageDirs = @((Join-Path $Root "test\fixtures"))
if ( ! $CoreOnly ) {
    $packageDirs = @((Join-Path $Root "release\extlibs")) + $packageDirs
}
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

        $sourceMatches = [regex]::Matches($text, '"(?<kind>cSources|cResources)"\s*:\s*\[(?<items>[^\]]*)\]')
        foreach ( $sourceMatch in $sourceMatches ) {
            $sourceKind = $sourceMatch.Groups["kind"].Value
            $items = $sourceMatch.Groups["items"].Value
            $itemMatches = [regex]::Matches($items, '"([^"]+\.c)"')
            foreach ( $itemMatch in $itemMatches ) {
                $source = $itemMatch.Groups[1].Value
                $sourcePath = Join-Path $json.DirectoryName $source
                Add-PackageDeclaredSource $sourcePath

                if ( ! (Test-Path -LiteralPath $sourcePath -PathType Leaf) ) {
                    $relJson = $json.FullName.Substring($rootPath.Length + 1).Replace("\", "/")
                    Add-Failure "package $sourceKind missing file: $relJson -> $source"
                }
            }
        }
    }

    foreach ( $xlp in $xlpFiles ) {
        $text = Read-Utf8Text $xlp.FullName

        $sourceMatches = [regex]::Matches($text, '"(?<kind>cSources|cResources)"\s*:\s*\[(?<items>[^\]]*)\]')
        foreach ( $sourceMatch in $sourceMatches ) {
            $sourceKind = $sourceMatch.Groups["kind"].Value
            $items = $sourceMatch.Groups["items"].Value
            $itemMatches = [regex]::Matches($items, '"([^"]+\.c)"')
            foreach ( $itemMatch in $itemMatches ) {
                $source = $itemMatch.Groups[1].Value
                $sourcePath = Join-Path $xlp.DirectoryName $source
                Add-PackageDeclaredSource $sourcePath

                if ( ! (Test-Path -LiteralPath $sourcePath -PathType Leaf) ) {
                    $relXlp = $xlp.FullName.Substring($rootPath.Length + 1).Replace("\", "/")
                    Add-Failure "xlp $sourceKind missing file: $relXlp -> $source"
                }
            }
        }
    }

    $packageAssets = @(Get-ChildItem -LiteralPath $packageDir -File -Recurse | Where-Object { $_.Extension -eq ".c" } | Sort-Object FullName)
    foreach ( $asset in $packageAssets ) {
        $relative = $asset.FullName.Substring($rootPath.Length + 1).Replace("\", "/")
        $assetFullPath = [System.IO.Path]::GetFullPath($asset.FullName)
        if ( ! $packageDeclaredSources.ContainsKey($assetFullPath.ToLowerInvariant()) -and
            ! (Test-DirectReference $entryText $relative $literalCPaths) ) {
            Add-Failure "package C source is neither declared by metadata nor wired as a test: $relative"
        }
    }
}

$entryCount = @(Get-ChildItem -Path (Join-Path $Root "test\cases"), (Join-Path $Root "release\examples") -File -Recurse | Where-Object {
    $relative = $_.FullName.Substring($rootPath.Length + 1).Replace("\", "/")
    ($_.Extension -in ".c", ".h") -and
        (! $CoreOnly -or ! $relative.StartsWith("release/examples/extlibs/"))
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

$scope = if ( $CoreOnly ) { "core" } else { "all" }
Write-Host "C fixture wiring ok ($scope): $entryCount entry fixtures, $packageCount package fixtures."
exit 0
