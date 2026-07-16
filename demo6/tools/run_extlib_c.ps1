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
        $path = Join-Path $packageDir ([string]$item)
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "Extension package C source not found: $path"
        }
        $sources.Add($path)
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

# Some standalone C tests include their implementation units at the end.
# Appending package sources again would give TCC duplicate definitions.
$sourceText = Get-Content -LiteralPath $sourcePath -Raw -Encoding UTF8
if ($sourceText -match '(?m)^\s*#include\s+"[^"]+\.c"\s*$') {
    & $xl run $sourcePath
    exit $LASTEXITCODE
}

foreach ($name in ($Package -split ',')) {
    $trimmed = $name.Trim()
    if ($trimmed.Length -ne 0) {
        Add-PackageSources $trimmed
    }
}
& $xl run $sourcePath @sources
exit $LASTEXITCODE
