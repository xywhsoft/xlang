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
$dynamicRunnerPath = Join-Path $Root "tools\run_extlib_examples.ps1"
$expectedMinPackages = 31
$fixtureNames = @(
    "class_api",
    "class_inherit_api",
    "counter",
    "dep_value",
    "dep_value@2.0.0",
    "include_dir_pkg",
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
    "packages",
    "packages",
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
    @{ Name = "absolute drive path"; Regex = "(?<![A-Za-z0-9_%+])[A-Za-z]:[\\/]" },
    @{ Name = "legacy release/libs path"; Regex = "release[\\/]libs" },
    @{ Name = "legacy demo5 path"; Regex = "demo5" },
    @{ Name = "host absolute path"; Regex = "(^|[^A-Za-z0-9_])/(home|(?-i:Users))/" }
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

function Validate-XrtModules($packageName, $manifestPath, $modules)
{
    foreach ( $module in $modules ) {
        if ( $module -notmatch '^(XRT_MODULE_|XRUNTIME_MODULE_)[A-Z0-9_]+$' ) {
            Add-Failure "release extlib $packageName has invalid xrtModules root: $manifestPath -> $module"
        }
    }
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
    if ( $file.FullName.IndexOf("\lib\", [System.StringComparison]::OrdinalIgnoreCase) -ge 0 ) {
        return $false
    }
    if ( $file.Name.Equals("README.md", [System.StringComparison]::OrdinalIgnoreCase) -and
         $file.Directory.Name.StartsWith("_", [System.StringComparison]::Ordinal) ) {
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
        $isTestFixture = $relative -match '^release[\\/]extlibs[\\/][^\\/]+[\\/]test[\\/]'
        $lines = [System.IO.File]::ReadAllLines($file.FullName, [System.Text.Encoding]::UTF8)
        for ( $i = 0; $i -lt $lines.Count; ++$i ) {
            foreach ( $pattern in $script:forbiddenTextPatterns ) {
                # 测试夹具可以包含待验证的绝对路径样本，其他发布内容仍禁止写死盘符。
                if ( $isTestFixture -and $pattern.Name -eq "absolute drive path" ) { continue }
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

function Validate-RelativeDirectoryList($pkg, $manifestRel, $items, $label)
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
        Check-PathValue $pkg.Name $label $item

        $fullPath = Join-Path $pkg.FullName $item
        if ( ! (Test-Path -LiteralPath $fullPath -PathType Container) ) {
            Add-Failure "release extlib $label missing directory: $manifestRel -> $item"
        }
    }
}
if ( ! (Test-Path -LiteralPath $dynamicRunnerPath -PathType Leaf) ) {
    Write-Host "release extlib dynamic runner missing: $dynamicRunnerPath"
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
$dynamicRunnerText = Read-Utf8Text $dynamicRunnerPath
$hasDynamicRunner =
    $runnerText.Contains("tools\run_extlib_examples.ps1") -and
    $dynamicRunnerText.Contains("Wires: release/examples/extlibs/*.xl")
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
    foreach ( $match in [regex]::Matches($text, '(?m)^\s*(?:import|from)\s+([A-Za-z0-9_.]+)\b') ) {
        $importName = $match.Groups[1].Value
        if ( ! $importsByPackage.ContainsKey($importName) ) {
            $importsByPackage[$importName] = New-Object System.Collections.Generic.List[string]
        }
        $importsByPackage[$importName].Add($file.Name) | Out-Null
    }
    if ( ! $runnerText.Contains("release\examples\extlibs\$($file.Name)") -and
         ! $runnerText.Contains("release/examples/extlibs/$($file.Name)") -and
         ! $hasDynamicRunner ) {
        Add-Failure "release extlib example is not executed by run_extlibs.bat: $($file.Name)"
    }
}

$packageTestFiles = @(
    Get-ChildItem -LiteralPath $releaseDir -File -Recurse -Filter "*.xl" |
        Where-Object {
            $_.FullName.IndexOf("\test\", [System.StringComparison]::OrdinalIgnoreCase) -ge 0
        } |
        Sort-Object FullName
)
foreach ( $file in $packageTestFiles ) {
    $relative = $file.FullName.Substring($releaseDir.Length + 1).Replace("\", "/")
    $text = Read-Utf8Text $file.FullName
    $exampleTextByFile[$relative] = Remove-IfFalseBlocks $text
    foreach ( $match in [regex]::Matches($text, '(?m)^\s*(?:import|from)\s+([A-Za-z0-9_.]+)\b') ) {
        $importName = $match.Groups[1].Value
        if ( ! $importsByPackage.ContainsKey($importName) ) {
            $importsByPackage[$importName] = New-Object System.Collections.Generic.List[string]
        }
        $importsByPackage[$importName].Add($relative) | Out-Null
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

    $isPrivate = [string]$manifest.kind -eq "demo6-private-extlib"

    if ( $manifest.format -ne "xlang-package-1" ) {
        Add-Failure "release extlib manifest format mismatch: $name"
    }
    if ( ! $isPrivate -and $manifest.kind -ne "demo6-extlib" ) {
        Add-Failure "release extlib manifest kind mismatch: $name"
    }
    if ( $isPrivate -and ! $name.StartsWith("_", [System.StringComparison]::Ordinal) ) {
        Add-Failure "private release extlib package name must start with underscore: $name"
    }
    if ( ! $isPrivate -and $name.StartsWith("_", [System.StringComparison]::Ordinal) ) {
        Add-Failure "underscore release extlib package must use private manifest kind: $name"
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
    $manifestIncludeDirs = String-Array $manifest.artifact.includeDirs
    $manifestXrtModules = String-Array $manifest.artifact.xrtModules
    if ( ! $isPrivate -and $manifestXlSources.Count -eq 0 ) {
        Add-Failure "release extlib xlSources must not be empty: $name"
    }
    Validate-RelativeFileList $pkg $manifestRel $manifestXlSources "xlSources" ".xl"
    Validate-RelativeFileList $pkg $manifestRel $manifestCSources "cSources" ".c"
    Validate-RelativeDirectoryList $pkg $manifestRel $manifestIncludeDirs "includeDirs"
    Validate-XrtModules $name $manifestRel $manifestXrtModules

    $coverageMode = [string]$manifest.coverage.mode
    if ( $coverageMode -ne "" -and $coverageMode -ne "suite" ) {
        Add-Failure "release extlib coverage.mode must be suite when present: $name"
    }
    if ( $coverageMode -eq "suite" ) {
        $coverageTests = String-Array $manifest.coverage.tests
        if ( $coverageTests.Count -eq 0 ) {
            Add-Failure "release extlib suite coverage must list tests: $name"
        }
        Validate-RelativeFileList $pkg $manifestRel $coverageTests "coverage.tests" ".xl"
    }

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

foreach ( $importName in $importsByPackage.Keys ) {
    if ( ! $manifestsByPackage.ContainsKey($importName) ) {
        Add-Failure "release extlib example or package test imports missing package: $importName"
    }
}

foreach ( $name in $manifestsByPackage.Keys ) {
    $manifest = $manifestsByPackage[$name]
    $pkg = $dirsByPackage[$name]

    if ( [string]$manifest.kind -eq "demo6-private-extlib" ) {
        continue
    }

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

    if ( [string]$manifest.coverage.mode -eq "suite" ) {
        continue
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
