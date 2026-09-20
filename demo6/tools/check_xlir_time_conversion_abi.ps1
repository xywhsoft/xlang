param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\xlir_time_conversion_abi.xl"
$hostSource = Join-Path $Root "test\cases\xlir_time_conversion_abi_host.c"
$supportSource = Join-Path $Root "test\cases\xlir_time_conversion_abi_support.c"
$outputDir = Join-Path $Root "build\test"
$typedPath = Join-Path $outputDir "xlir_time_conversion_abi.typed.json"
$loweredPath = Join-Path $outputDir "xlir_time_conversion_abi.lowered.json"
$generatedPath = Join-Path $outputDir "xlir_time_conversion_abi_generated.c"
$generatedAgainPath = Join-Path $outputDir "xlir_time_conversion_abi_generated_again.c"
$legacyPath = Join-Path $outputDir "xlir_time_conversion_abi_legacy.c"

foreach ( $path in @($xl, $source, $hostSource, $supportSource) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR time conversion gate input is missing: $path"
    }
}
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

function Invoke-Xl([string[]]$Arguments, [string]$Failure) {
    & $xl @Arguments
    if ( $LASTEXITCODE -ne 0 ) { throw $Failure }
}

$families = @(
    "time-to-integer", "time-to-float", "integer-to-time"
)
$truthFamily = "time-to-bool"

Invoke-Xl @("ir", $source, "--format", "json", "--emit", $typedPath) `
    "XLIR time conversion Typed IR failed"
$typed = [System.IO.File]::ReadAllText($typedPath, [System.Text.Encoding]::UTF8) |
    ConvertFrom-Json
$timeTypes = @($typed.types | Where-Object {
    $_.name -eq "time" -and $_.cAbi -eq 40 -and
    $_.ownershipAbi -eq 1 -and $_.nullAbi -eq 0 -and
    $_.integerTargetName -eq "none" -and
    $_.typeValueId -eq "0x667FBC37049F35EA" -and
    $_.errorResultCName -eq "xlir_result_i64"
})
if ( $timeTypes.Count -ne 1 ) {
    throw "Typed XLIR lost the distinct time/TIME_I64 runtime identity contract"
}
$intrinsics = @($typed.intrinsicCalls | Where-Object {
    $_.kindName -eq "scalar-convert"
})
$boolIntrinsics = @($typed.intrinsicCalls | Where-Object {
    $_.kindName -eq "bool-convert" -and
    $_.admissionName -eq "verified" -and
    $_.resultType -eq 1 -and
    $_.resultAbiName -eq "value" -and
    $_.errorAbiName -eq "no-raise" -and
    $_.operandCount -eq 1
})
$typedOperands = @($typed.intrinsicOperands)
$timeTruthIntrinsics = @($boolIntrinsics | Where-Object {
    $operand = $typedOperands[$_.firstOperand]
    $operand.type -eq $timeTypes[0].id -and $operand.abiName -eq "value"
})
if ( $timeTruthIntrinsics.Count -ne 1 ) {
    throw "Typed XLIR lost the verified $truthFamily value-truth ABI"
}
foreach ( $family in $families ) {
    $matches = @($intrinsics | Where-Object {
        $_.scalarConversionAbiName -eq $family -and
        $_.admissionName -eq "verified" -and
        $_.resultAbiName -eq "value" -and
        $_.errorAbiName -eq "no-raise"
    })
    if ( $matches.Count -lt 1 ) {
        throw "Typed XLIR lost a verified time conversion family: $family"
    }
}
$operands = $typedOperands
foreach ( $intrinsic in $intrinsics | Where-Object {
    $families -contains $_.scalarConversionAbiName
}) {
    if ( $intrinsic.operandCount -ne 1 -or
         $operands[$intrinsic.firstOperand].abiName -ne "value" ) {
        throw "Typed XLIR time conversion has no exact value operand ABI"
    }
}

Invoke-Xl @("ir", $source, "--lowered", "--format", "json", "--emit", $loweredPath) `
    "XLIR time conversion lowering failed"
$lowered = [System.IO.File]::ReadAllText($loweredPath, [System.Text.Encoding]::UTF8) |
    ConvertFrom-Json
