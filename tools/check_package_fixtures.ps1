param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$releasePackageDir = Join-Path $Root "release\extlibs"
$packageDir = Join-Path $Root "test\fixtures"
$readmePath = Join-Path $releasePackageDir "README.md"
$testBatPath = Join-Path $Root "test.bat"
$testSuiteDir = Join-Path $Root "test\suites"
$minReleasePackages = 31
$minPackages = 15
$fixturePackageNames = @(
    "class_api",
    "class_inherit_api",
    "counter",
    "dep_value",
    "dep_value@2.0.0",
    "include_dir_pkg",
    "libprobe",
    "metadata_combo",
    "native_one",
    "native_two",
    "settings",
    "union_api",
    "uses_dep",
    "uses_dep_bad",
    "uses_dep_v2"
)
$badDependencyPackages = @("uses_dep_bad")
$failures = New-Object System.Collections.Generic.List[string]

function Add-Failure($message)
{
    $script:failures.Add($message) | Out-Null
}

function Read-Utf8Text($path)
{
    return [System.IO.File]::ReadAllText($path, [System.Text.Encoding]::UTF8)
}

function Read-Json($path)
{
    $text = Read-Utf8Text $path
    return $text | ConvertFrom-Json
}

function String-Array($value)
{
    if ( $null -eq $value ) {
        return @()
    }
    return @($value | ForEach-Object { [string]$_ })
}

function Same-StringSet($left, $right)
{
    $a = @($left | Sort-Object)
    $b = @($right | Sort-Object)
    if ( $a.Count -ne $b.Count ) {
        return $false
    }
    for ( $i = 0; $i -lt $a.Count; ++$i ) {
        if ( $a[$i] -ne $b[$i] ) {
            return $false
        }
    }
    return $true
}

function Get-TestRegressionText()
{
    $parts = New-Object System.Collections.Generic.List[string]

    if ( Test-Path -LiteralPath $script:testBatPath -PathType Leaf ) {
        $parts.Add((Read-Utf8Text $script:testBatPath)) | Out-Null
    }

    if ( Test-Path -LiteralPath $script:testSuiteDir -PathType Container ) {
        $suiteFiles = @(Get-ChildItem -LiteralPath $script:testSuiteDir -File -Filter "*.bat" | Sort-Object Name)
        foreach ( $suite in $suiteFiles ) {
            $parts.Add((Read-Utf8Text $suite.FullName)) | Out-Null
        }
    }

    return [string]::Join("`n", $parts)
}

function Find-Dependency($manifest, $name, $version, $abiVersion)
{
    foreach ( $dep in @($manifest.dependencies) ) {
        if ( $null -eq $dep ) {
            continue
        }
        if ( [string]$dep.name -eq $name -and
             [string]$dep.version -eq $version -and
             [string]$dep.abiVersion -eq $abiVersion ) {
            return $true
        }
    }
    return $false
}

function Assert-Dependency($packageName, $depName, $depVersion, $depAbiVersion)
{
    if ( ! $script:manifests.ContainsKey($packageName) ) {
        Add-Failure "package fixture missing for dependency assertion: $packageName"
        return
    }
    if ( ! (Find-Dependency $script:manifests[$packageName] $depName $depVersion $depAbiVersion) ) {
        Add-Failure "package fixture dependency mismatch: test/fixtures/$packageName must depend on $depName@$depVersion#abi$depAbiVersion"
    }
}

function Read-PackageText($name, $relative)
{
    $path = Join-Path (Join-Path $script:packageDir $name) $relative
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        Add-Failure "package fixture file missing: test/fixtures/$name/$relative"
        return ""
    }
    return Read-Utf8Text $path
}

