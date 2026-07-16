param()

$ErrorActionPreference = 'Stop'

$root = Split-Path $PSScriptRoot -Parent
$publicPackages = @(
    'gzip', 'bz2', 'zip', 'tar', 'csv', 'ini', 'toml', 'plist', 'netrc',
    'xml.sax', 'xml.etree', 'xml.dom'
)
$privatePackages = @('_zlib', '_bzip2', '_libzip', '_expat', '_tomlc17')

if (-not (Get-Command gcc -ErrorAction SilentlyContinue)) {
    throw 'gcc is required for the format extension library guard.'
}

function Get-PackageSourceClosure([string]$packageName) {
    $packageRoot = Join-Path $root ('release\extlibs\' + $packageName)
    $manifestPath = Join-Path $packageRoot 'xlang.package.json'
    $manifest = Get-Content -Raw -LiteralPath $manifestPath | ConvertFrom-Json
    $sourceFiles = @(Get-ChildItem -LiteralPath $packageRoot -Recurse -File |
        Where-Object { $_.Extension -in '.c', '.h' })
    $filesByName = @{}
    $queue = New-Object System.Collections.Queue
    $seen = @{}

    foreach ($file in $sourceFiles) {
        if (-not $filesByName.ContainsKey($file.Name)) {
            $filesByName[$file.Name] = @()
        }
        $filesByName[$file.Name] += $file.FullName
    }

    foreach ($relativePath in $manifest.artifact.cSources) {
        $fullPath = [IO.Path]::GetFullPath((Join-Path $packageRoot ([string]$relativePath)))
        if (-not (Test-Path -LiteralPath $fullPath)) {
            throw "$packageName manifest source is missing: $relativePath"
        }
        $queue.Enqueue($fullPath)
    }

    while ($queue.Count -gt 0) {
        $filePath = [string]$queue.Dequeue()
        if ($seen.ContainsKey($filePath)) {
            continue
        }
        $seen[$filePath] = $true

        foreach ($line in Get-Content -LiteralPath $filePath) {
            if ($line -notmatch '^\s*#\s*include\s*"([^"]+)"') {
                continue
            }
            $includePath = $Matches[1].Replace('/', [IO.Path]::DirectorySeparatorChar)
            $candidate = [IO.Path]::GetFullPath((Join-Path (Split-Path $filePath -Parent) $includePath))
            if ((Test-Path -LiteralPath $candidate) -and
                $candidate.StartsWith($packageRoot, [StringComparison]::OrdinalIgnoreCase)) {
                $queue.Enqueue($candidate)
                continue
            }

            $fileName = [IO.Path]::GetFileName($includePath)
            if ($filesByName.ContainsKey($fileName) -and $filesByName[$fileName].Count -eq 1) {
                $queue.Enqueue($filesByName[$fileName][0])
            }
        }
    }

    return @($sourceFiles | Where-Object {
        $_.Extension -eq '.c' -and -not $seen.ContainsKey($_.FullName)
    })
}

foreach ($packageName in $privatePackages) {
    $unusedSources = @(Get-PackageSourceClosure $packageName)
    if ($unusedSources.Count -ne 0) {
        $paths = ($unusedSources | ForEach-Object { $_.FullName }) -join "`n"
        throw "$packageName contains C sources outside its manifest/include closure:`n$paths"
    }
}

$includePaths = @((Join-Path $root 'lib\xrt'))
foreach ($packageName in $publicPackages) {
    $packageRoot = Join-Path $root ('release\extlibs\' + $packageName)
    $includePaths += $packageRoot
    $includePaths += Join-Path $packageRoot 'include'
}
$includePaths += Join-Path $root 'release\extlibs\_zlib\third_party\zlib'
$includePaths += Join-Path $root 'release\extlibs\_bzip2\third_party\bzip2'
$includePaths += Join-Path $root 'release\extlibs\_libzip\third_party\libzip\lib'
$includePaths += Join-Path $root 'release\extlibs\_expat\vendor\expat\lib'
$includePaths += Join-Path $root 'release\extlibs\_tomlc17\src'

$commonArguments = @(
    '-std=c11', '-D_WIN32_WINNT=0x0601', '-Wall', '-Wextra', '-Werror',
    '-fsyntax-only'
)
foreach ($includePath in $includePaths) {
    $commonArguments += '-I' + $includePath
}

$compiledCount = 0
foreach ($packageName in $publicPackages) {
    $packageRoot = Join-Path $root ('release\extlibs\' + $packageName)
    foreach ($sourceFile in Get-ChildItem -LiteralPath $packageRoot -File -Filter '*.c') {
        & gcc @commonArguments $sourceFile.FullName
        if ($LASTEXITCODE -ne 0) {
            throw "strict compile failed: $($sourceFile.FullName)"
        }
        $compiledCount++
    }
}

Write-Output "Format extlib guard ok: $compiledCount project sources, $($privatePackages.Count) private payloads."
