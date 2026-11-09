param(
    [string]$Root = "",
    [string]$RuntimeDirectory = "",
    [string]$Example = "",
    [switch]$Run,
    [ValidateRange(0,10000)][int]$Frames = 0
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$runtime = if ($RuntimeDirectory) { [IO.Path]::GetFullPath($RuntimeDirectory) } else { Join-Path $Root "release\win_x64" }
$xl = Join-Path $runtime "xl.exe"
$exampleDir = Join-Path $runtime "examples\xui"

if ( ! (Test-Path -LiteralPath $xl -PathType Leaf) ) {
    Write-Host "xl executable missing: $xl"
    exit 1
}

# Checks do not execute interactive examples.
# NOTE: keep this file ASCII-only. Windows PowerShell 5.1 reads BOM-less
# scripts as ANSI/GBK; UTF-8 comment bytes can swallow the next newline
# and silently merge statements into comment lines.
$examples = @(Get-ChildItem -LiteralPath $exampleDir -File -Filter "*.xl" -Recurse | Where-Object { !$_.Name.StartsWith("_") } | Sort-Object FullName)
if ($Example) {
    $examples = @($examples | Where-Object { $_.BaseName -eq $Example })
}
if ( $examples.Count -eq 0 ) {
    Write-Host "No XUI examples found in $exampleDir"
    exit 1
}

$checkFail = 0
$checked = 0
$previousPackages = $env:XLANG_PACKAGE_PATH
$previousFrames = $env:XUI_EXAMPLE_FRAMES
try {
    $env:XLANG_PACKAGE_PATH = Join-Path $runtime "extlibs"
    if ($Frames -gt 0) { $env:XUI_EXAMPLE_FRAMES = [string]$Frames }
    else { Remove-Item Env:XUI_EXAMPLE_FRAMES -ErrorAction SilentlyContinue }
    Push-Location -LiteralPath $runtime
    try {
        foreach ( $exampleFile in $examples ) {
            $mode = if ($Run) { "run" } else { "check" }
            Write-Host "${mode}: $($exampleFile.Name)"
            & $xl $mode $exampleFile.FullName
            if ( $LASTEXITCODE -ne 0 ) { $checkFail += 1 }
            $checked += 1
        }
    } finally { Pop-Location }
} finally {
    $env:XLANG_PACKAGE_PATH = $previousPackages
    $env:XUI_EXAMPLE_FRAMES = $previousFrames
}

Write-Host ""
Write-Host "XUI examples: $($examples.Count) files, checked: $checked, check failures: $checkFail"

if ( $checkFail -gt 0 ) {
    exit 1
}
Write-Host "XUI examples check ok."
exit 0