function Validate-RelativeFileList($pkg, $manifestPath, $items, $label, $extension)
{
    foreach ( $item in $items ) {
        if ( [string]::IsNullOrWhiteSpace($item) ) {
            Add-Failure "package $label contains empty path: $manifestPath"
            continue
        }
        if ( [System.IO.Path]::IsPathRooted($item) ) {
            Add-Failure "package $label must be relative: $manifestPath -> $item"
            continue
        }
        if ( $item -match '(^|[\\/])[.][.]([\\/]|$)' ) {
            Add-Failure "package $label must not use parent traversal: $manifestPath -> $item"
            continue
        }
        if ( $extension -ne "" -and ! $item.EndsWith($extension) ) {
            Add-Failure "package $label extension mismatch: $manifestPath -> $item"
        }

        $fullPath = Join-Path $pkg.FullName $item
        if ( ! (Test-Path -LiteralPath $fullPath -PathType Leaf) ) {
            Add-Failure "package $label missing file: $manifestPath -> $item"
        }
    }
}

function Validate-RelativeDirectoryList($pkg, $manifestPath, $items, $label)
{
    foreach ( $item in $items ) {
        if ( [string]::IsNullOrWhiteSpace($item) ) {
            Add-Failure "package $label contains empty path: $manifestPath"
            continue
        }
        if ( [System.IO.Path]::IsPathRooted($item) ) {
            Add-Failure "package $label must be relative: $manifestPath -> $item"
            continue
        }
        if ( $item -match '(^|[\\/])[.][.]([\\/]|$)' ) {
            Add-Failure "package $label must not use parent traversal: $manifestPath -> $item"
            continue
        }

        $fullPath = Join-Path $pkg.FullName $item
        if ( ! (Test-Path -LiteralPath $fullPath -PathType Container) ) {
            Add-Failure "package $label missing directory: $manifestPath -> $item"
        }
    }
}

function Validate-XrtModules($manifestPath, $modules)
{
    foreach ( $module in $modules ) {
        if ( $module -notmatch '^(XRT_MODULE_|XRUNTIME_MODULE_)[A-Z0-9_]+$' ) {
            Add-Failure "package xrtModules contains invalid root module: $manifestPath -> $module"
        }
    }
}

if ( ! (Test-Path -LiteralPath $releasePackageDir -PathType Container) ) {
    Write-Host "release extlibs directory missing: $releasePackageDir"
    exit 1
}
if ( ! (Test-Path -LiteralPath $packageDir -PathType Container) ) {
    Write-Host "test fixtures directory missing: $packageDir"
    exit 1
}

$testText = Get-TestRegressionText
if ( ! $testText.Contains('set "XLANG_PACKAGE_PATH=release\extlibs;test\fixtures"') ) {
    Add-Failure "test.bat/test suites must set XLANG_PACKAGE_PATH=release\extlibs;test\fixtures before package/import regression"
}
foreach ( $snippet in @(
    "release\xl.exe run test\cases\import_dependency_versions.xl",
    "release\xl.exe build test\cases\import_dependency_versions.xl",
    "private package dependency is not exported"
) ) {
    if ( ! $testText.Contains($snippet) ) {
        Add-Failure "test.bat/test suites missing package dependency regression snippet: $snippet"
    }
}

$releasePackages = @(Get-ChildItem -LiteralPath $releasePackageDir -Directory | Sort-Object Name)
$packages = @(Get-ChildItem -LiteralPath $packageDir -Directory |
    Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName "xlang.package.json") -PathType Leaf } |
    Sort-Object Name)
$manifests = @{}
$packageIndex = New-Object System.Collections.Generic.HashSet[string]

if ( $releasePackages.Count -lt $minReleasePackages ) {
    Add-Failure "release package baseline regressed: $($releasePackages.Count) < $minReleasePackages"
}

foreach ( $name in $fixturePackageNames ) {
    $fixturePath = Join-Path $packageDir $name
    if ( ! (Test-Path -LiteralPath $fixturePath -PathType Container) ) {
        Add-Failure "package fixture directory missing: test/fixtures/$name"
    }
    $releasePath = Join-Path $releasePackageDir $name
    if ( Test-Path -LiteralPath $releasePath -PathType Container ) {
        Add-Failure "test fixture must not be published under release/extlibs: $name"
    }
}

if ( $packages.Count -lt $minPackages ) {
    Add-Failure "package fixture baseline regressed: $($packages.Count) < $minPackages"
}

