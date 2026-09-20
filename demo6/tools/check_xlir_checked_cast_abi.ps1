param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\xlir_checked_cast_abi.xl"
$legacySource = Join-Path $Root "test\cases\xlir_checked_cast_differential.xl"
$hostSource = Join-Path $Root "test\cases\xlir_checked_cast_abi_host.c"
$legacySupport = Join-Path $Root "test\cases\xlir_cgen_differential_expect_i64_legacy_support.c"
$outputDir = Join-Path $Root "build\test"
$typedPath = Join-Path $outputDir "xlir_checked_cast_abi.typed.json"
$loweredPath = Join-Path $outputDir "xlir_checked_cast_abi.lowered.json"
$generatedPath = Join-Path $outputDir "xlir_checked_cast_abi_generated.c"
$generatedAgainPath = Join-Path $outputDir "xlir_checked_cast_abi_generated_again.c"
$legacyPath = Join-Path $outputDir "xlir_checked_cast_abi_legacy.c"

foreach ( $path in @($xl, $source, $legacySource, $hostSource, $legacySupport) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR checked cast ABI gate input is missing: $path"
    }
}
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

function Invoke-Xl([string[]]$Arguments, [string]$Failure) {
    & $xl @Arguments
    if ( $LASTEXITCODE -ne 0 ) { throw $Failure }
}

function Assert-Count([object[]]$Items, [int]$Expected, [string]$Message) {
    if ( @($Items).Count -ne $Expected ) {
        throw "$Message (expected $Expected, got $(@($Items).Count))"
    }
}

$typedExpected = [ordered]@{
    "xvalue-to-bool-checked" = 1
    "xvalue-to-i64-checked" = 2
    "xvalue-to-u64-checked" = 1
    "xvalue-to-f64-checked" = 1
    "xvalue-to-time-checked" = 1
    "xvalue-to-string-checked" = 3
}
$loweredExpected = [ordered]@{
    "xvalue-to-bool-checked" = 1
    "xvalue-to-i64-checked" = 2
    "xvalue-to-u64-checked" = 1
    "xvalue-to-f64-checked" = 3
    "xvalue-to-time-checked" = 1
    "xvalue-to-string-checked" = 3
    "xvalue-to-raw-pointer-checked" = 2
}

Invoke-Xl @("ir", $source, "--typed", "--format", "json", "--emit", $typedPath) `
    "XLIR checked cast Typed IR failed"
$typed = [System.IO.File]::ReadAllText($typedPath, [System.Text.Encoding]::UTF8) |
    ConvertFrom-Json
$casts = @($typed.nodes | Where-Object {
    $_.sourceKindName -eq "type_as" -and $_.castAbiName -ne "invalid"
})
Assert-Count $casts 9 "Typed XLIR did not preserve every checked cast"
foreach ( $entry in $typedExpected.GetEnumerator() ) {
    Assert-Count @($casts | Where-Object { $_.castAbiName -eq $entry.Key }) `
        $entry.Value "Typed XLIR checked cast partition is incomplete for $($entry.Key)"
}
$floatIntrinsics = @($typed.intrinsicCalls | Where-Object {
    $_.kindName -eq "checked-float-convert"
})
Assert-Count $floatIntrinsics 2 `
    "Typed XLIR did not preserve builtin float(var) checked conversions"
foreach ( $intrinsic in $floatIntrinsics ) {
    if ( $intrinsic.admissionName -ne "verified" -or
         $intrinsic.castAbiName -ne "xvalue-to-f64-checked" -or
         $intrinsic.resultAbiName -ne "value" -or
         $intrinsic.errorAbiName -ne "may-raise" -or
         $intrinsic.operandCount -ne 1 ) {
        throw "Typed XLIR builtin float(var) lost its exact Cast/effect ABI"
    }
}
$pointerIntrinsics = @($typed.intrinsicCalls | Where-Object {
    $_.kindName -eq "checked-xvalue-convert"
})
Assert-Count $pointerIntrinsics 2 `
    "Typed XLIR did not preserve builtin point(var) checked conversions"
foreach ( $intrinsic in $pointerIntrinsics ) {
    if ( $intrinsic.admissionName -ne "verified" -or
         $intrinsic.castAbiName -ne "xvalue-to-raw-pointer-checked" -or
         $intrinsic.resultAbiName -ne "value" -or
         $intrinsic.errorAbiName -ne "may-raise" -or
         $intrinsic.operandCount -ne 1 ) {
        throw "Typed XLIR builtin point(var) lost its exact Cast/effect ABI"
    }
}
$cBoolIntrinsics = @($typed.intrinsicCalls | Where-Object {
    $_.kindName -eq "bool-convert" -and $_.admissionName -eq "verified"
})
if ( $cBoolIntrinsics.Count -lt 2 ) {
    throw "Typed XLIR did not preserve c_bool conversion through the bool intrinsic family"
}
foreach ( $cast in $casts ) {
    if ( $cast.castErrorAbiName -ne "may-raise" -or $cast.storageType -eq 0 ) {
        throw "Typed XLIR checked cast has no exact target/effect ABI"
    }
    $plans = @($typed.exitPlans | Where-Object {
        $_.origin -eq $cast.id -and $_.kind -eq 4
    })
    Assert-Count $plans 1 "Typed XLIR checked cast has no unique THROW exit plan"
}

