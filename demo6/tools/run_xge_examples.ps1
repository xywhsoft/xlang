param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$exampleDir = Join-Path $Root "release\examples\xge"

if ( ! (Test-Path -LiteralPath $xl -PathType Leaf) ) {
    Write-Host "xl executable missing: $xl"
    exit 1
}

# Wires: release/examples/xge/*.xl
$examples = @(Get-ChildItem -LiteralPath $exampleDir -File -Filter "*.xl" | Sort-Object Name)
foreach ( $example in $examples ) {
    Write-Host "Checking XGE example: $($example.Name)"
    & $xl check $example.FullName
    if ( $LASTEXITCODE -ne 0 ) {
        exit $LASTEXITCODE
    }
}

Write-Host "XGE examples check ok: $($examples.Count) files."
exit 0
