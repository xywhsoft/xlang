param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$releaseDir = Join-Path $Root "release\extlibs"
$exampleDir = Join-Path $Root "release\examples\extlibs"
$runnerPath = Join-Path $exampleDir "run_extlibs.bat"
$expectedMinPackages = 31
$fixtureNames = @(
    "broken_xapi",
    "class_api",
    "class_inherit_api",
    "counter",
    "dep_value",
    "dep_value@2.0.0",
    "invalid_decl_xapi",
    "libprobe",
    "metadata_combo",
    "missing_interface",
    "native_one",
    "native_two",
    "settings",
    "union_api",
    "unsupported_xapi",
    "uses_dep",
    "uses_dep_bad",
    "uses_dep_v2"
)
$badPathFragments = @(
    "test/fixtures",
    "test\fixtures",
    "demo6/packages",
    "demo6\packages",
    "release/libs",
    "release\libs",
    "examples/libs",
    "examples\libs",
    "release/extlibs/x.",
    "release\extlibs\x."
)
$releaseTextExtensions = New-Object System.Collections.Generic.HashSet[string]([System.StringComparer]::OrdinalIgnoreCase)
@(".xl", ".json", ".md", ".bat", ".ps1", ".c", ".h", ".hpp", ".cpp") | ForEach-Object {
    $releaseTextExtensions.Add($_) | Out-Null
}
$forbiddenTextPatterns = @(
    @{ Name = "absolute drive path"; Regex = "(?<![A-Za-z0-9_])[A-Za-z]:[\\/]" },
    @{ Name = "legacy release/libs path"; Regex = "release[\\/]libs" },
    @{ Name = "legacy demo5 path"; Regex = "demo5" },
    @{ Name = "host absolute path"; Regex = "(^|[^A-Za-z0-9_])/(home|usr)/" }
)
$failures = New-Object System.Collections.Generic.List[string]
$exportNameSetsByPackage = @{}

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

function Check-PathValue($packageName, $label, $value)
{
    $text = [string]$value
    if ( $text -eq "" ) {
        return
    }
    foreach ( $bad in $script:badPathFragments ) {
        if ( $text.IndexOf($bad, [System.StringComparison]::OrdinalIgnoreCase) -ge 0 ) {
            Add-Failure "release extlib $packageName has non-release $label path: $text"
        }
    }
}

