param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$exampleDir = Join-Path $Root "examples"
$minStdlibExampleFiles = 17

if ( !(Test-Path $xl) ) {
    Write-Host "release\xl.exe not found. Run build.bat before checking examples."
    exit 1
}

$examples = @(Get-ChildItem -Path $exampleDir -Filter "stdlib_*.xl" -File | Sort-Object Name)

if ( $examples.Count -eq 0 ) {
    Write-Host "No standard library examples found."
    exit 1
}

if ( $examples.Count -lt $minStdlibExampleFiles ) {
    Write-Host "Standard library example baseline regressed: $($examples.Count) < $minStdlibExampleFiles"
    exit 1
}

Push-Location $Root
try {
    foreach ( $example in $examples ) {
        Write-Host "CHECK example $($example.Name)"
        & $xl check $example.FullName
        if ( $LASTEXITCODE -ne 0 ) {
            exit $LASTEXITCODE
        }

        Write-Host "RUN example $($example.Name)"
        & $xl run $example.FullName
        if ( $LASTEXITCODE -ne 0 ) {
            exit $LASTEXITCODE
        }
    }
}
finally {
    Pop-Location
}

Write-Host "Standard library examples check/run ok: $($examples.Count) files."
exit 0
