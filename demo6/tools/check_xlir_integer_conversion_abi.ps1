param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\xlir_integer_conversion_source_abi.xl"
$hostSource = Join-Path $Root "test\cases\xlir_integer_conversion_source_abi_host.c"
$narrowSource = Join-Path $Root "test\cases\type_int_narrowing_wrap.xl"
$genericHost = Join-Path $Root "test\cases\xlir_default_class_layout_host.c"
$outputDir = Join-Path $Root "build\test"
$typedPath = Join-Path $outputDir "xlir_integer_conversion_source_abi.typed.json"
$loweredPath = Join-Path $outputDir "xlir_integer_conversion_source_abi.lowered.json"
$generatedPath = Join-Path $outputDir "xlir_integer_conversion_source_abi_generated.c"
$generatedAgainPath = Join-Path $outputDir "xlir_integer_conversion_source_abi_generated_again.c"
$narrowTypedPath = Join-Path $outputDir "type_int_narrowing_wrap.typed.json"
$narrowLoweredPath = Join-Path $outputDir "type_int_narrowing_wrap.lowered.json"
$narrowGeneratedPath = Join-Path $outputDir "type_int_narrowing_wrap_xlir.c"
$narrowGeneratedAgainPath = Join-Path $outputDir "type_int_narrowing_wrap_xlir_again.c"