function Should-CheckReleaseText($file)
{
    if ( ! $script:releaseTextExtensions.Contains($file.Extension) ) {
        return $false
    }
    if ( $file.FullName.IndexOf("\bin\", [System.StringComparison]::OrdinalIgnoreCase) -ge 0 ) {
        return $false
    }
    if ( $file.FullName.IndexOf("\third_party\", [System.StringComparison]::OrdinalIgnoreCase) -ge 0 ) {
        return $false
    }
    return $true
}

function Check-ReleaseTextContent($rootPath, $label)
{
    $files = @(
        Get-ChildItem -LiteralPath $rootPath -Recurse -File |
            Where-Object { Should-CheckReleaseText $_ }
    )
    foreach ( $file in $files ) {
        $relative = $file.FullName.Substring($script:Root.Length).TrimStart("\", "/")
        $lines = [System.IO.File]::ReadAllLines($file.FullName, [System.Text.Encoding]::UTF8)
        for ( $i = 0; $i -lt $lines.Count; ++$i ) {
            foreach ( $pattern in $script:forbiddenTextPatterns ) {
                if ( [regex]::IsMatch($lines[$i], $pattern.Regex, [System.Text.RegularExpressions.RegexOptions]::IgnoreCase) ) {
                    Add-Failure "release $label contains $($pattern.Name): ${relative}:$($i + 1)"
                }
            }
        }
    }
}

function Text-ContainsName($text, $name)
{
    if ( [string]::IsNullOrEmpty($text) -or [string]::IsNullOrEmpty($name) ) {
        return $false
    }
    return [regex]::IsMatch($text, "\b$([regex]::Escape($name))\b")
}

function Get-PackageExportNameSet($packageName)
{
    if ( [string]::IsNullOrEmpty($packageName) ) {
        return $null
    }
    if ( $script:exportNameSetsByPackage.ContainsKey($packageName) ) {
        return $script:exportNameSetsByPackage[$packageName]
    }

    $path = Join-Path $script:releaseDir (Join-Path $packageName "package.xapi.json")
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        return $null
    }

    $xapi = Read-Json $path
    $set = New-Object System.Collections.Generic.HashSet[string]
    foreach ( $export in @($xapi.exports) ) {
        $exportName = [string]$export.name
        if ( $exportName -ne "" ) {
            $set.Add($exportName) | Out-Null
        }
    }
    foreach ( $type in @($xapi.types) ) {
        $typeName = [string]$type.name
        if ( $typeName -ne "" ) {
            $set.Add($typeName) | Out-Null
        }
    }
    $script:exportNameSetsByPackage[$packageName] = $set
    return $set
}

if ( ! (Test-Path -LiteralPath $releaseDir -PathType Container) ) {
    Write-Host "release extlibs directory missing: $releaseDir"
    exit 1
}
if ( ! (Test-Path -LiteralPath $exampleDir -PathType Container) ) {
    Write-Host "release extlib examples directory missing: $exampleDir"
    exit 1
}
if ( ! (Test-Path -LiteralPath $runnerPath -PathType Leaf) ) {
    Write-Host "release extlib runner missing: $runnerPath"
    exit 1
}

$packages = @(Get-ChildItem -LiteralPath $releaseDir -Directory | Sort-Object Name)
if ( $packages.Count -lt $expectedMinPackages ) {
    Add-Failure "release extlib package baseline regressed: $($packages.Count) < $expectedMinPackages"
}

foreach ( $fixture in $fixtureNames ) {
    if ( Test-Path -LiteralPath (Join-Path $releaseDir $fixture) -PathType Container ) {
        Add-Failure "test fixture must not be published under release/extlibs: $fixture"
    }
}

Check-ReleaseTextContent $releaseDir "extlib text"
Check-ReleaseTextContent $exampleDir "extlib example text"

$runnerText = [System.IO.File]::ReadAllText($runnerPath, [System.Text.Encoding]::UTF8)
$exampleFiles = @(Get-ChildItem -LiteralPath $exampleDir -File -Filter "*.xl" | Sort-Object Name)
$exampleTextByFile = @{}
$importsByPackage = @{}

foreach ( $file in $exampleFiles ) {
    if ( $file.Name -like "x_*.xl" ) {
        Add-Failure "release extlib example must not use old x_ prefix: $($file.Name)"
    }
    $text = [System.IO.File]::ReadAllText($file.FullName, [System.Text.Encoding]::UTF8)
    $exampleTextByFile[$file.Name] = $text
    foreach ( $match in [regex]::Matches($text, '(?m)^\s*import\s+([A-Za-z0-9_.]+)\b') ) {
        $importName = $match.Groups[1].Value
        if ( ! $importsByPackage.ContainsKey($importName) ) {
            $importsByPackage[$importName] = New-Object System.Collections.Generic.List[string]
        }
        $importsByPackage[$importName].Add($file.Name) | Out-Null
    }
    if ( ! $runnerText.Contains("release\examples\extlibs\$($file.Name)") -and
         ! $runnerText.Contains("release/examples/extlibs/$($file.Name)") ) {
        Add-Failure "release extlib example is not executed by run_extlibs.bat: $($file.Name)"
    }
}

$packageIds = New-Object System.Collections.Generic.HashSet[string]

foreach ( $pkg in $packages ) {
    $name = $pkg.Name
    $manifestPath = Join-Path $pkg.FullName "xlang.package.json"
    $xapiPath = Join-Path $pkg.FullName "package.xapi.json"

    if ( $name.StartsWith("x.") ) {
        Add-Failure "release extlib package keeps old x. namespace prefix: $name"
    }
    if ( $name.StartsWith("x_") ) {
        Add-Failure "release extlib package keeps old x_ prefix: $name"
    }
    if ( ! (Test-Path -LiteralPath $manifestPath -PathType Leaf) ) {
        Add-Failure "release extlib manifest missing: release/extlibs/$name/xlang.package.json"
        continue
    }
    if ( ! (Test-Path -LiteralPath $xapiPath -PathType Leaf) ) {
        Add-Failure "release extlib xapi missing: release/extlibs/$name/package.xapi.json"
        continue
    }

    try {
        $manifest = Read-Json $manifestPath
    } catch {
        Add-Failure "release extlib manifest is not valid JSON: release/extlibs/$name/xlang.package.json"
        continue
    }
    try {
        $xapi = Read-Json $xapiPath
    } catch {
        Add-Failure "release extlib xapi is not valid JSON: release/extlibs/$name/package.xapi.json"
        continue
    }

    if ( $manifest.format -ne "xlang-package-1" ) {
        Add-Failure "release extlib manifest format mismatch: $name"
    }
    if ( $manifest.kind -ne "demo6-extlib" ) {
        Add-Failure "release extlib manifest kind mismatch: $name"
    }
    if ( [string]$manifest.name -ne $name ) {
        Add-Failure "release extlib manifest name must match directory: $name"
    }
    if ( $xapi.format -ne "xlang-xapi-1" ) {
        Add-Failure "release extlib xapi format mismatch: $name"
    }
    if ( [string]$xapi.package.name -ne $name ) {
        Add-Failure "release extlib xapi package name must match directory: $name"
    }
    if ( [string]$xapi.module.name -ne $name ) {
        Add-Failure "release extlib xapi module name must match directory: $name"
    }
    if ( [string]$xapi.module.source -ne "release/extlibs/$name" ) {
        Add-Failure "release extlib module.source mismatch: $name"
    }
    if ( [string]$manifest.xapi -ne "package.xapi.json" ) {
        Add-Failure "release extlib manifest xapi must be package.xapi.json: $name"
    }
    if ( [string]$manifest.version -ne [string]$xapi.package.version ) {
        Add-Failure "release extlib version mismatch between manifest and xapi: $name"
    }
    if ( [string]$manifest.abiVersion -ne [string]$xapi.package.abiVersion ) {
        Add-Failure "release extlib abiVersion mismatch between manifest and xapi: $name"
    }

    $expectedPackageId = "{0}@{1}#abi{2}" -f $manifest.name, $manifest.version, $manifest.abiVersion
    if ( [string]$xapi.package.packageId -ne $expectedPackageId ) {
        Add-Failure "release extlib packageId mismatch: $name"
    } elseif ( ! $packageIds.Add($expectedPackageId) ) {
        Add-Failure "duplicate release extlib package identity: $expectedPackageId"
    }

    $manifestCSources = String-Array $manifest.artifact.cSources
    $xapiCSources = String-Array $xapi.cSources
    if ( ! (Same-StringSet $manifestCSources $xapiCSources) ) {
        Add-Failure "release extlib cSources mismatch between manifest and xapi: $name"
    }
    foreach ( $source in $manifestCSources ) {
        Check-PathValue $name "cSource" $source
        if ( ! (Test-Path -LiteralPath (Join-Path $pkg.FullName $source) -PathType Leaf) ) {
            Add-Failure "release extlib cSource missing: release/extlibs/$name/$source"
        }
    }

    $manifestXlSources = String-Array $manifest.artifact.xlSources
    foreach ( $source in $manifestXlSources ) {
        Check-PathValue $name "xlSource" $source
        $sourcePath = Join-Path $pkg.FullName $source
        if ( ! (Test-Path -LiteralPath $sourcePath -PathType Leaf) ) {
            Add-Failure "release extlib xlSource missing: release/extlibs/$name/$source"
        } else {
            $sourceText = [System.IO.File]::ReadAllText($sourcePath, [System.Text.Encoding]::UTF8)
            foreach ( $importMatch in [regex]::Matches($sourceText, '(?m)^\s*import\s+([A-Za-z0-9_.]+)\b') ) {
                $importName = $importMatch.Groups[1].Value
                $exportNames = Get-PackageExportNameSet $importName
                if ( $null -eq $exportNames ) {
                    continue
                }
                $memberPattern = "(?<![A-Za-z0-9_])$([regex]::Escape($importName))\.([A-Za-z_][A-Za-z0-9_]*)\b"
                foreach ( $memberMatch in [regex]::Matches($sourceText, $memberPattern) ) {
                    $memberName = $memberMatch.Groups[1].Value
                    if ( ! $exportNames.Contains($memberName) ) {
                        Add-Failure "release extlib source references non-exported dependency symbol: $name -> $importName.$memberName in $source"
                    }
                }
            }
        }
    }

    foreach ( $value in (String-Array $manifest.links) + (String-Array $manifest.loadlibs) + (String-Array $xapi.links) + (String-Array $xapi.loadlibs) ) {
        Check-PathValue $name "link/loadlib" $value
    }
    if ( ! (Same-StringSet (String-Array $manifest.links) (String-Array $xapi.links)) ) {
        Add-Failure "release extlib links mismatch between manifest and xapi: $name"
    }
    if ( ! (Same-StringSet (String-Array $manifest.loadlibs) (String-Array $xapi.loadlibs)) ) {
        Add-Failure "release extlib loadlibs mismatch between manifest and xapi: $name"
    }

    if ( ! $importsByPackage.ContainsKey($name) ) {
        Add-Failure "release extlib has no example importing it: $name"
    } else {
        $exampleText = ""
        foreach ( $exampleName in $importsByPackage[$name] ) {
            if ( $exampleTextByFile.ContainsKey($exampleName) ) {
                $exampleText += "`n" + $exampleTextByFile[$exampleName]
            }
        }
        $functionNames = @(
            $xapi.exports |
                Where-Object { [string]$_.kind -eq "function" -and ! [string]::IsNullOrEmpty([string]$_.name) } |
                ForEach-Object { [string]$_.name } |
                Sort-Object -Unique
        )
        foreach ( $functionName in $functionNames ) {
            if ( ! (Text-ContainsName $exampleText $functionName) ) {
                Add-Failure "release extlib example coverage missing function: $name.$functionName"
            }
        }
    }
}

if ( $failures.Count -gt 0 ) {
    foreach ( $failure in $failures ) {
        Write-Host $failure
    }
    exit 1
}

Write-Host "Release extlibs ok: $($packages.Count) packages, $($exampleFiles.Count) examples."
exit 0
