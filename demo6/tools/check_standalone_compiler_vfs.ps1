param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$Root = (Resolve-Path -LiteralPath $Root).Path
$releaseDir = Join-Path $Root "release"
$testDir = Join-Path ([System.IO.Path]::GetTempPath()) "xlang_demo6_standalone_compiler_vfs"
$xlExe = Join-Path $releaseDir "xl.exe"
$xlcDll = Join-Path $releaseDir "xlc.dll"
$source = Join-Path $releaseDir "examples\hello.xl"
$completed = $false

foreach ( $path in @($xlExe, $xlcDll, $source) ) {
    if ( !(Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "standalone compiler VFS input missing: $path"
    }
}

if ( Test-Path -LiteralPath $testDir ) {
    $resolvedTestDir = (Resolve-Path -LiteralPath $testDir).Path
    $expectedTestDir = [System.IO.Path]::GetFullPath($testDir).TrimEnd(
        [System.IO.Path]::DirectorySeparatorChar,
        [System.IO.Path]::AltDirectorySeparatorChar)
    if ( ![string]::Equals($resolvedTestDir.TrimEnd(
                              [System.IO.Path]::DirectorySeparatorChar,
                              [System.IO.Path]::AltDirectorySeparatorChar),
                          $expectedTestDir,
                          [System.StringComparison]::OrdinalIgnoreCase) ) {
        throw "standalone compiler VFS cleanup refused: $resolvedTestDir"
    }
    Remove-Item -LiteralPath $resolvedTestDir -Recurse -Force
}
New-Item -ItemType Directory -Path $testDir | Out-Null

Copy-Item -LiteralPath $xlExe -Destination (Join-Path $testDir "xl.exe")
Copy-Item -LiteralPath $xlcDll -Destination (Join-Path $testDir "xlc.dll")
Copy-Item -LiteralPath $source -Destination (Join-Path $testDir "hello.xl")

$savedLocation = Get-Location
try {
    Set-Location -LiteralPath $testDir
    $buildOutput = & ".\xl.exe" build "hello.xl" 2>&1
    if ( $LASTEXITCODE -ne 0 ) {
        throw "standalone xl build failed: $([string]::Join([Environment]::NewLine, @($buildOutput)))"
    }
    if ( !(Test-Path -LiteralPath ".\hello.exe" -PathType Leaf) ) {
        throw "standalone xl build did not create hello.exe"
    }
    $runOutput = & ".\hello.exe" 2>&1
    if ( $LASTEXITCODE -ne 0 ) {
        throw "standalone hello.exe failed: $([string]::Join([Environment]::NewLine, @($runOutput)))"
    }
    $completed = $true
} finally {
    Set-Location -LiteralPath $savedLocation
}

if ( $completed ) {
    Remove-Item -LiteralPath $testDir -Recurse -Force
}

Write-Host "Standalone compiler VFS ok: xl.exe + xlc.dll built hello.xl without repository files."