Invoke-Xl @("ir", $source, "--lowered", "--format", "json", "--emit", $loweredPath) `
    "XLIR checked cast lowering failed"
$lowered = [System.IO.File]::ReadAllText($loweredPath, [System.Text.Encoding]::UTF8) |
    ConvertFrom-Json
$instructions = @($lowered.instructions)
$operations = @($instructions | Where-Object {
    $_.kindName -eq "runtime-call" -and $_.runtimeOpName -eq "xvalue-cast-checked"
})
Assert-Count $operations 13 "Lowered XLIR lost checked cast RuntimeOps"
foreach ( $entry in $loweredExpected.GetEnumerator() ) {
    Assert-Count @($operations | Where-Object { $_.castAbiName -eq $entry.Key }) `
        $entry.Value "Lowered XLIR checked cast partition is incomplete for $($entry.Key)"
}
foreach ( $operation in $operations ) {
    if ( $operation.operandCount -ne 1 -or $operation.type -eq 0 -or
         $operation.payloadType -ne $operation.type ) {
        throw "Lowered checked cast RuntimeOp lost source/result/payload identity"
    }
    $nextIndex = [int]$operation.id
    if ( $nextIndex -ge $instructions.Count ) {
        throw "Lowered checked cast is not followed by ERROR_TEST"
    }
    $next = $instructions[$nextIndex]
    if ( $next.kindName -ne "error-test" -or
         ! (@($next.operands) -contains [int]$operation.result) ) {
        throw "Lowered checked cast does not route its result through adjacent ERROR_TEST"
    }
}

Invoke-Xl @("cgen", $source, "--backend", "xlir", "--emit", $generatedPath) `
    "XLIR checked cast Cgen failed"
Invoke-Xl @("cgen", $source, "--backend", "xlir", "--emit", $generatedAgainPath) `
    "XLIR checked cast deterministic Cgen failed"
if ( ! [System.Linq.Enumerable]::SequenceEqual(
        [byte[]][System.IO.File]::ReadAllBytes($generatedPath),
        [byte[]][System.IO.File]::ReadAllBytes($generatedAgainPath)
    ) ) {
    throw "XLIR checked cast C output is not deterministic"
}
$generated = [System.IO.File]::ReadAllText($generatedPath, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "static bool xlir_xvalue_cast_checked(",
    "xlir_xvalue_type_id(source) != expectedTypeId",
    "xrtValueGetBool(source, (bool*)outValue)",
    "xrtValueGetInt(source, (int64_t*)outValue)",
    "xrtValueGetUInt(source, (uint64_t*)outValue)",
    "xrtValueGetFloat(source, (double*)outValue)",
    "xrtValueGetTime(source, (int64_t*)outValue)",
    "xrtValueGetString(source, &text)",
    "xrtValueGetPointer(source, (void**)outValue)",
    "TypeError", "MemoryError",
    "if (xrtGetError() == NULL)"
) ) {
    if ( ! $generated.Contains($required) ) {
        throw "XLIR checked cast mechanical C emission is incomplete: $required"
    }
}

Push-Location $Root
try {
    $xlirCommand = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include $generatedPath $hostSource !XL6_CGEN_LINKS! -o build\test\xlir_checked_cast_abi_xlir.exe && build\test\xlir_checked_cast_abi_xlir.exe"
    & $env:ComSpec /d /v:on /c $xlirCommand
    if ( $LASTEXITCODE -ne 0 ) { throw "XLIR checked cast strict-C runtime failed" }

    Invoke-Xl @("cgen", $legacySource, "--backend", "legacy", "--emit", $legacyPath) `
        "Legacy checked cast differential Cgen failed"
    $legacyCommand = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror $legacyPath $legacySupport !XL6_CGEN_XLANG_LINKS! -o build\test\xlir_checked_cast_abi_legacy.exe && build\test\xlir_checked_cast_abi_legacy.exe"
    & $env:ComSpec /d /v:on /c $legacyCommand
    if ( $LASTEXITCODE -ne 0 ) { throw "Legacy checked cast differential runtime failed" }
} finally {
    Pop-Location
}

Write-Output "XLIR checked cast ABI gate ok: closed scalar/string cast identities including builtin float(var), exact TypeIds, adjacent recoverable failure CFG, rvalue ownership, deterministic strict C and shared-partition legacy differential runtime semantics."
