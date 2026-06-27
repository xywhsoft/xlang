param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$packageDir = Join-Path $Root "packages"
$readmePath = Join-Path $packageDir "README.md"
$minPackages = 17
$failures = New-Object System.Collections.Generic.List[string]
$missingXapiPackages = @("missing_interface")
$invalidFormatPackages = @("broken_xapi")
$badDependencyPackages = @("uses_dep_bad")

function Add-Failure($message)
{
    $script:failures.Add($message) | Out-Null
}

function Read-Json($path)
{
    $text = [System.IO.File]::ReadAllText($path, [System.Text.Encoding]::UTF8)
    return $text | ConvertFrom-Json
}

function String-Array($value)
{
    if ( $null -eq $value ) {
        return @()
    }
    return @($value | ForEach-Object { [string]$_ })
}

function Artifact-Sources($manifest)
{
    if ( $null -eq $manifest.artifact -or $null -eq $manifest.artifact.cSources ) {
        return @()
    }
    return String-Array $manifest.artifact.cSources
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

if ( ! (Test-Path -LiteralPath $packageDir -PathType Container) ) {
    Write-Host "packages directory missing: $packageDir"
    exit 1
}

$packages = @(Get-ChildItem -LiteralPath $packageDir -Directory | Sort-Object Name)
$manifests = @{}
$packageIndex = New-Object System.Collections.Generic.HashSet[string]
$moduleIndex = New-Object System.Collections.Generic.HashSet[string]
$exportIndex = New-Object System.Collections.Generic.HashSet[string]

if ( $packages.Count -lt $minPackages ) {
    Add-Failure "package fixture baseline regressed: $($packages.Count) < $minPackages"
}

if ( ! (Test-Path -LiteralPath $readmePath -PathType Leaf) ) {
    Add-Failure "packages README missing: packages/README.md"
} else {
    $readmeText = [System.IO.File]::ReadAllText($readmePath, [System.Text.Encoding]::UTF8)
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

        $path = Join-Path $packageDir $name
        if ( ! (Test-Path -LiteralPath $path -PathType Container) ) {
            Add-Failure "packages README references missing package directory: packages/$name"
        }
    }

    foreach ( $pkg in $packages ) {
        if ( ! $readmePackageNames.Contains($pkg.Name) ) {
            Add-Failure "packages README does not list package directory: packages/$($pkg.Name)"
        }
    }
}

foreach ( $pkg in $packages ) {
    $manifestPath = Join-Path $pkg.FullName "xlang.package.json"
    if ( ! (Test-Path -LiteralPath $manifestPath -PathType Leaf) ) {
        Add-Failure "package manifest missing: packages/$($pkg.Name)/xlang.package.json"
        continue
    }

    try {
        $manifest = Read-Json $manifestPath
    } catch {
        Add-Failure "package manifest is not valid JSON: packages/$($pkg.Name)/xlang.package.json"
        continue
    }

    $manifests[$pkg.Name] = $manifest

    if ( $manifest.format -ne "xlang-package-1" ) {
        Add-Failure "package manifest format mismatch: packages/$($pkg.Name)/xlang.package.json"
    }
    if ( [string]::IsNullOrEmpty($manifest.name) ) {
        Add-Failure "package manifest name missing: packages/$($pkg.Name)/xlang.package.json"
    }
    if ( [string]::IsNullOrEmpty($manifest.version) ) {
        Add-Failure "package manifest version missing: packages/$($pkg.Name)/xlang.package.json"
    }
    if ( [string]::IsNullOrEmpty($manifest.abiVersion) ) {
        Add-Failure "package manifest abiVersion missing: packages/$($pkg.Name)/xlang.package.json"
    }
    if ( [string]::IsNullOrEmpty($manifest.xapi) ) {
        Add-Failure "package manifest xapi missing: packages/$($pkg.Name)/xlang.package.json"
    }

    $expectedPackageId = "{0}@{1}#abi{2}" -f $manifest.name, $manifest.version, $manifest.abiVersion
    if ( ! $packageIndex.Add($expectedPackageId) ) {
        Add-Failure "duplicate package identity: $expectedPackageId"
    }
}

