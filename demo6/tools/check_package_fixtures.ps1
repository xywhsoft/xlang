param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$packageDir = Join-Path $Root "packages"
$failures = New-Object System.Collections.Generic.List[string]
$missingXapiPackages = @("missing_interface")
$legacyFormatPackages = @("broken_xapi")
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

    $packageIndex.Add(("{0}@{1}#abi{2}" -f $manifest.name, $manifest.version, $manifest.abiVersion)) | Out-Null
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

    if ( $legacyFormatPackages -contains $pkg.Name ) {
        if ( $xapi.format -eq "xlang-xapi-1" ) {
            Add-Failure "legacy-format negative package unexpectedly uses xlang-xapi-1: packages/$($pkg.Name)/$xapiRel"
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
foreach ( $name in $legacyFormatPackages ) {
    if ( ! $manifests.ContainsKey($name) ) {
        Add-Failure "legacy-format negative package not found: $name"
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
