param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$exampleDir = Join-Path $Root "examples"

if ( !(Test-Path $xl) ) {
    Write-Host "release\xl.exe not found. Run build.bat before checking examples."
    exit 1
}

$examples = Get-ChildItem -Path $exampleDir -Filter "stdlib_*.xl" -File | Sort-Object Name

foreach ( $example in $examples ) {
    Write-Host "CHECK example $($example.Name)"
    & $xl check $example.FullName
    if ( $LASTEXITCODE -ne 0 ) {
        exit $LASTEXITCODE
    }
}

Write-Host "Standard library examples check ok: $($examples.Count) files."
exit 0
