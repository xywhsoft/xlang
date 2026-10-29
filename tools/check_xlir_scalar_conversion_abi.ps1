param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\xlir_scalar_conversion_abi.xl"
$hostSource = Join-Path $Root "test\cases\xlir_default_class_layout_host.c"
$outputDir = Join-Path $Root "build\test"
$typedPath = Join-Path $outputDir "xlir_scalar_conversion_abi.typed.json"
$loweredPath = Join-Path $outputDir "xlir_scalar_conversion_abi.lowered.json"
$generatedPath = Join-Path $outputDir "xlir_scalar_conversion_abi_generated.c"
$generatedAgainPath = Join-Path $outputDir "xlir_scalar_conversion_abi_generated_again.c"
$promotionSource = Join-Path $Root "test\cases\type_mixed_int_arith_infer.xl"
$promotionLoweredPath = Join-Path $outputDir "type_mixed_int_arith_infer.lowered.json"
$promotionGeneratedPath = Join-Path $outputDir "type_mixed_int_arith_infer.generated.c"

foreach ( $path in @($xl, $source, $promotionSource, $hostSource) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR scalar conversion gate input is missing: $path"
    }
}
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

function Invoke-Xl([string[]]$Arguments, [string]$Failure) {
    & $xl @Arguments
    if ( $LASTEXITCODE -ne 0 ) { throw $Failure }
}

$families = @(
    "bool-to-bool", "bool-to-integer", "bool-to-float", "integer-to-bool",
    "integer-to-integer", "integer-to-float", "float-to-integer",
    "float-to-float"
)
$typedScalarFamilies = @(
    "bool-to-integer", "bool-to-float", "integer-to-integer",
    "integer-to-float", "float-to-integer", "float-to-float"
)
$loweredScalarFamilies = @($families | Where-Object { $_ -ne "integer-to-bool" })

Invoke-Xl @("ir", $source, "--format", "json", "--emit", $typedPath) `
    "XLIR scalar conversion Typed IR failed"
$typed = [System.IO.File]::ReadAllText($typedPath, [System.Text.Encoding]::UTF8) |
    ConvertFrom-Json
$intrinsics = @($typed.intrinsicCalls | Where-Object {
    $_.kindName -eq "scalar-convert"
})
foreach ( $family in $typedScalarFamilies ) {
    $matches = @($intrinsics | Where-Object {
        $_.scalarConversionAbiName -eq $family -and
        $_.admissionName -eq "verified" -and
        $_.resultAbiName -eq "value" -and
        $_.errorAbiName -eq "no-raise"
    })
    if ( $matches.Count -lt 1 ) {
        throw "Typed XLIR lost a verified scalar conversion family: $family"
    }
}
$operands = @($typed.intrinsicOperands)
foreach ( $intrinsic in $intrinsics ) {
    if ( $intrinsic.operandCount -ne 1 -or
         $operands[$intrinsic.firstOperand].abiName -ne "value" ) {
        throw "Typed XLIR scalar conversion has no exact value operand ABI"
    }
}
$boolIntrinsics = @($typed.intrinsicCalls | Where-Object {
    $_.kindName -eq "bool-convert" -and
    $_.admissionName -eq "verified" -and
    $_.resultAbiName -eq "value" -and
    $_.errorAbiName -eq "no-raise" -and
    $_.operandCount -eq 1
})
if ( $boolIntrinsics.Count -ne 2 ) {
    throw "Typed XLIR lost the dedicated bool-to-bool/integer-to-bool semantic operations"
}
$boolSources = @($boolIntrinsics | ForEach-Object {
    $boolIntrinsic = $_
    $operand = $operands[[int]$boolIntrinsic.firstOperand]
    $sourceType = @($typed.types | Where-Object { [int]$_.id -eq [int]$operand.type })
    $targetType = @($typed.types | Where-Object {
        [int]$_.id -eq [int]$boolIntrinsic.resultType
    })
    if ( $sourceType.Count -ne 1 -or $targetType.Count -ne 1 -or
         $targetType[0].name -ne "c_bool" -or $operand.abiName -ne "value" ) {
        throw "Typed XLIR dedicated bool conversion lost its exact source/result TypeId ABI"
    }
    $sourceType[0].name
})
if ( @($boolSources | Where-Object { $_ -eq "bool" }).Count -ne 1 -or
     @($boolSources | Where-Object { $_ -eq "int" }).Count -ne 1 ) {
    throw "Typed XLIR dedicated bool conversion source partition is incomplete"
}
$integerToBoolIntrinsic = @($boolIntrinsics | Where-Object {
    $operand = $operands[[int]$_.firstOperand]
    @($typed.types | Where-Object {
        [int]$_.id -eq [int]$operand.type -and $_.name -eq "int"
    }).Count -eq 1
})
if ( $integerToBoolIntrinsic.Count -ne 1 ) {
    throw "Typed XLIR has no unique integer-to-bool semantic operation"
}

Invoke-Xl @("ir", $source, "--lowered", "--format", "json", "--emit", $loweredPath) `
    "XLIR scalar conversion lowering failed"