if ( ! (Test-Path -LiteralPath $readmePath -PathType Leaf) ) {
    Add-Failure "packages README missing: release/extlibs/README.md"
} else {
    $readmeText = Read-Utf8Text $readmePath
    $readmePackageNames = New-Object System.Collections.Generic.HashSet[string]
    $startMarker = "<!-- package-index:start -->"
    $endMarker = "<!-- package-index:end -->"
    $start = $readmeText.IndexOf($startMarker)
    $end = $readmeText.IndexOf($endMarker)

    if ( $start -lt 0 -or $end -lt 0 -or $end -le $start ) {
        Add-Failure "packages README package index markers are missing or invalid"
        $indexText = ""
    } else {
        $indexStart = $start + $startMarker.Length
        $indexText = $readmeText.Substring($indexStart, $end - $indexStart)
    }

    $readmeMatches = [regex]::Matches($indexText, '(?m)^-\s+`([^`]+)`')
    foreach ( $match in $readmeMatches ) {
        $name = $match.Groups[1].Value
        if ( ! $readmePackageNames.Add($name) ) {
            Add-Failure "packages README lists package more than once: $name"
        }

        $path = Join-Path $releasePackageDir $name
        if ( ! (Test-Path -LiteralPath $path -PathType Container) ) {
            Add-Failure "packages README references missing package directory: release/extlibs/$name"
        }
    }

    foreach ( $pkg in $releasePackages ) {
        if ( ! $readmePackageNames.Contains($pkg.Name) ) {
            Add-Failure "packages README does not list package directory: release/extlibs/$($pkg.Name)"
        }
    }
}

foreach ( $pkg in $packages ) {
    if ( $fixturePackageNames -notcontains $pkg.Name ) {
        continue
    }

    $manifestPath = Join-Path $pkg.FullName "xlang.package.json"
    $manifestRel = "test/fixtures/$($pkg.Name)/xlang.package.json"
    try {
        $manifest = Read-Json $manifestPath
    } catch {
        Add-Failure "package manifest is not valid JSON: $manifestRel"
        continue
    }

    $manifests[$pkg.Name] = $manifest

    if ( $manifest.format -ne "xlang-package-1" ) {
        Add-Failure "package manifest format mismatch: $manifestRel"
    }
    if ( [string]::IsNullOrEmpty($manifest.name) ) {
        Add-Failure "package manifest name missing: $manifestRel"
    }
    if ( [string]::IsNullOrEmpty($manifest.version) ) {
        Add-Failure "package manifest version missing: $manifestRel"
    }
    if ( [string]::IsNullOrEmpty($manifest.abiVersion) ) {
        Add-Failure "package manifest abiVersion missing: $manifestRel"
    }
    if ( $null -eq $manifest.artifact ) {
        Add-Failure "package manifest artifact missing: $manifestRel"
        continue
    }
    if ( [string]$manifest.artifact.kind -ne "source" ) {
        Add-Failure "package artifact.kind must be source: $manifestRel"
    }

    $xlSources = String-Array $manifest.artifact.xlSources
    $cSources = String-Array $manifest.artifact.cSources
    $includeDirs = String-Array $manifest.artifact.includeDirs
    $xrtModules = String-Array $manifest.artifact.xrtModules
    if ( $xlSources.Count -eq 0 ) {
        Add-Failure "package artifact.xlSources must not be empty: $manifestRel"
    }
    Validate-RelativeFileList $pkg $manifestRel $xlSources "xlSources" ".xl"
    Validate-RelativeFileList $pkg $manifestRel $cSources "cSources" ".c"
    Validate-RelativeDirectoryList $pkg $manifestRel $includeDirs "includeDirs"
    Validate-XrtModules $manifestRel $xrtModules

    $expectedPackageId = "{0}@{1}#abi{2}" -f $manifest.name, $manifest.version, $manifest.abiVersion
    if ( ! $packageIndex.Add($expectedPackageId) ) {
        Add-Failure "duplicate package identity: $expectedPackageId"
    }

    if ( $null -ne $manifest.links ) {
        foreach ( $link in (String-Array $manifest.links) ) {
            if ( [string]::IsNullOrWhiteSpace($link) ) {
                Add-Failure "package links contains empty path: $manifestRel"
            }
        }
    }
    if ( $null -ne $manifest.loadlibs ) {
        foreach ( $loadlib in (String-Array $manifest.loadlibs) ) {
            if ( [string]::IsNullOrWhiteSpace($loadlib) ) {
                Add-Failure "package loadlibs contains empty path: $manifestRel"
            }
        }
    }

    if ( $null -ne $manifest.native ) {
        if ( $null -eq $manifest.native.singleton ) {
            Add-Failure "package native.singleton missing: $manifestRel"
        }
        if ( $null -ne $manifest.native.loadName -and [string]::IsNullOrWhiteSpace([string]$manifest.native.loadName) ) {
            Add-Failure "package native.loadName empty: $manifestRel"
        }
        if ( $null -ne $manifest.native.version -and [string]::IsNullOrWhiteSpace([string]$manifest.native.version) ) {
            Add-Failure "package native.version empty: $manifestRel"
        }
    }
}

