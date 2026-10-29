param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

function Read-Utf8([string]$path)
{
    return [System.IO.File]::ReadAllText($path, [System.Text.Encoding]::UTF8)
}

$xl = Join-Path $Root "release\xl.exe"
$outputDir = Join-Path $Root "build\tmp"
$lowerSource = Join-Path $Root "src\ir\xl6_ir_lower.c"
$cgenSource = Join-Path $Root "src\cgen\xl6_ir_cgen.c"
$genericHost = Join-Path $Root "test\cases\xlir_generated_module_host.c"
$cases = @(
    @{ Name = "checked"; Source = "test\cases\cgen_var_numeric_convert.xl"; Kinds = @("checked-integer-convert") },
    @{ Name = "trivial-integer"; Source = "test\cases\xlir_intrinsic_trivial_integer_convert.xl"; Kinds = @("trivial-integer-convert") },
    @{ Name = "trivial-identity"; Source = "test\cases\xlir_intrinsic_trivial_identity_convert.xl"; Kinds = @("trivial-identity-convert") },
    @{ Name = "string"; Source = "test\cases\cgen_async_string_ownership.xl"; Kinds = @("string-convert", "scalar-convert") },
    @{ Name = "foreign"; Source = "test\cases\c_abi_types.xl"; Kinds = @("scalar-convert") },
    @{ Name = "copy"; Source = "test\cases\cgen_copy_clone.xl"; Kinds = @("copy", "clone") },
    @{ Name = "copy-noncopyable"; Source = "test\cases\cgen_native_class_lifecycle.xl"; Kinds = @("copy") },
    @{ Name = "append"; Source = "test\cases\xlir_class_virtual_dispatch.xl"; Kinds = @("array-append") },
    @{ Name = "range"; Source = "test\cases\xlir_intrinsic_range.xl"; Kinds = @("range") },
    @{ Name = "xcall"; Source = "test\cases\cgen_extern_function_value.xl"; Kinds = @("xcall") },
    @{ Name = "xcall-class"; Source = "test\cases\cgen_xcall_class.xl"; Kinds = @("xcall", "checked-integer-convert") }
)

foreach ( $path in @($xl, $lowerSource, $cgenSource, $genericHost) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR IntrinsicCall gate input is missing: $path"
    }
}
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

foreach ( $case in $cases ) {
    $source = Join-Path $Root $case.Source
    $typedJson = Join-Path $outputDir "xlir_intrinsic_$($case.Name).typed.json"
    $loweredText = Join-Path $outputDir "xlir_intrinsic_$($case.Name).lowered.ir"
    if ( ! (Test-Path -LiteralPath $source -PathType Leaf) ) {
        throw "XLIR IntrinsicCall fixture is missing: $($case.Source)"
    }

    & $xl ir $source --format json --emit $typedJson
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR typed IntrinsicCall dump failed: $($case.Name)"
    }
    $module = Read-Utf8 $typedJson | ConvertFrom-Json
    $intrinsics = @($module.intrinsicCalls)
    $operands = @($module.intrinsicOperands)
    if ( $module.intrinsicCallCount -ne $intrinsics.Count -or
         $module.intrinsicOperandCount -ne $operands.Count ) {
        throw "XLIR typed IntrinsicCall arena count is inconsistent: $($case.Name)"
    }
    foreach ( $kind in $case.Kinds ) {
        if ( @($intrinsics | Where-Object { $_.kindName -eq $kind }).Count -eq 0 ) {
            throw "XLIR typed IntrinsicCall identity is absent for $($case.Name): $kind"
        }
    }
    $callOrigins = @($module.callSites | ForEach-Object { $_.origin })
    foreach ( $intrinsic in $intrinsics ) {
        if ( $intrinsic.admissionName -ne "verified" -or
             $intrinsic.operandCount -lt 1 -or $intrinsic.firstOperand -lt 0 -or
             $intrinsic.firstOperand + $intrinsic.operandCount -gt $operands.Count -or
             $callOrigins -contains $intrinsic.origin ) {
            throw "XLIR typed IntrinsicCall has no verified admission or has an invalid/overlapping slice: $($case.Name)"
        }
        $expectedResult = switch ( $intrinsic.kindName ) {
            "string-convert" { "owned" }
            "copy" { "owned" }
            "clone" { "owned" }
            "xcall" { "owned" }
            "range" { "descriptor" }
            default { "value" }
        }
        $expectedError = if ( $intrinsic.kindName -eq "checked-integer-convert" -or
                              $intrinsic.kindName -eq "xcall" ) { "may-raise" } else { "no-raise" }
        if ( $intrinsic.resultAbiName -ne $expectedResult -or
             $intrinsic.errorAbiName -ne $expectedError ) {
            throw "XLIR typed IntrinsicCall result/effect ABI is inconsistent: $($case.Name)/$($intrinsic.kindName)"
        }
        if ( $intrinsic.kindName -eq "trivial-integer-convert" -and
             $intrinsic.integerTargetName -eq "none" ) {
            throw "XLIR trivial integer IntrinsicCall lost its exact target ABI: $($case.Name)"
        }
    }

    & $xl ir $source --lowered --format text --emit $loweredText
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR IntrinsicCall lowering failed: $($case.Name)"
    }
    $lowered = Read-Utf8 $loweredText
    foreach ( $kind in $case.Kinds ) {
        if ( ! $lowered.Contains("kind=$kind") ) {
            throw "XLIR lowered module lost typed IntrinsicCall identity: $($case.Name)/$kind"
        }
    }
}