foreach ( $path in @($xl, $source, $hostSource, $narrowSource, $genericHost) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR integer conversion gate input is missing: $path"
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
    "XLIR integer conversion Typed IR failed"
$typed = [System.IO.File]::ReadAllText($typedPath, [System.Text.Encoding]::UTF8) |
    ConvertFrom-Json
$checked = @($typed.intrinsicCalls | Where-Object {
    $_.kindName -eq "checked-integer-convert" -and
    $_.admissionName -eq "verified"
})
$wrapping = @($typed.intrinsicCalls | Where-Object {
    $_.kindName -eq "wrapping-integer-convert" -and
    $_.admissionName -eq "verified"
})
Assert-Count $checked 8 "Typed XLIR did not preserve every checked conversion"
Assert-Count $wrapping 2 "Typed XLIR did not preserve static U64 wrapping conversions"
foreach ( $signature in @(
    @{ Source = "managed-string"; Target = "i64"; Expected = 2 },
    @{ Source = "managed-string"; Target = "u64"; Expected = 3 },
    @{ Source = "f64"; Target = "i64"; Expected = 1 },
    @{ Source = "xvalue"; Target = "i64"; Expected = 1 },
    @{ Source = "xvalue"; Target = "u64"; Expected = 1 }
) ) {
    $matches = @($checked | Where-Object {
        $_.integerSourceAbiName -eq $signature.Source -and
        $_.integerTargetName -eq $signature.Target
    })
    Assert-Count $matches $signature.Expected `
        "Typed XLIR checked conversion signature is incomplete: $($signature.Source) -> $($signature.Target)"
}
if ( @($wrapping | Where-Object {
        $_.integerSourceAbiName -ne "u64" -or
        $_.integerTargetName -ne "i64" -or
        $_.errorAbiName -ne "no-raise"
    }).Count -ne 0 ) {
    throw "Typed XLIR static U64 conversion is not an explicit non-raising wrapping ABI"
}

Invoke-Xl @("ir", $source, "--lowered", "--format", "json", "--emit", $loweredPath) `
    "XLIR integer conversion lowering failed"
$lowered = [System.IO.File]::ReadAllText($loweredPath, [System.Text.Encoding]::UTF8) |
    ConvertFrom-Json
$checkedInstructions = @($lowered.instructions | Where-Object {
    $_.runtimeOpName -eq "integer-convert-checked"
})
$wrappingInstructions = @($lowered.instructions | Where-Object {
    $_.runtimeOpName -eq "integer-convert-wrapping"
})
Assert-Count $checkedInstructions 8 "Lowered XLIR lost checked conversion operations"
Assert-Count $wrappingInstructions 2 "Lowered XLIR lost wrapping conversion operations"
$instructions = @($lowered.instructions)
foreach ( $instruction in $checkedInstructions ) {
    # JSON instruction ids are one-based while the array is zero-based; the
    # next instruction therefore lives at array index equal to the current id.
    $nextIndex = [int]$instruction.id
    if ( $nextIndex -ge $instructions.Count ) {
        throw "Checked integer conversion is not followed by ERROR_TEST"
    }
    $next = $instructions[$nextIndex]
    if ( $next.kindName -ne "error-test" -or
         ! (@($next.operands) -contains [int]$instruction.result) ) {
        throw "Checked integer conversion does not route failure through its exact result"
    }
}

Invoke-Xl @("cgen", $source, "--backend", "xlir", "--emit", $generatedPath) `
    "XLIR integer conversion Cgen failed"
Invoke-Xl @("cgen", $source, "--backend", "xlir", "--emit", $generatedAgainPath) `
    "XLIR integer conversion deterministic Cgen failed"
if ( ! [System.Linq.Enumerable]::SequenceEqual(
        [byte[]][System.IO.File]::ReadAllBytes($generatedPath),
        [byte[]][System.IO.File]::ReadAllBytes($generatedAgainPath)
    ) ) {
    throw "XLIR integer conversion C output is not deterministic"
}
$generated = [System.IO.File]::ReadAllText($generatedPath, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "extern bool xl6IntegerTextConvertV1(xstrview, unsigned int, void*)",
    "extern bool xl6IntegerValueConvertV1(const xvalue*, unsigned int, uint64_t, void*)",
    "projection_ok_",
    "static int64_t xlir_integer_wrap_to_i64(uint64_t value, int target)",
    "xl6IntegerTextConvertV1(v",
    "xl6IntegerValueConvertV1(v"
) ) {
    if ( ! $generated.Contains($required) ) {
        throw "XLIR integer conversion C emission is incomplete: $required"
    }
}
if ( $generated.Contains("xlir_u64_to_i64_checked") ) {
    throw "Static U64 conversion incorrectly re-entered the checked ABI"
}

Invoke-Xl @("ir", $narrowSource, "--format", "json", "--emit", $narrowTypedPath) `
    "XLIR narrow integer Typed IR failed"
$narrowTyped = [System.IO.File]::ReadAllText($narrowTypedPath, [System.Text.Encoding]::UTF8) |
    ConvertFrom-Json
foreach ( $entry in @(
    @{ Name = "int8"; Target = "i8" }, @{ Name = "uint8"; Target = "u8" },
    @{ Name = "int16"; Target = "i16" }, @{ Name = "uint16"; Target = "u16" },
    @{ Name = "int32"; Target = "i32" }, @{ Name = "uint32"; Target = "u32" },
    @{ Name = "int"; Target = "i64" }
) ) {
    $type = @($narrowTyped.types | Where-Object { $_.name -eq $entry.Name })
    if ( $type.Count -ne 1 -or $type[0].integerTargetName -ne $entry.Target ) {
        throw "XLIR integer TypeId has no exact normalization domain: $($entry.Name)"
    }
}
$narrowIntrinsics = @($narrowTyped.intrinsicCalls | Where-Object {
    $_.kindName -eq "wrapping-integer-convert" -and
    $_.admissionName -eq "verified"
})
Assert-Count $narrowIntrinsics 8 "Typed XLIR narrow conversions are not canonical wrapping intrinsics"

Invoke-Xl @("ir", $narrowSource, "--lowered", "--format", "json", "--emit", $narrowLoweredPath) `
    "XLIR narrow integer lowering failed"
$narrowLowered = [System.IO.File]::ReadAllText($narrowLoweredPath, [System.Text.Encoding]::UTF8) |
    ConvertFrom-Json
Assert-Count @($narrowLowered.instructions | Where-Object {
    $_.runtimeOpName -eq "integer-convert-wrapping"
}) 12 "Lowered XLIR did not normalize explicit, initializer and compound narrow integer results"
$compoundAdds = @($narrowLowered.instructions | Where-Object {
    $_.kindName -eq "binary" -and $_.opcodeName -eq "add"
})
Assert-Count $compoundAdds 2 "Lowered XLIR did not preserve the two compound additions"
foreach ( $addition in $compoundAdds ) {
    $leftValue = [int]@($addition.operands)[0]
    $promotion = @($narrowLowered.instructions | Where-Object {
        [int]$_.result -eq $leftValue -and
        $_.kindName -eq "scalar-convert" -and
        $_.scalarConversionAbiName -eq "integer-to-integer" -and
        [int]$_.type -eq [int]$addition.type
    })
    Assert-Count $promotion 1 `
        "Lowered XLIR did not explicitly promote a narrow compound operand into the binary execution type"
}

Invoke-Xl @("cgen", $narrowSource, "--backend", "xlir", "--emit", $narrowGeneratedPath) `
    "XLIR narrow integer Cgen failed"
Invoke-Xl @("cgen", $narrowSource, "--backend", "xlir", "--emit", $narrowGeneratedAgainPath) `
    "XLIR narrow integer deterministic Cgen failed"
if ( ! [System.Linq.Enumerable]::SequenceEqual(
        [byte[]][System.IO.File]::ReadAllBytes($narrowGeneratedPath),
        [byte[]][System.IO.File]::ReadAllBytes($narrowGeneratedAgainPath)
    ) ) {
    throw "XLIR narrow integer C output is not deterministic"
}

Push-Location $Root
try {
    $xlirCommand = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror $hostSource !XL6_CGEN_LINKS! -o build\test\xlir_integer_conversion_source_abi_host.exe && build\test\xlir_integer_conversion_source_abi_host.exe"
    & $env:ComSpec /d /v:on /c $xlirCommand
    if ( $LASTEXITCODE -ne 0 ) { throw "XLIR integer conversion strict-C runtime failed" }

    $narrowXlirCommand = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include $narrowGeneratedPath $genericHost !XL6_CGEN_LINKS! -o build\test\type_int_narrowing_wrap_xlir.exe && build\test\type_int_narrowing_wrap_xlir.exe"
    & $env:ComSpec /d /v:on /c $narrowXlirCommand
    if ( $LASTEXITCODE -ne 0 ) { throw "XLIR narrow integer strict-C runtime failed" }

} finally {
    Pop-Location
}

Write-Output "XLIR integer conversion ABI gate ok: explicit source/target metadata, checked failure CFG, static modulo wrapping, narrow initializer/compound normalization, deterministic mechanical C, and strict runtime semantics."