$instructions = @($lowered.instructions | Where-Object {
    $_.kindName -eq "scalar-convert"
})
$allInstructions = @($lowered.instructions)
$timeTruthInstructions = @()
foreach ( $instruction in $allInstructions | Where-Object {
    $_.kindName -eq "binary" -and $_.opcodeName -eq "ne" -and
    $_.type -eq 1 -and $_.operandCount -eq 2
}) {
    $left = @($allInstructions | Where-Object {
        $_.result -eq $instruction.operands[0]
    })[0]
    $right = @($allInstructions | Where-Object {
        $_.result -eq $instruction.operands[1]
    })[0]
    if ( $null -ne $left -and $null -ne $right -and
         $left.type -eq $timeTypes[0].id -and
         $right.type -eq $timeTypes[0].id -and
         $right.kindName -eq "const" -and $right.hasImmediate -eq 1 ) {
        $timeTruthInstructions += $instruction
    }
}
if ( $timeTruthInstructions.Count -ne 1 ) {
    throw "Lowered XLIR lost the mechanical $truthFamily nonzero comparison"
}
foreach ( $family in $families ) {
    if ( @($instructions | Where-Object {
            $_.scalarConversionAbiName -eq $family
        }).Count -lt 1 ) {
        throw "Lowered XLIR lost a time conversion family: $family"
    }
}
if ( @($instructions | Where-Object {
        $_.scalarConversionAbiName -eq "invalid" -or $_.operandCount -ne 1
    }).Count -ne 0 ) {
    throw "Lowered XLIR contains an ambiguous time conversion"
}

Invoke-Xl @("cgen", $source, "--backend", "xlir", "--emit", $generatedPath) `
    "XLIR time conversion Cgen failed"
Invoke-Xl @("cgen", $source, "--backend", "xlir", "--emit", $generatedAgainPath) `
    "XLIR time conversion deterministic Cgen failed"
if ( ! [System.Linq.Enumerable]::SequenceEqual(
        [byte[]][System.IO.File]::ReadAllBytes($generatedPath),
        [byte[]][System.IO.File]::ReadAllBytes($generatedAgainPath)
    ) ) {
    throw "XLIR time conversion C output is not deterministic"
}
$generated = [System.IO.File]::ReadAllText($generatedPath, [System.Text.Encoding]::UTF8)
foreach ( $cast in @("(int64_t)", "(double)") ) {
    if ( ! $generated.Contains($cast) ) {
        throw "XLIR time conversion C emission is incomplete: $cast"
    }
}
foreach ( $signature in @(
    "extern int64_t xlir_test_time_value(void);",
    "extern int64_t xlir_test_time_echo(int64_t);"
) ) {
    if ( ! $generated.Contains($signature) ) {
        throw "XLIR time extern C ABI is incomplete: $signature"
    }
}

Invoke-Xl @("cgen", $source, "--backend", "legacy", "--emit", $legacyPath) `
    "Legacy time conversion differential Cgen failed"

Push-Location $Root
try {
    $xlirCommand = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror $hostSource !XL6_CGEN_LINKS! -o build\test\xlir_time_conversion_abi_xlir.exe && build\test\xlir_time_conversion_abi_xlir.exe"
    & $env:ComSpec /d /v:on /c $xlirCommand
    if ( $LASTEXITCODE -ne 0 ) { throw "XLIR time conversion strict-C runtime failed" }

    $legacyCommand = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_USE_LEGACY_GENERATED $hostSource !XL6_CGEN_XLANG_LINKS! -o build\test\xlir_time_conversion_abi_legacy.exe && build\test\xlir_time_conversion_abi_legacy.exe"
    & $env:ComSpec /d /v:on /c $legacyCommand
    if ( $LASTEXITCODE -ne 0 ) { throw "Legacy time conversion differential runtime failed" }
} finally {
    Pop-Location
}

Write-Output "XLIR time conversion ABI gate ok: distinct TIME_I64 identity, time-to-bool truth ABI, three scalar conversion families, separate typed bool truth semantics, extern parameter/return ABI, deterministic mechanical C, strict compile, and legacy differential runtime semantics."
