param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"
if ( $Root -eq "" ) { $Root = Split-Path -Parent $PSScriptRoot }

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\stdlib_math.xl"
$hostSource = Join-Path $Root "test\cases\xlir_default_class_layout_host.c"
$legacySupport = Join-Path $Root "test\cases\xlir_cgen_differential_expect_i64_legacy_support.c"
$outputDir = Join-Path $Root "build\test"
$loweredPath = Join-Path $outputDir "xlir_special_f64_constant_abi.lowered.json"
$generatedPath = Join-Path $outputDir "xlir_special_f64_constant_abi_generated.c"
$generatedAgainPath = Join-Path $outputDir "xlir_special_f64_constant_abi_generated_again.c"
$legacyPath = Join-Path $outputDir "xlir_special_f64_constant_abi_legacy.c"

foreach ( $path in @($xl, $source, $hostSource, $legacySupport) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR special f64 constant ABI gate input is missing: $path"
    }
}
[System.IO.Directory]::CreateDirectory($outputDir) | Out-Null

function Invoke-Xl([string[]]$Arguments, [string]$Failure) {
    & $xl @Arguments
    if ( $LASTEXITCODE -ne 0 ) { throw $Failure }
}

Invoke-Xl @("ir", $source, "--lowered", "--format", "json", "--emit", $loweredPath) `
    "XLIR special f64 constant lowering failed"
$lowered = [System.IO.File]::ReadAllText(
    $loweredPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$operations = @($lowered.instructions | Where-Object {
    $_.runtimeOpName -in @("f64-nan", "f64-positive-infinity")
})
if ( $operations.Count -ne 3 -or
     @($operations | Where-Object { $_.runtimeOpName -eq "f64-nan" }).Count -ne 1 -or
     @($operations | Where-Object {
        $_.runtimeOpName -eq "f64-positive-infinity"
     }).Count -ne 2 ) {
    throw "Lowered XLIR special f64 constant partition is not canonical"
}
foreach ( $operation in $operations ) {
    $type = @($lowered.types | Where-Object { $_.id -eq $operation.type })
    $origin = @($lowered.nodes | Where-Object { $_.id -eq $operation.origin })
    if ( $operation.kindName -ne "runtime-call" -or
         $operation.operandCount -ne 0 -or $operation.result -eq 0 -or
         $operation.hasImmediate -ne 0 -or
         $type.Count -ne 1 -or $type[0].name -ne "float" -or
         $type[0].cAbi -ne 4 -or $type[0].ownershipAbi -ne 1 -or
         $origin.Count -ne 1 -or $origin[0].kindName -ne "literal" -or
         $origin[0].sourceKindName -ne "float" -or
         $origin[0].line -notin @(28, 29, 31) ) {
        throw "Special f64 constant has no exact zero-operand trivial Typed ABI"
    }
}
if ( @($lowered.instructions | Where-Object {
        $_.kindName -eq "const" -and $_.origin -in @($operations.origin)
    }).Count -ne 0 ) {
    throw "Special f64 semantics leaked into an arbitrary CONST C payload"
}

Invoke-Xl @("cgen", $source, "--backend", "xlir", "--emit", $generatedPath) `
    "XLIR special f64 constant Cgen failed"
Invoke-Xl @("cgen", $source, "--backend", "xlir", "--emit", $generatedAgainPath) `
    "XLIR special f64 constant deterministic Cgen failed"
if ( ! [System.Linq.Enumerable]::SequenceEqual(
        [byte[]][System.IO.File]::ReadAllBytes($generatedPath),
        [byte[]][System.IO.File]::ReadAllBytes($generatedAgainPath)) ) {
    throw "XLIR special f64 constant generated C is not byte-for-byte deterministic"
}
$generated = [System.IO.File]::ReadAllText(
    $generatedPath, [System.Text.Encoding]::UTF8)
if ( ! $generated.Contains("memcpy(&value, &bits, sizeof(value))") -or
     ! $generated.Contains("DBL_MANT_DIG == 53") -or
     ([regex]::Matches($generated, 'xlir_f64_from_bits\(UINT64_C\(0x7ff8000000000000\)\)').Count) -ne 1 -or
     ([regex]::Matches($generated, 'xlir_f64_from_bits\(UINT64_C\(0x7ff0000000000000\)\)').Count) -ne 2 -or
     $generated -match 'strtod\("(?:NAN|INFINITY)"' ) {
    throw "XLIR special f64 constants must use exact binary64 bits without CRT text parsing"
}

Push-Location $Root
try {
    # Strict native C alone cannot detect Windows TCC's older CRT strtod behavior.
    Invoke-Xl @("run", $source, "test\cases\cgen_class_host.c") `
        "Special f64 CLI run failed"
    $cliExe = Join-Path $outputDir "xlir_special_f64_constant_cli.exe"
    Invoke-Xl @("build", $source, "test\cases\cgen_class_host.c", "-o", $cliExe) `
        "Special f64 CLI build failed"
    & $cliExe
    if ($LASTEXITCODE -ne 0) { throw "Special f64 CLI executable failed" }
    $xlirCommand = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include $generatedPath $hostSource !XL6_CGEN_LINKS! -o build\test\xlir_special_f64_constant_abi_xlir.exe && build\test\xlir_special_f64_constant_abi_xlir.exe"
    & $env:ComSpec /d /v:on /c $xlirCommand
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR special f64 constant strict-C/runtime gate failed"
    }

    Invoke-Xl @("cgen", $source, "--backend", "legacy", "--emit", $legacyPath) `
        "Legacy special f64 constant differential Cgen failed"
    $legacyCommand = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror $legacyPath $legacySupport !XL6_CGEN_XLANG_LINKS! -o build\test\xlir_special_f64_constant_abi_legacy.exe && build\test\xlir_special_f64_constant_abi_legacy.exe"
    & $env:ComSpec /d /v:on /c $legacyCommand
    if ( $LASTEXITCODE -ne 0 ) {
        throw "Legacy special f64 constant differential runtime failed"
    }
} finally {
    Pop-Location
}

Write-Output "XLIR special f64 constant ABI gate passed: exact zero-operand f64-nan/f64-positive-infinity identities, no arbitrary CONST payload, binary64 bit construction, CLI run/build, deterministic strict C and legacy differential stdlib math semantics."