$lowered = [System.IO.File]::ReadAllText($loweredPath, [System.Text.Encoding]::UTF8) |
    ConvertFrom-Json
$instructions = @($lowered.instructions | Where-Object {
    $_.kindName -eq "scalar-convert"
})
foreach ( $family in $loweredScalarFamilies ) {
    if ( @($instructions | Where-Object {
            $_.scalarConversionAbiName -eq $family
        }).Count -lt 1 ) {
        throw "Lowered XLIR lost a scalar conversion family: $family"
    }
}
if ( @($lowered.instructions | Where-Object {
        [int]$_.origin -eq [int]$integerToBoolIntrinsic[0].origin -and
        $_.kindName -eq "binary" -and $_.opcodeName -eq "ne"
    }).Count -ne 1 ) {
    throw "Lowered XLIR lost the explicit integer nonzero truth operation"
}
if ( @($instructions | Where-Object {
        $_.scalarConversionAbiName -eq "invalid" -or $_.operandCount -ne 1
    }).Count -ne 0 ) {
    throw "Lowered XLIR contains an ambiguous scalar conversion"
}

Invoke-Xl @("ir", $promotionSource, "--lowered", "--format", "json", "--emit", $promotionLoweredPath) `
    "XLIR mixed-width operator promotion lowering failed"
$promotionLowered = [System.IO.File]::ReadAllText(
    $promotionLoweredPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
if ( @($promotionLowered.instructions | Where-Object {
        $_.kindName -eq "scalar-convert" -and
        $_.scalarConversionAbiName -eq "integer-to-integer"
    }).Count -lt 1 ) {
    throw "Lowered XLIR lost the explicit mixed-width integer operator promotion"
}

Invoke-Xl @("cgen", $source, "--backend", "xlir", "--emit", $generatedPath) `
    "XLIR scalar conversion Cgen failed"
Invoke-Xl @("cgen", $promotionSource, "--backend", "xlir", "--emit", $promotionGeneratedPath) `
    "XLIR mixed-width operator promotion Cgen failed"
Invoke-Xl @("cgen", $source, "--backend", "xlir", "--emit", $generatedAgainPath) `
    "XLIR scalar conversion deterministic Cgen failed"
if ( ! [System.Linq.Enumerable]::SequenceEqual(
        [byte[]][System.IO.File]::ReadAllBytes($generatedPath),
        [byte[]][System.IO.File]::ReadAllBytes($generatedAgainPath)
    ) ) {
    throw "XLIR scalar conversion C output is not deterministic"
}
$generated = [System.IO.File]::ReadAllText($generatedPath, [System.Text.Encoding]::UTF8)
foreach ( $cast in @("(bool)", "(int)", "(unsigned int)", "(double)", "(float)") ) {
    if ( ! $generated.Contains($cast) ) {
        throw "XLIR scalar conversion C emission is incomplete: $cast"
    }
}

Push-Location $Root
try {
    $xlirCommand = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include $generatedPath $hostSource !XL6_CGEN_LINKS! -o build\test\xlir_scalar_conversion_abi_xlir.exe && build\test\xlir_scalar_conversion_abi_xlir.exe"
    & $env:ComSpec /d /v:on /c $xlirCommand
    if ( $LASTEXITCODE -ne 0 ) { throw "XLIR scalar conversion strict-C runtime failed" }

    $promotionCommand = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include $promotionGeneratedPath $hostSource !XL6_CGEN_LINKS! -o build\test\type_mixed_int_arith_infer_xlir.exe && build\test\type_mixed_int_arith_infer_xlir.exe"
    & $env:ComSpec /d /v:on /c $promotionCommand
    if ( $LASTEXITCODE -ne 0 ) { throw "XLIR mixed-width operator promotion strict-C runtime failed" }

} finally {
    Pop-Location
}

Write-Output "XLIR scalar conversion ABI gate ok: eight explicit semantic families, lossless mixed-width operator promotion, exact TypeId operands/results, deterministic mechanical C, and strict runtime semantics."