foreach ( $pkg in $packages ) {
    if ( ! $manifests.ContainsKey($pkg.Name) ) {
        continue
    }

    $manifest = $manifests[$pkg.Name]
    $xapiRel = [string]$manifest.xapi
    $xapiPath = Join-Path $pkg.FullName $xapiRel

    if ( ! (Test-Path -LiteralPath $xapiPath -PathType Leaf) ) {
        if ( $missingXapiPackages -notcontains $pkg.Name ) {
            Add-Failure "package xapi file missing: packages/$($pkg.Name)/$xapiRel"
        }
        continue
    }

    try {
        $xapi = Read-Json $xapiPath
    } catch {
        Add-Failure "package xapi is not valid JSON: packages/$($pkg.Name)/$xapiRel"
        continue
    }

    if ( $invalidFormatPackages -contains $pkg.Name ) {
        if ( $xapi.format -eq "xlang-xapi-1" ) {
            Add-Failure "invalid-format negative package unexpectedly uses xlang-xapi-1: packages/$($pkg.Name)/$xapiRel"
        }
        continue
    }

    if ( $xapi.format -ne "xlang-xapi-1" ) {
        Add-Failure "package xapi format mismatch: packages/$($pkg.Name)/$xapiRel"
        continue
    }

    if ( $xapi.package.name -ne $manifest.name ) {
        Add-Failure "package name mismatch between manifest and xapi: packages/$($pkg.Name)"
    }
    if ( $xapi.package.version -ne $manifest.version ) {
        Add-Failure "package version mismatch between manifest and xapi: packages/$($pkg.Name)"
    }
    if ( $xapi.package.abiVersion -ne $manifest.abiVersion ) {
        Add-Failure "package abiVersion mismatch between manifest and xapi: packages/$($pkg.Name)"
    }

    $expectedPackageId = "{0}@{1}#abi{2}" -f $manifest.name, $manifest.version, $manifest.abiVersion
    if ( [string]$xapi.package.packageId -ne $expectedPackageId ) {
        Add-Failure "packageId mismatch between manifest and xapi: packages/$($pkg.Name)"
    }

    if ( [string]::IsNullOrEmpty($xapi.module.name) ) {
        Add-Failure "package xapi module.name missing: packages/$($pkg.Name)/$xapiRel"
    } elseif ( [string]$xapi.module.name -ne [string]$manifest.name ) {
        Add-Failure "module name mismatch between manifest and xapi: packages/$($pkg.Name)"
    }

    $expectedModuleId = "{0}::{1}" -f $expectedPackageId, $xapi.module.name
    if ( [string]$xapi.module.moduleId -ne $expectedModuleId ) {
        Add-Failure "moduleId mismatch between packageId and module name: packages/$($pkg.Name)"
    } elseif ( ! $moduleIndex.Add([string]$xapi.module.moduleId) ) {
        Add-Failure "duplicate xapi moduleId: $($xapi.module.moduleId)"
    }

    $expectedSource = "packages/$($pkg.Name)"
    if ( [string]$xapi.module.source -ne $expectedSource ) {
        Add-Failure "module source mismatch: packages/$($pkg.Name), expected $expectedSource"
    }

    if ( [string]$xapi.entry.kind -ne "none" ) {
        Add-Failure "package xapi entry.kind must be none: packages/$($pkg.Name)"
    }
    if ( [string]$xapi.entry.initSymbol -ne "xlInit" ) {
        Add-Failure "package xapi entry.initSymbol must be xlInit: packages/$($pkg.Name)"
    }
    if ( [string]$xapi.entry.unitSymbol -ne "xlUnit" ) {
        Add-Failure "package xapi entry.unitSymbol must be xlUnit: packages/$($pkg.Name)"
    }

    foreach ( $export in @($xapi.exports) ) {
        if ( $null -eq $export ) {
            continue
        }

        if ( [string]::IsNullOrEmpty($export.kind) ) {
            Add-Failure "package xapi export kind missing: packages/$($pkg.Name)"
        }
        if ( [string]::IsNullOrEmpty($export.name) ) {
            Add-Failure "package xapi export name missing: packages/$($pkg.Name)"
        }
        if ( [string]::IsNullOrEmpty($export.signature) ) {
            Add-Failure "package xapi export signature missing: packages/$($pkg.Name)::$($export.name)"
        }
        if ( [string]::IsNullOrEmpty($export.exportId) ) {
            Add-Failure "package xapi exportId missing: packages/$($pkg.Name)::$($export.name)"
            continue
        }

        $expectedExportId = "{0}::{1}({2})" -f $xapi.module.moduleId, $export.name, $export.signature
        if ( [string]$export.exportId -ne $expectedExportId ) {
            Add-Failure "package xapi exportId mismatch: packages/$($pkg.Name)::$($export.name)"
        }

        if ( ! $exportIndex.Add([string]$export.exportId) ) {
            Add-Failure "duplicate xapi exportId: $($export.exportId)"
        }
    }

    $manifestSources = Artifact-Sources $manifest
    $xapiSources = String-Array $xapi.cSources
    if ( ! (Same-StringSet $manifestSources $xapiSources) ) {
        Add-Failure "package cSources mismatch between manifest and xapi: packages/$($pkg.Name)"
    }

    foreach ( $source in $manifestSources ) {
        $sourcePath = Join-Path $pkg.FullName $source
        if ( ! (Test-Path -LiteralPath $sourcePath -PathType Leaf) ) {
            Add-Failure "package cSource missing: packages/$($pkg.Name)/$source"
        }
    }

    if ( ! (Same-StringSet (String-Array $manifest.links) (String-Array $xapi.links)) ) {
        Add-Failure "package links mismatch between manifest and xapi: packages/$($pkg.Name)"
    }
    if ( ! (Same-StringSet (String-Array $manifest.loadlibs) (String-Array $xapi.loadlibs)) ) {
        Add-Failure "package loadlibs mismatch between manifest and xapi: packages/$($pkg.Name)"
    }

    if ( $null -ne $manifest.native -and $null -ne $xapi.native ) {
        if ( [bool]$manifest.native.singleton -ne [bool]$xapi.native.singleton ) {
            Add-Failure "package native.singleton mismatch between manifest and xapi: packages/$($pkg.Name)"
        }
        if ( [string]$manifest.native.loadName -ne [string]$xapi.native.loadName ) {
            Add-Failure "package native.loadName mismatch between manifest and xapi: packages/$($pkg.Name)"
        }
        if ( [string]$manifest.native.version -ne [string]$xapi.native.version ) {
            Add-Failure "package native.version mismatch between manifest and xapi: packages/$($pkg.Name)"
        }
    }

    if ( $badDependencyPackages -notcontains $pkg.Name ) {
        foreach ( $dep in @($manifest.dependencies)) {
            if ( $null -eq $dep ) {
                continue
            }
            $key = "{0}@{1}#abi{2}" -f $dep.name, $dep.version, $dep.abiVersion
            if ( ! $packageIndex.Contains($key) ) {
                Add-Failure "package dependency target missing: packages/$($pkg.Name) -> $key"
            }
        }
    }
}

foreach ( $name in $missingXapiPackages ) {
    if ( ! $manifests.ContainsKey($name) ) {
        Add-Failure "missing-xapi negative package not found: $name"
    }
}
foreach ( $name in $invalidFormatPackages ) {
    if ( ! $manifests.ContainsKey($name) ) {
        Add-Failure "invalid-format negative package not found: $name"
    }
}
foreach ( $name in $badDependencyPackages ) {
    if ( ! $manifests.ContainsKey($name) ) {
        Add-Failure "bad-dependency negative package not found: $name"
    }
}

if ( $failures.Count -gt 0 ) {
    foreach ( $failure in $failures ) {
        Write-Host $failure
    }
    exit 1
}

Write-Host "Package fixtures ok: $($packages.Count) packages."
exit 0
