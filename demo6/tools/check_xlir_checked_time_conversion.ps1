param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\xlir_checked_time_conversion.xl"
$hostSource = Join-Path $Root "test\cases\xlir_checked_time_conversion_host.c"
$legacySupport = Join-Path $Root "test\cases\xlir_cgen_differential_expect_i64_legacy_support.c"
$outputDir = Join-Path $Root "build\test"
$typedPath = Join-Path $outputDir "xlir_checked_time_conversion.typed.json"
$loweredPath = Join-Path $outputDir "xlir_checked_time_conversion.lowered.json"
$generatedPath = Join-Path $outputDir "xlir_checked_time_conversion_generated.c"
$generatedAgainPath = Join-Path $outputDir "xlir_checked_time_conversion_generated_again.c"
$legacyPath = Join-Path $outputDir "xlir_checked_time_conversion_legacy.c"

foreach ( $path in @($xl, $source, $hostSource, $legacySupport) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR checked time conversion gate input is missing: $path"
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
    "XLIR checked time conversion Typed IR failed"
$typed = [System.IO.File]::ReadAllText($typedPath, [System.Text.Encoding]::UTF8) |
    ConvertFrom-Json
$checked = @($typed.intrinsicCalls | Where-Object {
    $_.kindName -eq "checked-time-convert"
})
Assert-Count $checked 4 "Typed XLIR did not preserve all checked time conversions"
foreach ( $intrinsic in $checked ) {
    if ( $intrinsic.admissionName -ne "verified" -or
         $intrinsic.resultAbiName -ne "value" -or
         $intrinsic.errorAbiName -ne "may-raise" -or
         $intrinsic.operandCount -ne 1 ) {
        throw "Checked time intrinsic has no verified value/failure ABI"
    }
    $operand = $typed.intrinsicOperands[[int]$intrinsic.firstOperand]
    $sourceType = @($typed.types | Where-Object { $_.id -eq $operand.type })
    $resultType = @($typed.types | Where-Object { $_.id -eq $intrinsic.resultType })
    if ( $operand.abiName -ne "borrow" -or $sourceType.Count -ne 1 -or
         $resultType.Count -ne 1 -or $resultType[0].name -ne "time" -or
         $resultType[0].ownershipAbi -ne 1 -or
         @("string", "var") -notcontains $sourceType[0].name ) {
        throw "Checked time intrinsic has no exact borrowed string/xvalue and time TypeId ABI"
    }
}
Assert-Count @($checked | ForEach-Object {
    $operand = $typed.intrinsicOperands[[int]$_.firstOperand]
    @($typed.types | Where-Object { $_.id -eq $operand.type -and $_.name -eq "string" })
}) 2 "Typed XLIR checked time string source partition is incomplete"
Assert-Count @($checked | ForEach-Object {
    $operand = $typed.intrinsicOperands[[int]$_.firstOperand]
    @($typed.types | Where-Object { $_.id -eq $operand.type -and $_.name -eq "var" })
}) 2 "Typed XLIR checked time xvalue source partition is incomplete"

Invoke-Xl @("ir", $source, "--lowered", "--format", "json", "--emit", $loweredPath) `
    "XLIR checked time conversion lowering failed"
$lowered = [System.IO.File]::ReadAllText($loweredPath, [System.Text.Encoding]::UTF8) |
    ConvertFrom-Json
$instructions = @($lowered.instructions)
$fromString = @($instructions | Where-Object {
    $_.runtimeOpName -eq "string-to-time-checked"
})
$fromXValue = @($instructions | Where-Object {
    $_.runtimeOpName -eq "xvalue-to-time-checked"
})
$toString = @($instructions | Where-Object {
    $_.runtimeOpName -eq "time-to-string"
})
$boxedTime = @($instructions | Where-Object {
    $_.runtimeOpName -eq "xvalue-time"
})
Assert-Count $fromString 2 "Lowered XLIR lost checked string-to-time operations"
Assert-Count $fromXValue 2 "Lowered XLIR lost checked xvalue-to-time operations"
Assert-Count $toString 1 "Lowered XLIR lost time-to-string ownership operation"
Assert-Count $boxedTime 1 "Lowered XLIR lost exact xvalue time boxing operation"
foreach ( $conversion in @($fromString) + @($fromXValue) ) {
    $nextIndex = [int]$conversion.id
    if ( $nextIndex -ge $instructions.Count ) {
        throw "Checked time conversion is not followed by ERROR_TEST"
    }
    $next = $instructions[$nextIndex]
    if ( $next.kindName -ne "error-test" -or
         ! (@($next.operands) -contains [int]$conversion.result) ) {
        throw "Checked time conversion does not route its exact result through ERROR_TEST"
    }
}

Invoke-Xl @("cgen", $source, "--backend", "xlir", "--emit", $generatedPath) `
    "XLIR checked time conversion Cgen failed"
Invoke-Xl @("cgen", $source, "--backend", "xlir", "--emit", $generatedAgainPath) `
    "XLIR checked time conversion deterministic Cgen failed"
if ( ! [System.Linq.Enumerable]::SequenceEqual(
        [byte[]][System.IO.File]::ReadAllBytes($generatedPath),
        [byte[]][System.IO.File]::ReadAllBytes($generatedAgainPath)
    ) ) {
    throw "XLIR checked time conversion C output is not deterministic"
}
$generated = [System.IO.File]::ReadAllText($generatedPath, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "static int64_t xlir_string_to_time_checked(const char* text)",
    "static int64_t xlir_xvalue_to_time_checked(const xvalue* value)",
    "static char* xlir_time_to_string(int64_t value)",
    "xrtTimeParseAny(xrtStrView(text), &result)",
    "xrtValueGetTime(value, &result)",
    "xrtValueTime(", "ValueError", "TypeError",
    "if (xrtGetError() == NULL)"
) ) {
    if ( ! $generated.Contains($required) ) {
        throw "XLIR checked time conversion C emission is incomplete: $required"
    }
}

Push-Location $Root
try {
    $xlirCommand = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror $hostSource !XL6_CGEN_LINKS! -o build\test\xlir_checked_time_conversion_host.exe && build\test\xlir_checked_time_conversion_host.exe"
    & $env:ComSpec /d /v:on /c $xlirCommand
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR checked time conversion strict-C runtime failed"
    }

    Invoke-Xl @("cgen", $source, "--backend", "legacy", "--emit", $legacyPath) `
        "Legacy checked time conversion differential Cgen failed"
    $legacyCommand = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror $legacyPath $legacySupport !XL6_CGEN_XLANG_LINKS! -o build\test\xlir_checked_time_conversion_legacy.exe && build\test\xlir_checked_time_conversion_legacy.exe"
    & $env:ComSpec /d /v:on /c $legacyCommand
    if ( $LASTEXITCODE -ne 0 ) {
        throw "Legacy checked time conversion differential runtime failed"
    }
} finally {
    Pop-Location
}

Write-Output "XLIR checked time conversion gate ok: canonical string/xvalue sources, explicit time boxing/formatting, adjacent failure CFG, deterministic mechanical C, strict XLIR failure recovery and shared-path legacy differential semantics."
