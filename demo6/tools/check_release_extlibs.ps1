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
    "class_api",
    "class_inherit_api",
    "counter",
    "dep_value",
    "dep_value@2.0.0",
    "libprobe",
    "metadata_combo",
    "missing_interface",
    "native_one",
    "native_two",
    "settings",
    "union_api",
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
$forbiddenExportNames = New-Object System.Collections.Generic.HashSet[string]([System.StringComparer]::OrdinalIgnoreCase)
@("unsupported") | ForEach-Object {
    $forbiddenExportNames.Add($_) | Out-Null
}
$externalServiceCoverageExemptionsByPackage = @{}
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

function Check-NoDraftFiles($rootPath, $label)
{
    $draftFiles = @(
        Get-ChildItem -LiteralPath $rootPath -Recurse -File |
            Where-Object {
                $_.Name.IndexOf(".draft.", [System.StringComparison]::OrdinalIgnoreCase) -ge 0 -or
                $_.Name.EndsWith(".draft", [System.StringComparison]::OrdinalIgnoreCase)
            }
    )
    foreach ( $file in $draftFiles ) {
        $relative = $file.FullName.Substring($script:Root.Length).TrimStart("\", "/")
        Add-Failure "release $label must not contain draft file: $relative"
    }
}

function Text-ContainsName($text, $name)
{
    if ( [string]::IsNullOrEmpty($text) -or [string]::IsNullOrEmpty($name) ) {
        return $false
    }
    return [regex]::IsMatch($text, "\b$([regex]::Escape($name))\b")
}

function Remove-IfFalseBlocks($text)
{
    $lines = $text -split "\r?\n"
    $out = New-Object System.Collections.Generic.List[string]
    $inBlock = $false
    foreach ( $line in $lines ) {
        if ( ! $inBlock -and $line -match '^\s*if\s+false\s+then\b' ) {
            $inBlock = $true
            continue
        }
        if ( $inBlock ) {
            if ( $line -match '^\s*end\s*$' ) {
                $inBlock = $false
            }
            continue
        }
        $out.Add($line) | Out-Null
    }
    return $out -join "`n"
}

function Is-CoverageExempt($packageName, $functionName)
{
    if ( ! $script:externalServiceCoverageExemptionsByPackage.ContainsKey($packageName) ) {
        return $false
    }
    return @($script:externalServiceCoverageExemptionsByPackage[$packageName]) -contains $functionName
}

function Sanitize-PackageName($name)
{
    return ([string]$name).Replace(".", "_").Replace("-", "_")
}

function Get-PublicFunctionNames($pkg, $manifest)
{
    $set = New-Object System.Collections.Generic.HashSet[string]
    $prefix = "__xl6_import_{0}_" -f (Sanitize-PackageName $manifest.name)

    foreach ( $source in (String-Array $manifest.artifact.xlSources) ) {
        $sourcePath = Join-Path $pkg.FullName $source
        if ( ! (Test-Path -LiteralPath $sourcePath -PathType Leaf) ) {
            continue
        }

        $text = Read-Utf8Text $sourcePath
        $matches = [regex]::Matches(
            $text,
            '(?m)^\s*(?:extern\s+"[^"]+"\s+|export\s+"[^"]+"\s+)?func\s+([A-Za-z_][A-Za-z0-9_]*)\s*\('
        )
        foreach ( $match in $matches ) {
            $name = $match.Groups[1].Value
            if ( ! $name.StartsWith($prefix, [System.StringComparison]::Ordinal) ) {
                continue
            }

            $publicName = $name.Substring($prefix.Length)
            if ( $publicName.EndsWith("_export", [System.StringComparison]::Ordinal) ) {
                continue
            }
            if ( $publicName -ne "" ) {
                $set.Add($publicName) | Out-Null
            }
        }
    }

    return $set
}

function Validate-RelativeFileList($pkg, $manifestRel, $items, $label, $extension)
{
    foreach ( $item in $items ) {
        if ( [string]::IsNullOrWhiteSpace($item) ) {
            Add-Failure "release extlib $label contains empty path: $manifestRel"
            continue
        }
        if ( [System.IO.Path]::IsPathRooted($item) ) {
            Add-Failure "release extlib $label must be relative: $manifestRel -> $item"
            continue
        }
        if ( $item -match '(^|[\\/])[.][.]([\\/]|$)' ) {
            Add-Failure "release extlib $label must not use parent traversal: $manifestRel -> $item"
            continue
        }
        if ( $extension -ne "" -and ! $item.EndsWith($extension) ) {
            Add-Failure "release extlib $label extension mismatch: $manifestRel -> $item"
        }
        Check-PathValue $pkg.Name $label $item

        $fullPath = Join-Path $pkg.FullName $item
        if ( ! (Test-Path -LiteralPath $fullPath -PathType Leaf) ) {
            Add-Failure "release extlib $label missing file: $manifestRel -> $item"
        }
    }
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
Check-NoDraftFiles $releaseDir "extlibs"
Check-NoDraftFiles $exampleDir "extlib examples"

$runnerText = Read-Utf8Text $runnerPath
$exampleFiles = @(Get-ChildItem -LiteralPath $exampleDir -File -Filter "*.xl" | Sort-Object Name)
$exampleTextByFile = @{}
$importsByPackage = @{}

foreach ( $file in $exampleFiles ) {
    if ( $file.Name -like "x_*.xl" ) {
        Add-Failure "release extlib example must not use old x_ prefix: $($file.Name)"
    }
    $text = Read-Utf8Text $file.FullName
    if ( [regex]::IsMatch($text, '(?m)^\s*if\s+false\s+then\b') ) {
        Add-Failure "release extlib example must not hide coverage behind if false: $($file.Name)"
    }
    if ( [regex]::IsMatch($text, '(?<![A-Za-z0-9_])unsupported\s*\(') ) {
        Add-Failure "release extlib example must not cover placeholder unsupported API: $($file.Name)"
    }
    $exampleTextByFile[$file.Name] = Remove-IfFalseBlocks $text
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
$manifestsByPackage = @{}
$dirsByPackage = @{}

foreach ( $pkg in $packages ) {
    $name = $pkg.Name
    $manifestPath = Join-Path $pkg.FullName "xlang.package.json"
    $manifestRel = "release/extlibs/$name/xlang.package.json"

    if ( $name.StartsWith("x.") ) {
        Add-Failure "release extlib package keeps old x. namespace prefix: $name"
    }
    if ( $name.StartsWith("x_") ) {
        Add-Failure "release extlib package keeps old x_ prefix: $name"
    }
    if ( ! (Test-Path -LiteralPath $manifestPath -PathType Leaf) ) {
        Add-Failure "release extlib manifest missing: $manifestRel"
        continue
    }

    try {
        $manifest = Read-Json $manifestPath
    } catch {
        Add-Failure "release extlib manifest is not valid JSON: $manifestRel"
        continue
    }

    $manifestsByPackage[$name] = $manifest
    $dirsByPackage[$name] = $pkg

    if ( $manifest.format -ne "xlang-package-1" ) {
        Add-Failure "release extlib manifest format mismatch: $name"
    }
    if ( $manifest.kind -ne "demo6-extlib" ) {
        Add-Failure "release extlib manifest kind mismatch: $name"
    }
    if ( [string]$manifest.name -ne $name ) {
        Add-Failure "release extlib manifest name must match directory: $name"
    }
    if ( [string]::IsNullOrWhiteSpace([string]$manifest.version) ) {
        Add-Failure "release extlib manifest version missing: $name"
    }
    if ( [string]::IsNullOrWhiteSpace([string]$manifest.abiVersion) ) {
        Add-Failure "release extlib manifest abiVersion missing: $name"
    }
    if ( $null -eq $manifest.artifact ) {
        Add-Failure "release extlib artifact missing: $name"
        continue
    }
    if ( [string]$manifest.artifact.kind -ne "source" ) {
        Add-Failure "release extlib artifact.kind must be source: $name"
    }

    $manifestXlSources = String-Array $manifest.artifact.xlSources
    $manifestCSources = String-Array $manifest.artifact.cSources
    if ( $manifestXlSources.Count -eq 0 ) {
        Add-Failure "release extlib xlSources must not be empty: $name"
    }
    Validate-RelativeFileList $pkg $manifestRel $manifestXlSources "xlSources" ".xl"
    Validate-RelativeFileList $pkg $manifestRel $manifestCSources "cSources" ".c"

    foreach ( $value in (String-Array $manifest.links) + (String-Array $manifest.loadlibs) ) {
        Check-PathValue $name "link/loadlib" $value
    }

    $expectedPackageId = "{0}@{1}#abi{2}" -f $manifest.name, $manifest.version, $manifest.abiVersion
    if ( ! $packageIds.Add($expectedPackageId) ) {
        Add-Failure "duplicate release extlib package identity: $expectedPackageId"
    }
}

foreach ( $name in $manifestsByPackage.Keys ) {
    $manifest = $manifestsByPackage[$name]
    foreach ( $dep in @($manifest.dependencies) ) {
        if ( $null -eq $dep ) {
            continue
        }
        $key = "{0}@{1}#abi{2}" -f $dep.name, $dep.version, $dep.abiVersion
        if ( ! $packageIds.Contains($key) ) {
            Add-Failure "release extlib dependency target missing: $name -> $key"
        }
    }
}

foreach ( $name in $manifestsByPackage.Keys ) {
    $manifest = $manifestsByPackage[$name]
    $pkg = $dirsByPackage[$name]

    if ( ! $importsByPackage.ContainsKey($name) ) {
        Add-Failure "release extlib has no example importing it: $name"
        continue
    }

    $exampleText = ""
    foreach ( $exampleName in $importsByPackage[$name] ) {
        if ( $exampleTextByFile.ContainsKey($exampleName) ) {
            $exampleText += "`n" + $exampleTextByFile[$exampleName]
        }
    }

    $functionNames = Get-PublicFunctionNames $pkg $manifest
    foreach ( $functionName in ($functionNames | Sort-Object) ) {
        if ( $forbiddenExportNames.Contains($functionName) ) {
            Add-Failure "release extlib must not export placeholder function: $name.$functionName"
            continue
        }
        if ( Is-CoverageExempt $name $functionName ) {
            continue
        }
        if ( ! (Text-ContainsName $exampleText $functionName) ) {
            Add-Failure "release extlib example coverage missing function: $name.$functionName"
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
