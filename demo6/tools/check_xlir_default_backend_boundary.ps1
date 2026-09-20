param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$shell = Join-Path $Root "src\driver\xl6_xlc_shell.c"
$api = Join-Path $Root "src\api\xl_api.c"
$header = Join-Path $Root "release\include\xlc.h"
$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\xlir_checked_integer_arithmetic_abi.xl"
$tempDir = Join-Path $Root "build\test"
$defaultOutput = Join-Path $tempDir "xlir_default_backend_default.c"
$xlirOutput = Join-Path $tempDir "xlir_default_backend_explicit.c"
$legacyOutput = Join-Path $tempDir "xlir_default_backend_legacy.c"
$matrixName = "XLIR" + ([char]0x8FC1) + ([char]0x79FB) +
    ([char]0x9A8C) + ([char]0x6536) + ([char]0x77E9) + ([char]0x9635) + ".md"
$matrix = Join-Path $Root (Join-Path "docs" $matrixName)

foreach ( $path in @($shell, $api, $header, $xl, $source, $matrix) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR default-backend boundary input is missing: $path"
    }
}

$shellText = [System.IO.File]::ReadAllText($shell, [System.Text.Encoding]::UTF8)
$apiText = [System.IO.File]::ReadAllText($api, [System.Text.Encoding]::UTF8)
$headerText = [System.IO.File]::ReadAllText($header, [System.Text.Encoding]::UTF8)
if ( ! $headerText.Contains('XLC_BACKEND_XLIR = 0') -or
     ! $headerText.Contains('XLC_BACKEND_LEGACY = 1') ) {
    throw "public compiler API must expose XLIR as the zero/default backend and legacy explicitly"
}
if ( ! $apiText.Contains('ctx->backend = XLC_BACKEND_XLIR;') -or
     ! $apiText.Contains('ctx->backend == XLC_BACKEND_LEGACY') -or
     ! $apiText.Contains('xlEmitUnitXlirCToFile(') ) {
    throw "compiler context default or selected-backend C emission routing regressed"
}
if ( ! $shellText.Contains('xlcSetBackend(ctx, XLC_BACKEND_XLIR)') -or
     ! $shellText.Contains('xlcSetBackend(ctx, XLC_BACKEND_LEGACY)') -or
     ! $shellText.Contains('[--backend legacy|xlir]') ) {
    throw "CLI must expose and apply both explicit backend selections"
}

$matrixText = [System.IO.File]::ReadAllText($matrix, [System.Text.Encoding]::UTF8)
if ( ! $matrixText.Contains('XLC_BACKEND_XLIR') -or
     ! $matrixText.Contains('--backend legacy') -or
     ! $matrixText.Contains('legacy-vs-XLIR') ) {
    throw "XLIR default-backend contract is undocumented"
}

[System.IO.Directory]::CreateDirectory($tempDir) | Out-Null
foreach ( $output in @($defaultOutput, $xlirOutput, $legacyOutput) ) {
    if ( Test-Path -LiteralPath $output ) {
        Remove-Item -LiteralPath $output -Force
    }
}

& $xl cgen $source --emit $defaultOutput
if ( $LASTEXITCODE -ne 0 ) { throw "default cgen failed" }
& $xl cgen $source --backend xlir --emit $xlirOutput
if ( $LASTEXITCODE -ne 0 ) { throw "explicit XLIR cgen failed" }
& $xl cgen $source --backend legacy --emit $legacyOutput
if ( $LASTEXITCODE -ne 0 ) { throw "explicit legacy cgen failed" }

$defaultBytes = [System.Convert]::ToBase64String(
    [System.IO.File]::ReadAllBytes($defaultOutput))
$xlirBytes = [System.Convert]::ToBase64String(
    [System.IO.File]::ReadAllBytes($xlirOutput))
$legacyBytes = [System.Convert]::ToBase64String(
    [System.IO.File]::ReadAllBytes($legacyOutput))
$defaultText = [System.IO.File]::ReadAllText($defaultOutput, [System.Text.Encoding]::UTF8)
$legacyText = [System.IO.File]::ReadAllText($legacyOutput, [System.Text.Encoding]::UTF8)
if ( $defaultBytes -cne $xlirBytes ) {
    throw "cgen without --backend must be byte-identical to explicit XLIR"
}
if ( $defaultBytes -ceq $legacyBytes -or $legacyText.Contains('xlir_module_init') ) {
    throw "explicit legacy cgen must remain a distinct differential baseline"
}
if ( ! $defaultText.Contains('xlir_module_init') -or
     ! $defaultText.Contains('xlang_main') ) {
    throw "default cgen did not emit the verified XLIR module and language entry"
}
if ( [System.Text.RegularExpressions.Regex]::IsMatch(
        $defaultText, '(?m)^\s*int\s+main\s*\(') ) {
    throw "cgen module artifacts must not own the native process main entry"
}

Write-Output "XLIR default backend boundary ok: default equals explicit XLIR, legacy remains explicit, and cgen emits an embeddable module."
exit 0
