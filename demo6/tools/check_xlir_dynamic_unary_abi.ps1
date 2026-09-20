param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"
if ( $Root -eq "" ) { $Root = Split-Path -Parent $PSScriptRoot }

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\xlir_dynamic_unary_abi.xl"
$hostSource = Join-Path $Root "test\cases\xlir_default_class_layout_host.c"
$legacySupport = Join-Path $Root "test\cases\xlir_cgen_differential_expect_i64_legacy_support.c"
$outputDir = Join-Path $Root "build\test"
$loweredPath = Join-Path $outputDir "xlir_dynamic_unary_abi.lowered.json"
$generatedPath = Join-Path $outputDir "xlir_dynamic_unary_abi_generated.c"
$generatedAgainPath = Join-Path $outputDir "xlir_dynamic_unary_abi_generated_again.c"
$legacyPath = Join-Path $outputDir "xlir_dynamic_unary_abi_legacy.c"

foreach ( $path in @($xl, $source, $hostSource, $legacySupport) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR dynamic unary gate input is missing: $path"
    }
}
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

function Invoke-Xl([string[]]$Arguments, [string]$Failure) {
    & $xl @Arguments
    if ( $LASTEXITCODE -ne 0 ) { throw $Failure }
}

Invoke-Xl @("ir", $source, "--lowered", "--format", "json", "--emit", $loweredPath) `
    "XLIR dynamic unary lowering failed"
$lowered = [System.IO.File]::ReadAllText($loweredPath, [System.Text.Encoding]::UTF8) |
    ConvertFrom-Json
$operations = @($lowered.instructions | Where-Object {
    $_.runtimeOpName -in @(
        "xvalue-unary-pos", "xvalue-unary-neg", "xvalue-unary-bit-not")
})
if ( $operations.Count -ne 3 ) {
    throw "Lowered XLIR did not preserve all three dynamic unary operations"
}
foreach ( $name in @("xvalue-unary-pos", "xvalue-unary-neg", "xvalue-unary-bit-not") ) {
    $operation = @($operations | Where-Object { $_.runtimeOpName -eq $name })
    if ( $operation.Count -ne 1 -or $operation[0].operandCount -ne 1 -or
         $operation[0].type -eq 0 -or $operation[0].result -eq 0 ) {
        throw "Dynamic unary operation has no exact borrowed xvalue/owned result ABI: $name"
    }
}
if ( @($lowered.instructions | Where-Object {
        $_.kindName -eq "unary" -and $_.origin -in @($operations.origin)
    }).Count -ne 0 ) {
    throw "Dynamic unary semantics leaked into a physical UNARY instruction"
}

Invoke-Xl @("cgen", $source, "--backend", "xlir", "--emit", $generatedPath) `
    "XLIR dynamic unary Cgen failed"
Invoke-Xl @("cgen", $source, "--backend", "xlir", "--emit", $generatedAgainPath) `
    "XLIR dynamic unary deterministic Cgen failed"
if ( ! [System.Linq.Enumerable]::SequenceEqual(
        [byte[]][System.IO.File]::ReadAllBytes($generatedPath),
        [byte[]][System.IO.File]::ReadAllBytes($generatedAgainPath)
    ) ) {
    throw "XLIR dynamic unary C output is not deterministic"
}
$generated = [System.IO.File]::ReadAllText($generatedPath, [System.Text.Encoding]::UTF8)
foreach ( $required in @("xlir_xvalue_unary(", "INT64_MIN", "xrtValueInt(~v)") ) {
    if ( ! $generated.Contains($required) ) {
        throw "XLIR dynamic unary C emission is incomplete: $required"
    }
}

Push-Location $Root
try {
    $xlirCommand = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include $generatedPath $hostSource !XL6_CGEN_LINKS! -o build\test\xlir_dynamic_unary_abi_xlir.exe && build\test\xlir_dynamic_unary_abi_xlir.exe"
    & $env:ComSpec /d /v:on /c $xlirCommand
    if ( $LASTEXITCODE -ne 0 ) { throw "XLIR dynamic unary strict-C runtime failed" }

    Invoke-Xl @("cgen", $source, "--backend", "legacy", "--emit", $legacyPath) `
        "Legacy dynamic unary differential Cgen failed"
    $legacyCommand = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror $legacyPath $legacySupport !XL6_CGEN_XLANG_LINKS! -o build\test\xlir_dynamic_unary_abi_legacy.exe && build\test\xlir_dynamic_unary_abi_legacy.exe"
    & $env:ComSpec /d /v:on /c $legacyCommand
    if ( $LASTEXITCODE -ne 0 ) { throw "Legacy dynamic unary differential runtime failed" }
} finally {
    Pop-Location
}

Write-Output "XLIR dynamic unary ABI gate ok: three explicit owned-xvalue operations, no ambiguous UNARY fallback, deterministic strict C and legacy differential runtime semantics."
