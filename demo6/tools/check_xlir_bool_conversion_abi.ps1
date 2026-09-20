param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\xlir_bool_conversion_abi.xl"
$hostSource = Join-Path $Root "test\cases\xlir_bool_conversion_abi_host.c"
$legacySupport = Join-Path $Root "test\cases\xlir_cgen_differential_expect_i64_legacy_support.c"
$outputDir = Join-Path $Root "build\test"
$typedPath = Join-Path $outputDir "xlir_bool_conversion_abi.typed.json"
$loweredPath = Join-Path $outputDir "xlir_bool_conversion_abi.lowered.json"
$generatedPath = Join-Path $outputDir "xlir_bool_conversion_abi_generated.c"
$generatedAgainPath = Join-Path $outputDir "xlir_bool_conversion_abi_generated_again.c"
$legacyPath = Join-Path $outputDir "xlir_bool_conversion_abi_legacy.c"

foreach ( $path in @($xl, $source, $hostSource, $legacySupport) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR bool conversion gate input is missing: $path"
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

Invoke-Xl @("ir", $source, "--format", "json", "--emit", $typedPath) `
    "XLIR bool conversion Typed IR failed"
$typed = [System.IO.File]::ReadAllText($typedPath, [System.Text.Encoding]::UTF8) |
    ConvertFrom-Json
$intrinsics = @($typed.intrinsicCalls | Where-Object {
    $_.kindName -eq "bool-convert"
})
Assert-Count $intrinsics 14 "Typed XLIR did not preserve every explicit bool conversion"
$valueSources = @("bool", "int", "float", "null", "time")
$valueCount = 0
$dynamicCount = 0
$nullOrigin = 0
foreach ( $intrinsic in $intrinsics ) {
    if ( $intrinsic.admissionName -ne "verified" -or
         $intrinsic.resultAbiName -ne "value" -or
         $intrinsic.errorAbiName -ne "no-raise" -or
         $intrinsic.operandCount -ne 1 -or
         $intrinsic.scalarConversionAbiName -ne "invalid" ) {
        throw "Bool intrinsic has no verified semantic identity/result/effect ABI"
    }
    $operand = $typed.intrinsicOperands[[int]$intrinsic.firstOperand]
    $sourceType = @($typed.types | Where-Object { $_.id -eq $operand.type })
    $resultType = @($typed.types | Where-Object { $_.id -eq $intrinsic.resultType })
    if ( $sourceType.Count -ne 1 -or $resultType.Count -ne 1 -or
         $resultType[0].name -ne "bool" ) {
        throw "Bool intrinsic lost an exact source/result TypeId"
    }
    if ( $sourceType[0].name -eq "var" ) {
        if ( $operand.abiName -ne "borrow" ) {
            throw "Dynamic bool conversion does not borrow its xvalue source"
        }
        $dynamicCount += 1
    } elseif ( $valueSources -contains $sourceType[0].name ) {
        if ( $operand.abiName -ne "value" ) {
            throw "Static/null bool conversion does not use a value operand"
        }
        $valueCount += 1
        if ( $sourceType[0].name -eq "null" ) {
            $nullOrigin = [int]$intrinsic.origin
        }
    } else {
        throw "Bool intrinsic admitted an unexpected source partition: $($sourceType[0].name)"
    }
}
if ( $valueCount -ne 7 -or $dynamicCount -ne 7 ) {
    throw "Bool intrinsic source partition is incomplete: value=$valueCount dynamic=$dynamicCount"
}

Invoke-Xl @("ir", $source, "--lowered", "--format", "json", "--emit", $loweredPath) `
    "XLIR bool conversion lowering failed"
$lowered = [System.IO.File]::ReadAllText($loweredPath, [System.Text.Encoding]::UTF8) |
    ConvertFrom-Json
$instructions = @($lowered.instructions)
$nonzero = @($instructions | Where-Object {
    $_.kindName -eq "binary" -and $_.opcodeName -eq "ne"
})
$dynamicTruth = @($instructions | Where-Object {
    $_.kindName -eq "runtime-call" -and $_.runtimeOpName -eq "xvalue-truthy"
})
$falseConstants = @($instructions | Where-Object {
    $_.kindName -eq "const" -and $_.origin -eq $nullOrigin -and
    $_.hasImmediate -eq 1
})
Assert-Count $nonzero 5 "Lowered XLIR lost static scalar nonzero truth operations"
Assert-Count $dynamicTruth 7 "Lowered XLIR lost dynamic xvalue truth operations"
if ( $nullOrigin -eq 0 -or $falseConstants.Count -ne 1 ) {
    throw "Lowered XLIR lost the null-to-false constant operation"
}
foreach ( $operation in $nonzero ) {
    if ( $operation.operandCount -ne 2 -or $operation.type -eq 0 ) {
        throw "Static bool conversion has no exact comparison operands/result TypeId"
    }
}
foreach ( $operation in $dynamicTruth ) {
    if ( $operation.operandCount -ne 1 -or $operation.type -eq 0 ) {
        throw "Dynamic bool conversion has no exact borrowed source/result TypeId"
    }
}

Invoke-Xl @("cgen", $source, "--backend", "xlir", "--emit", $generatedPath) `
    "XLIR bool conversion Cgen failed"
Invoke-Xl @("cgen", $source, "--backend", "xlir", "--emit", $generatedAgainPath) `
    "XLIR bool conversion deterministic Cgen failed"
if ( ! [System.Linq.Enumerable]::SequenceEqual(
        [byte[]][System.IO.File]::ReadAllBytes($generatedPath),
        [byte[]][System.IO.File]::ReadAllBytes($generatedAgainPath)
    ) ) {
    throw "XLIR bool conversion C output is not deterministic"
}
$generated = [System.IO.File]::ReadAllText($generatedPath, [System.Text.Encoding]::UTF8)
foreach ( $required in @(" != ", "xrtValueTruthy(", "false") ) {
    if ( ! $generated.Contains($required) ) {
        throw "XLIR bool conversion C emission is incomplete: $required"
    }
}

Push-Location $Root
try {
    $xlirCommand = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror $hostSource !XL6_CGEN_LINKS! -o build\test\xlir_bool_conversion_abi_xlir.exe && build\test\xlir_bool_conversion_abi_xlir.exe"
    & $env:ComSpec /d /v:on /c $xlirCommand
    if ( $LASTEXITCODE -ne 0 ) { throw "XLIR bool conversion strict-C runtime failed" }

    Invoke-Xl @("cgen", $source, "--backend", "legacy", "--emit", $legacyPath) `
        "Legacy bool conversion differential Cgen failed"
    $legacyCommand = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror $legacyPath $legacySupport !XL6_CGEN_XLANG_LINKS! -o build\test\xlir_bool_conversion_abi_legacy.exe && build\test\xlir_bool_conversion_abi_legacy.exe"
    & $env:ComSpec /d /v:on /c $legacyCommand
    if ( $LASTEXITCODE -ne 0 ) { throw "Legacy bool conversion differential runtime failed" }
} finally {
    Pop-Location
}

Write-Output "XLIR bool conversion ABI gate ok: explicit typed truth semantics, closed scalar/null/xvalue partitions, mechanical nonzero/runtime lowering, deterministic strict C and legacy differential runtime semantics."
