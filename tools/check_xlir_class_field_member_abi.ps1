param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\xlir_class_field_member_abi.xl"
$hostSource = Join-Path $Root "test\cases\xlir_class_field_member_abi_host.c"
$genericHost = Join-Path $Root "test\cases\xlir_generated_module_host.c"
$outputDir = Join-Path $Root "build\test"
$typedJsonPath = Join-Path $outputDir "xlir_class_field_member_abi.typed.json"
$loweredJsonPath = Join-Path $outputDir "xlir_class_field_member_abi.lowered.json"
$cPath = Join-Path $outputDir "xlir_class_field_member_abi_generated.c"
$exePath = Join-Path $outputDir "xlir_class_field_member_abi_host.exe"

foreach ( $path in @($xl, $source, $hostSource, $genericHost) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR class-field member ABI gate input is missing: $path"
    }
}
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

& $xl ir $source --format json --emit $typedJsonPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR class-field member ABI typed JSON failed" }
$typed = [System.IO.File]::ReadAllText(
    $typedJsonPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$expectedMembers = [ordered]@{
    "class-field-bool" = 2
    "class-field-i64" = 2
    "class-field-u64" = 2
    "class-field-f64" = 2
    "class-field-time" = 2
    "class-field-string-copy" = 2
    "class-field-raw-pointer-borrow" = 2
    "class-field-xvalue-borrow" = 9
}
foreach ( $name in $expectedMembers.Keys ) {
    if ( @($typed.nodes | Where-Object { $_.memberAbiName -eq $name }).Count -ne
         $expectedMembers[$name] ) {
        throw "Typed XLIR class-field MemberAbi count is not exact for $name"
    }
}
if ( @($typed.nodes | Where-Object {
        $_.memberAbiName -ne "none" -and
        ! $expectedMembers.Contains($_.memberAbiName)
    }).Count -ne 0 ) {
    throw "Typed XLIR contains an unexpected non-canonical member ABI"
}

& $xl ir $source --lowered --format json --emit $loweredJsonPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR class-field member ABI lowered JSON failed" }
$lowered = [System.IO.File]::ReadAllText(
    $loweredJsonPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$expectedOps = [ordered]@{
    "xvalue-object-get-literal-bool" = 1
    "xvalue-object-get-literal-i64" = 1
    "xvalue-object-get-literal-u64" = 1
    "xvalue-object-get-literal-f64" = 1
    "xvalue-object-get-literal-time" = 1
    "xvalue-object-get-literal-string-copy" = 1
    "xvalue-object-get-literal" = 7
    "xvalue-object-get-literal-raw-pointer" = 3
    "xvalue-object-get-literal-native-handle-borrow" = 12
}
foreach ( $name in $expectedOps.Keys ) {
    $ops = @($lowered.instructions | Where-Object { $_.runtimeOpName -eq $name })
    if ( $ops.Count -ne $expectedOps[$name] -or @($ops | Where-Object {
            $_.kindName -ne "runtime-call" -or $_.operandCount -ne 1 -or
            $_.hasImmediate -ne 1 -or $_.targetSymbol -ne 0 -or
            $_.targetValue -ne 0 -or $_.calleeType -ne 0 -or
            $_.payloadType -ne 0 -or $_.opcodeName -ne "none"
        }).Count -ne 0 ) {
        throw "Lowered XLIR class-field runtime operation is not exact for $name"
    }
}
$rawBoxes = @($lowered.instructions | Where-Object {
    $_.runtimeOpName -eq "xvalue-raw-pointer-borrow"
})
if ( $rawBoxes.Count -ne 2 -or @($rawBoxes | Where-Object {
        $_.kindName -ne "runtime-call" -or $_.operandCount -ne 1 -or
        $_.hasImmediate -ne 0 -or $_.targetSymbol -ne 0 -or
        $_.targetValue -ne 0 -or $_.calleeType -ne 0 -or
        $_.payloadType -ne 0 -or $_.opcodeName -ne "none"
    }).Count -ne 0 ) {
    throw "Lowered XLIR raw pointer boxing has no exact borrowed ABI"
}

& $xl cgen $source --backend xlir --emit $cPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR class-field member ABI Cgen failed" }
$generated = [System.IO.File]::ReadAllText($cPath, [System.Text.Encoding]::UTF8)
foreach ( $helper in @(
    "xlir_xvalue_object_get_literal_bool",
    "xlir_xvalue_object_get_literal_i64",
    "xlir_xvalue_object_get_literal_u64",
    "xlir_xvalue_object_get_literal_f64",
    "xlir_xvalue_object_get_literal_time",
    "xlir_xvalue_object_get_literal_string_copy",
    "xlir_xvalue_object_get_literal_or_null",
    "xlir_xvalue_object_get_literal_raw_pointer",
    "xlir_xvalue_object_get_literal_native_handle"
) ) {
    if ( ! $generated.Contains($helper) ) {
        throw "XLIR class-field C rendering is missing mechanical helper: $helper"
    }
}
if ( ([regex]::Matches($generated, "xlir_xvalue_object_get_literal_or_null\(v")).Count -ne 3 -or
     ([regex]::Matches($generated, "xlir_xvalue_object_get_literal_raw_pointer\(v")).Count -ne 3 -or
     ([regex]::Matches($generated, "xlir_xvalue_object_get_literal_native_pointer\(v")).Count -ne 0 -or
     ([regex]::Matches($generated, "xlir_xvalue_object_get_literal_native_handle\(v")).Count -ne 1 -or
     ! $generated.Contains("xrtValuePointer(") -or
     $generated.Contains("memberAbi") ) {
    throw "XLIR class-field C rendering is not mechanical from verified runtime operations"
}

$command = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_GENERATED_MODULE_PREINCLUDED -include `"$cPath`" -include `"$hostSource`" `"$genericHost`" !XL6_CGEN_LINKS! -o `"$exePath`" && `"$exePath`""
& $env:ComSpec /d /v:on /c $command
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR class-field member ABI strict generated-C runtime failed"
}

Write-Output "XLIR class-field member ABI gate ok: typed scalar and managed-class field identity, nested raw/owned native projections, raw pointer boxing, mechanical C, and ownership runtime semantics are exact."
exit 0
