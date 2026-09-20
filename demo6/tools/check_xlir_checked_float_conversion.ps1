param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\xlir_checked_float_string.xl"
$hostSource = Join-Path $Root "test\cases\xlir_checked_float_string_host.c"
$outputDir = Join-Path $Root "build\test"
$typedPath = Join-Path $outputDir "xlir_checked_float_string.typed.json"
$loweredPath = Join-Path $outputDir "xlir_checked_float_string.lowered.json"
$generatedPath = Join-Path $outputDir "xlir_checked_float_string_generated.c"
$generatedAgainPath = Join-Path $outputDir "xlir_checked_float_string_generated_again.c"

foreach ( $path in @($xl, $source, $hostSource) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR checked float conversion gate input is missing: $path"
    }
}
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

function Invoke-Xl([string[]]$Arguments, [string]$Failure) {
    & $xl @Arguments
    if ( $LASTEXITCODE -ne 0 ) { throw $Failure }
}

Invoke-Xl @("ir", $source, "--format", "json", "--emit", $typedPath) `
    "XLIR checked float conversion Typed IR failed"
$typed = [System.IO.File]::ReadAllText($typedPath, [System.Text.Encoding]::UTF8) |
    ConvertFrom-Json
$intrinsics = @($typed.intrinsicCalls | Where-Object {
    $_.kindName -eq "checked-float-convert"
})
if ( $intrinsics.Count -ne 3 ) {
    throw "Typed XLIR did not preserve all checked float conversions"
}
foreach ( $intrinsic in $intrinsics ) {
    if ( $intrinsic.admissionName -ne "verified" -or
         $intrinsic.resultAbiName -ne "value" -or
         $intrinsic.errorAbiName -ne "may-raise" -or
         $intrinsic.operandCount -ne 1 ) {
        throw "Checked float intrinsic has no verified value/failure ABI"
    }
    $operand = $typed.intrinsicOperands[[int]$intrinsic.firstOperand]
    $sourceType = @($typed.types | Where-Object { $_.id -eq $operand.type })
    $resultType = @($typed.types | Where-Object { $_.id -eq $intrinsic.resultType })
    if ( $operand.abiName -ne "borrow" -or $sourceType.Count -ne 1 -or
         $sourceType[0].name -ne "string" -or $sourceType[0].ownershipAbi -ne 2 -or
         $resultType.Count -ne 1 -or $resultType[0].cAbi -ne 4 -or
         $resultType[0].ownershipAbi -ne 1 ) {
        throw "Checked float intrinsic has no exact borrowed managed-string/F64 TypeId ABI"
    }
}

Invoke-Xl @("ir", $source, "--lowered", "--format", "json", "--emit", $loweredPath) `
    "XLIR checked float conversion lowering failed"
$lowered = [System.IO.File]::ReadAllText($loweredPath, [System.Text.Encoding]::UTF8) |
    ConvertFrom-Json
$instructions = @($lowered.instructions)
$conversions = @($instructions | Where-Object {
    $_.runtimeOpName -eq "string-to-f64-checked"
})
if ( $conversions.Count -ne 3 ) {
    throw "Lowered XLIR lost checked string-to-F64 runtime operations"
}
foreach ( $conversion in $conversions ) {
    # Instruction ids are one-based, so the array slot at `id` is the next
    # instruction.  The verifier and this gate require an adjacent ERROR_TEST.
    $nextIndex = [int]$conversion.id
    if ( $nextIndex -ge $instructions.Count ) {
        throw "Checked float conversion is not followed by ERROR_TEST"
    }
    $next = $instructions[$nextIndex]
    if ( $next.kindName -ne "error-test" -or
         ! (@($next.operands) -contains [int]$conversion.result) ) {
        throw "Checked float conversion does not route its exact result through ERROR_TEST"
    }
}

Invoke-Xl @("cgen", $source, "--backend", "xlir", "--emit", $generatedPath) `
    "XLIR checked float conversion Cgen failed"
Invoke-Xl @("cgen", $source, "--backend", "xlir", "--emit", $generatedAgainPath) `
    "XLIR checked float conversion deterministic Cgen failed"
if ( ! [System.Linq.Enumerable]::SequenceEqual(
        [byte[]][System.IO.File]::ReadAllBytes($generatedPath),
        [byte[]][System.IO.File]::ReadAllBytes($generatedAgainPath)
    ) ) {
    throw "XLIR checked float conversion C output is not deterministic"
}
$generated = [System.IO.File]::ReadAllText($generatedPath, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "static double xlir_string_to_f64_checked(const char* text)",
    "strtod(text, &end)", "invalid float string", "ValueError",
    "if (xrtGetError() == NULL)"
) ) {
    if ( ! $generated.Contains($required) ) {
        throw "XLIR checked float conversion C emission is incomplete: $required"
    }
}

Push-Location $Root
try {
    $command = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror test\cases\xlir_checked_float_string_host.c !XL6_CGEN_LINKS! -o build\test\xlir_checked_float_string_host.exe && build\test\xlir_checked_float_string_host.exe"
    & $env:ComSpec /d /v:on /c $command
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR checked float conversion strict-C runtime failed"
    }
} finally {
    Pop-Location
}

Write-Output "XLIR checked float conversion gate ok: canonical intrinsic, borrowed string/F64 ABI, adjacent failure CFG, deterministic mechanical C, ValueError recovery and strict runtime."