$lower = Read-Utf8 $lowerSource
foreach ( $spelling in @("int8", "uint8", "int16", "uint16", "int32", "uint32", "int64", "string", "copy", "clone", "append", "range", "xcall") ) {
    if ( $lower -match ('strcmp\s*\([^\r\n]*"' + [regex]::Escape($spelling) + '"') ) {
        throw "XLIR lowering still classifies an intrinsic from source spelling: $spelling"
    }
}
$cgen = Read-Utf8 $cgenSource
foreach ( $include in [regex]::Matches(
    $cgen, '#include "(?<path>src/cgen/xl6_ir_cgen[^\"]+\.c)"') ) {
    $cgen += "`n" + (Read-Utf8 (
        (Join-Path $Root ($include.Groups["path"].Value -replace '/', '\'))))
}
foreach ( $forbidden in @("XL6_IrIntrinsicCall", "intrinsicCalls", "intrinsicOperands") ) {
    if ( $cgen.Contains($forbidden) ) {
        throw "XLIR Cgen must not consume Typed IntrinsicCall metadata: $forbidden"
    }
}

$rangeSource = Join-Path $Root "test\cases\xlir_intrinsic_range.xl"
$rangeC = Join-Path $outputDir "xlir_intrinsic_range.c"
$rangeExe = Join-Path $outputDir "xlir_intrinsic_range.exe"
& $xl cgen $rangeSource --backend xlir --emit $rangeC
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR range IntrinsicCall C generation failed"
}
$command = 'call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_GENERATED_MODULE_PREINCLUDED -include build\tmp\xlir_intrinsic_range.c test\cases\xlir_generated_module_host.c !XL6_CGEN_LINKS! -o build\tmp\xlir_intrinsic_range.exe && build\tmp\xlir_intrinsic_range.exe'
& $env:ComSpec /d /v:on /c $command
if ( $LASTEXITCODE -ne 0 -or ! (Test-Path -LiteralPath $rangeExe -PathType Leaf) ) {
    throw "XLIR range IntrinsicCall strict-C runtime failed"
}

$identitySource = Join-Path $Root "test\cases\xlir_intrinsic_trivial_identity_convert.xl"
$identityHost = Join-Path $Root "test\cases\xlir_intrinsic_trivial_identity_convert_host.c"
$identityC = Join-Path $outputDir "xlir_intrinsic_trivial_identity_convert.c"
$identityExe = Join-Path $outputDir "xlir_intrinsic_trivial_identity_convert.exe"
& $xl cgen $identitySource --backend xlir --emit $identityC
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR trivial identity IntrinsicCall C generation failed"
}
$identityText = Read-Utf8 $identityC
if ( ! $identityText.Contains("xlir_identity_point_source") -or
     ! $identityText.Contains("xlir_expect_same_point") ) {
    throw "XLIR trivial identity generated C lost its exact point ABI"
}
$command = 'call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_GENERATED_MODULE_PREINCLUDED -include build\tmp\xlir_intrinsic_trivial_identity_convert.c test\cases\xlir_intrinsic_trivial_identity_convert_host.c test\cases\xlir_generated_module_host.c !XL6_CGEN_LINKS! -o build\tmp\xlir_intrinsic_trivial_identity_convert.exe && build\tmp\xlir_intrinsic_trivial_identity_convert.exe'
& $env:ComSpec /d /v:on /c $command
if ( $LASTEXITCODE -ne 0 -or ! (Test-Path -LiteralPath $identityExe -PathType Leaf) ) {
    throw "XLIR trivial identity IntrinsicCall strict-C runtime failed"
}

Write-Output "XLIR IntrinsicCall gate ok: typed identity/admission separation including nominal-class xcall, operand/result/effect ABI, generic-ready arena, spelling-free lowering, instruction-only Cgen, range and trivial-identity strict C/runtime."
