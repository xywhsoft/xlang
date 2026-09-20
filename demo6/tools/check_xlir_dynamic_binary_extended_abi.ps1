param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"
if ( $Root -eq "" ) { $Root = Split-Path -Parent $PSScriptRoot }

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\xlir_dynamic_binary_extended_abi.xl"
$hostSource = Join-Path $Root "test\cases\xlir_default_class_layout_host.c"
$legacySupport = Join-Path $Root "test\cases\xlir_cgen_differential_expect_i64_legacy_support.c"
$outputDir = Join-Path $Root "build\test"
$loweredPath = Join-Path $outputDir "xlir_dynamic_binary_extended_abi.lowered.json"
$generatedPath = Join-Path $outputDir "xlir_dynamic_binary_extended_abi_generated.c"
$generatedAgainPath = Join-Path $outputDir "xlir_dynamic_binary_extended_abi_generated_again.c"
$legacyPath = Join-Path $outputDir "xlir_dynamic_binary_extended_abi_legacy.c"

foreach ( $path in @($xl, $source, $hostSource, $legacySupport) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR extended dynamic binary gate input is missing: $path"
    }
}
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

function Invoke-Xl([string[]]$Arguments, [string]$Failure) {
    & $xl @Arguments
    if ( $LASTEXITCODE -ne 0 ) { throw $Failure }
}

Invoke-Xl @("ir", $source, "--lowered", "--format", "json", "--emit", $loweredPath) `
    "XLIR extended dynamic binary lowering failed"
$lowered = [System.IO.File]::ReadAllText($loweredPath, [System.Text.Encoding]::UTF8) |
    ConvertFrom-Json
$expectedCounts = [ordered]@{
    "xvalue-binary-div" = 2
    "xvalue-binary-mod" = 3
    "xvalue-binary-bit-and" = 1
    "xvalue-binary-bit-or" = 1
    "xvalue-binary-bit-xor" = 1
    "xvalue-binary-shl" = 3
    "xvalue-binary-shr" = 1
    "xvalue-binary-add" = 2
}
$operationNames = @($expectedCounts.Keys)
$operations = @($lowered.instructions | Where-Object {
    $_.runtimeOpName -in $operationNames
})
if ( $operations.Count -ne 14 ) {
    throw "Lowered XLIR did not preserve all fourteen extended dynamic binary operations"
}
foreach ( $entry in $expectedCounts.GetEnumerator() ) {
    $matching = @($operations | Where-Object { $_.runtimeOpName -eq $entry.Key })
    if ( $matching.Count -ne $entry.Value ) {
        throw "Extended dynamic binary operation count is not canonical: $($entry.Key)"
    }
    foreach ( $operation in $matching ) {
        if ( $operation.operandCount -ne 2 -or $operation.type -eq 0 -or
             $operation.result -eq 0 ) {
            throw "Extended dynamic binary operation has no exact two-borrow/owned-result ABI: $($entry.Key)"
        }
    }
}
$checkedNames = @(
    "xvalue-binary-div", "xvalue-binary-mod",
    "xvalue-binary-shl", "xvalue-binary-shr")
$checkedOperations = @($operations | Where-Object {
    $_.runtimeOpName -in $checkedNames
})
if ( $checkedOperations.Count -ne 9 ) {
    throw "Extended dynamic binary checked-effect partition is incomplete"
}
foreach ( $operation in $checkedOperations ) {
    $origin = @($lowered.nodes | Where-Object { $_.id -eq $operation.origin })
    $next = @($lowered.instructions | Where-Object {
        $_.index -eq $operation.index + 1
    })
    if ( $origin.Count -ne 1 -or
         $origin[0].operatorErrorAbiName -ne "may-raise" -or
         $next.Count -ne 1 -or $next[0].kindName -ne "error-test" -or
         $next[0].operandCount -ne 1 -or
         $next[0].operands[0] -ne $operation.result ) {
        throw "Checked dynamic binary operation has no Typed effect and adjacent ERROR_TEST ABI: $($operation.runtimeOpName)"
    }
}
$noRaiseOperations = @($operations | Where-Object {
    $_.runtimeOpName -notin $checkedNames
})
foreach ( $operation in $noRaiseOperations ) {
    $origin = @($lowered.nodes | Where-Object { $_.id -eq $operation.origin })
    if ( $origin.Count -ne 1 -or
         $origin[0].operatorErrorAbiName -ne "no-raise" ) {
        throw "Non-raising dynamic binary operation has no exact Typed effect ABI: $($operation.runtimeOpName)"
    }
}
if ( @($lowered.instructions | Where-Object {
        $_.kindName -eq "binary" -and $_.origin -in @($operations.origin)
    }).Count -ne 0 ) {
    throw "Dynamic binary semantics leaked into a physical BINARY instruction"
}

Invoke-Xl @("cgen", $source, "--backend", "xlir", "--emit", $generatedPath) `
    "XLIR extended dynamic binary Cgen failed"
Invoke-Xl @("cgen", $source, "--backend", "xlir", "--emit", $generatedAgainPath) `
    "XLIR extended dynamic binary deterministic Cgen failed"
if ( ! [System.Linq.Enumerable]::SequenceEqual(
        [byte[]][System.IO.File]::ReadAllBytes($generatedPath),
        [byte[]][System.IO.File]::ReadAllBytes($generatedAgainPath)
    ) ) {
    throw "XLIR extended dynamic binary C output is not deterministic"
}
$generated = [System.IO.File]::ReadAllText($generatedPath, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "xlir_xvalue_binary(",
    "xlir_xvalue_binary_failure(",
    "xrtValueArrayConcat(left, right)",
    "ZeroDivisionError",
    "shift count is outside the left operand width",
    "b < 0 || b >= 64",
    "(uint64_t)a + (uint64_t)b",
    "INT64_MIN && b == -1"
) ) {
    if ( ! $generated.Contains($required) ) {
        throw "XLIR extended dynamic binary C emission is incomplete: $required"
    }
}

Push-Location $Root
try {
    $xlirCommand = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include $generatedPath $hostSource !XL6_CGEN_LINKS! -o build\test\xlir_dynamic_binary_extended_abi_xlir.exe && build\test\xlir_dynamic_binary_extended_abi_xlir.exe"
    & $env:ComSpec /d /v:on /c $xlirCommand
    if ( $LASTEXITCODE -ne 0 ) { throw "XLIR extended dynamic binary strict-C runtime failed" }

    Invoke-Xl @("cgen", $source, "--backend", "legacy", "--emit", $legacyPath) `
        "Legacy extended dynamic binary differential Cgen failed"
    $legacyCommand = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror $legacyPath $legacySupport !XL6_CGEN_XLANG_LINKS! -o build\test\xlir_dynamic_binary_extended_abi_legacy.exe && build\test\xlir_dynamic_binary_extended_abi_legacy.exe"
    & $env:ComSpec /d /v:on /c $legacyCommand
    if ( $LASTEXITCODE -ne 0 ) { throw "Legacy extended dynamic binary differential runtime failed" }
} finally {
    Pop-Location
}

Write-Output "XLIR extended dynamic binary ABI gate ok: fourteen explicit borrowed-xvalue operations, nine Typed may-raise effects with adjacent ERROR_TEST CFG, checked integer errors, float-null behavior, string/array addition, deterministic strict C and legacy differential runtime semantics."
