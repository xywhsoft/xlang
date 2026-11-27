param(
    [string]$Root = "",
    [string]$StartAt = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\win_x64\xl.exe"
$exampleDir = Join-Path $Root "release\win_x64\examples\extlibs"
$legacyBatch = Join-Path $exampleDir "run_extlibs.bat"

if ( ! (Test-Path -LiteralPath $xl -PathType Leaf) ) {
    Write-Host "xl executable missing: $xl"
    exit 1
}

$legacyText = [System.IO.File]::ReadAllText($legacyBatch, [System.Text.Encoding]::UTF8)

# Wires: release/win_x64/examples/extlibs/*/*.xl
$examples = @(Get-ChildItem -LiteralPath $exampleDir -File -Filter "*.xl" -Recurse | Sort-Object FullName)
$started = $StartAt -eq ""
$runCount = 0
foreach ( $example in $examples ) {
    if ( $example.Name -eq "com.xl" -or $legacyText.Contains($example.Name) ) {
        continue
    }
    if ( ! $started ) {
        if ( $example.Name -ne $StartAt ) {
            continue
        }
        $started = $true
    }

    Write-Host "Running extension-library example: $($example.Name)"
    & $xl check $example.FullName
    if ( $LASTEXITCODE -ne 0 ) {
        exit $LASTEXITCODE
    }
    & $xl run $example.FullName
    if ( $LASTEXITCODE -ne 0 ) {
        exit $LASTEXITCODE
    }
    $runCount++
}

if ( ! $started ) {
    Write-Host "Extension-library start example not found: $StartAt"
    exit 1
}

Write-Host "Extension-library catalog examples ok: $runCount files."
exit 0
