param(
    [string]$Root = "",
    [string]$RuntimeDirectory = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$runtime = if ($RuntimeDirectory) { [IO.Path]::GetFullPath($RuntimeDirectory) } else { Join-Path $Root "release\win_x64" }
$xl = Join-Path $runtime "xl.exe"
$exampleDir = Join-Path $runtime "examples\xge"

if ( ! (Test-Path -LiteralPath $xl -PathType Leaf) ) {
    Write-Host "xl executable missing: $xl"
    exit 1
}

# Check declarations only. Interactive examples are never executed here.
$examples = @(Get-ChildItem -LiteralPath $exampleDir -File -Filter "*.xl" | Sort-Object Name)
if ($examples.Count -eq 0) { throw "No maintained XGE examples found in $exampleDir" }
foreach ( $example in $examples ) {
    Write-Host "Checking XGE example: $($example.Name)"
    & $xl check $example.FullName
    if ( $LASTEXITCODE -ne 0 ) {
        exit $LASTEXITCODE
    }
}

Write-Host "XGE examples check ok: $($examples.Count) files."
exit 0
