param(
    [Parameter(Mandatory = $true)]
    [string]$Package,

    [Parameter(Mandatory = $true)]
    [string]$Source
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$xl = Join-Path $root "release\xl.exe"
$packagesRoot = Join-Path $root "release\extlibs"
$visited = @{}
$sources = [System.Collections.Generic.List[string]]::new()
$links = [System.Collections.Generic.List[string]]::new()
$includeDirs = [System.Collections.Generic.List[string]]::new()
$rootPackages = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)

foreach ($name in ($Package -split ',')) {
    $trimmed = $name.Trim()
    if ($trimmed.Length -ne 0) {
        $rootPackages.Add($trimmed) | Out-Null
    }
}

function Add-PackageSources([string]$name) {
    if ($visited.ContainsKey($name)) {
        return
    }
    $visited[$name] = $true

    $packageDir = Join-Path $packagesRoot $name
    $manifestPath = Join-Path $packageDir "xlang.package.json"
    if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) {
        throw "Extension package manifest not found: $manifestPath"
    }
    $manifest = Get-Content -LiteralPath $manifestPath -Raw -Encoding UTF8 | ConvertFrom-Json
    foreach ($dependency in @($manifest.dependencies)) {
        Add-PackageSources ([string]$dependency.name)
    }
    foreach ($item in @($manifest.artifact.cSources)) {
        $relativePath = ([string]$item).Replace('\', '/')

        # 依赖包的语言桥由 XLang import 负责，C smoke 只链接其可复用 C 实现。
        $isRootPackage = $rootPackages.Contains($name)
        $isDependency = -not $isRootPackage
        $isLanguageSource = $relativePath.StartsWith("src/xlang/", [System.StringComparison]::OrdinalIgnoreCase)
        if ($isDependency -and $isLanguageSource) {
            continue
        }
        $path = Join-Path $packageDir $relativePath
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "Extension package C source not found: $path"
        }
        $sources.Add($path)
    }
    foreach ($item in @($manifest.artifact.includeDirs)) {
        $path = Join-Path $packageDir ([string]$item)
        if (-not (Test-Path -LiteralPath $path -PathType Container)) {
            throw "Extension package include directory not found: $path"
        }
        $resolved = (Resolve-Path -LiteralPath $path).Path
        if (-not $includeDirs.Contains($resolved)) {
            $includeDirs.Add($resolved)
        }
    }
    foreach ($item in @($manifest.links)) {
        $link = ([string]$item).Trim()
        if ($link.Length -ne 0 -and -not $links.Contains($link)) {
            $links.Add($link)
        }
    }
}

if (-not (Test-Path -LiteralPath $xl -PathType Leaf)) {
    throw "xl.exe not found: $xl"
}
$sourcePath = if ([System.IO.Path]::IsPathRooted($Source)) {
    $Source
} else {
    Join-Path $root $Source
}
if (-not (Test-Path -LiteralPath $sourcePath -PathType Leaf)) {
    throw "C source not found: $sourcePath"
}

# Standalone tests may include part or all of their implementation units.
# Only suppress manifest sources that are already explicitly included; a test
# that embeds one bridge file can still need other package sources (SQLite).
$sourceText = Get-Content -LiteralPath $sourcePath -Raw -Encoding UTF8
foreach ($name in ($Package -split ',')) {
    $trimmed = $name.Trim()
    if ($trimmed.Length -ne 0) {
        Add-PackageSources $trimmed
    }
}

$includedSources = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
foreach ($match in [regex]::Matches($sourceText, '(?m)^\s*#include\s+"([^"]+\.c)"\s*$')) {
    $includePath = Join-Path (Split-Path -Parent $sourcePath) $match.Groups[1].Value
    if (Test-Path -LiteralPath $includePath -PathType Leaf) {
        $includedSources.Add((Resolve-Path -LiteralPath $includePath).Path) | Out-Null
    }
}
$runSources = [System.Collections.Generic.List[string]]::new()
foreach ($packageSource in $sources) {
    $resolvedSource = (Resolve-Path -LiteralPath $packageSource).Path
    if (-not $includedSources.Contains($resolvedSource)) {
        $runSources.Add($packageSource)
    }
}
$linkArgs = @()
foreach ($link in $links) { $linkArgs += @("--link", $link) }
$includeArgs = @("--inc-path", (Join-Path $root "lib/xrt"))
foreach ($includeDir in $includeDirs) {
    $includeArgs += @("--inc-path", $includeDir)
}
& $xl run $sourcePath @runSources @includeArgs @linkArgs
exit $LASTEXITCODE