foreach ( $pkg in $packages ) {
    if ( $fixturePackageNames -notcontains $pkg.Name -or ! $manifests.ContainsKey($pkg.Name) ) {
        continue
    }
    if ( $badDependencyPackages -contains $pkg.Name ) {
        continue
    }
    foreach ( $dep in @($manifests[$pkg.Name].dependencies) ) {
        if ( $null -eq $dep ) {
            continue
        }
        $key = "{0}@{1}#abi{2}" -f $dep.name, $dep.version, $dep.abiVersion
        if ( ! $packageIndex.Contains($key) ) {
            Add-Failure "package dependency target missing: test/fixtures/$($pkg.Name) -> $key"
        }
    }
}

foreach ( $name in $badDependencyPackages ) {
    if ( ! $manifests.ContainsKey($name) ) {
        Add-Failure "bad-dependency negative package not found: $name"
    }
}

$requiredPackageIds = @(
    "dep_value@1.0.0#abi1",
    "dep_value@2.0.0#abi1",
    "uses_dep@1.0.0#abi1",
    "uses_dep_v2@1.0.0#abi1",
    "uses_dep_bad@1.0.0#abi1"
)
foreach ( $id in $requiredPackageIds ) {
    if ( ! $packageIndex.Contains($id) ) {
        Add-Failure "dependency-version fixture package identity missing: $id"
    }
}

Assert-Dependency "uses_dep" "dep_value" "1.0.0" "1"
Assert-Dependency "uses_dep_v2" "dep_value" "2.0.0" "1"
Assert-Dependency "uses_dep_bad" "dep_value" "3.0.0" "1"

$usesDepSource = Read-PackageText "uses_dep" "uses_dep.c"
$usesDepV2Source = Read-PackageText "uses_dep_v2" "uses_dep_v2.c"
$depValueSource = Read-PackageText "dep_value" "dep_value.c"
$depValueV2Source = Read-PackageText "dep_value@2.0.0" "dep_value_v2.c"

if ( $usesDepSource -notmatch '\bxl6_dep_value\s*\(' -or
     $usesDepSource -match '\bxl6_dep_value_v2\s*\(' ) {
    Add-Failure "uses_dep C fixture must call only the dep_value 1.0.0 C symbol"
}
if ( $usesDepV2Source -notmatch '\bxl6_dep_value_v2\s*\(' ) {
    Add-Failure "uses_dep_v2 C fixture must call the dep_value 2.0.0 C symbol"
}
if ( $depValueSource -notmatch '\bxl6_dep_value\s*\(' -or
     $depValueSource -match '\bxl6_dep_value_v2\s*\(' ) {
    Add-Failure "dep_value 1.0.0 C fixture must define only the v1 C symbol"
}
if ( $depValueV2Source -notmatch '\bxl6_dep_value_v2\s*\(' ) {
    Add-Failure "dep_value 2.0.0 C fixture must define the v2 C symbol"
}

if ( $failures.Count -gt 0 ) {
    foreach ( $failure in $failures ) {
        Write-Host $failure
    }
    exit 1
}

Write-Host "Package fixtures ok: $($packages.Count) package fixture manifests."
exit 0
