param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\xlir_u64_scalar.xl"
$hostSource = Join-Path $Root "test\cases\xlir_u64_scalar_host.c"
$asyncSource = Join-Path $Root "test\cases\xlir_u64_async_closure.xl"
$asyncHostSource = Join-Path $Root "test\cases\xlir_u64_async_closure_host.c"
$genericSource = Join-Path $Root "test\cases\xlir_u64_generic_intrinsic.xl"
$genericHostSource = Join-Path $Root "test\cases\xlir_u64_generic_intrinsic_host.c"
$dynamicSource = Join-Path $Root "test\cases\xlir_u64_dynamic.xl"
$dynamicHostSource = Join-Path $Root "test\cases\xlir_u64_dynamic_host.c"
$outputDir = Join-Path $Root "build\test"
$typedTextPath = Join-Path $outputDir "xlir_u64_scalar.typed.xlir"
$typedJsonPath = Join-Path $outputDir "xlir_u64_scalar.typed.json"
$loweredTextPath = Join-Path $outputDir "xlir_u64_scalar.lowered.xlir"
$loweredJsonPath = Join-Path $outputDir "xlir_u64_scalar.lowered.json"
$cPath = Join-Path $outputDir "xlir_u64_scalar_generated.c"
$headerPath = Join-Path $outputDir "xlir_u64_scalar_generated.h"
$exePath = Join-Path $outputDir "xlir_u64_scalar_host.exe"
$asyncLoweredPath = Join-Path $outputDir "xlir_u64_async_closure.lowered.xlir"
$asyncCPath = Join-Path $outputDir "xlir_u64_async_closure_generated.c"
$asyncExePath = Join-Path $outputDir "xlir_u64_async_closure_host.exe"
$genericTypedPath = Join-Path $outputDir "xlir_u64_generic_intrinsic.typed.xlir"
$genericLoweredPath = Join-Path $outputDir "xlir_u64_generic_intrinsic.lowered.xlir"
$genericCPath = Join-Path $outputDir "xlir_u64_generic_intrinsic_generated.c"
$genericExePath = Join-Path $outputDir "xlir_u64_generic_intrinsic_host.exe"
$dynamicTypedPath = Join-Path $outputDir "xlir_u64_dynamic.typed.xlir"
$dynamicLoweredPath = Join-Path $outputDir "xlir_u64_dynamic.lowered.xlir"
$dynamicCPath = Join-Path $outputDir "xlir_u64_dynamic_generated.c"
$dynamicExePath = Join-Path $outputDir "xlir_u64_dynamic_host.exe"

foreach ( $path in @($xl, $source, $hostSource, $asyncSource, $asyncHostSource,
                      $genericSource, $genericHostSource, $dynamicSource,
                      $dynamicHostSource) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR U64 ABI gate input is missing: $path"
    }
}
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

& $xl ir $source --format text --emit $typedTextPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR U64 typed text IR failed" }
$typedText = [System.IO.File]::ReadAllText($typedTextPath, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "cabi=34 native-slot=u64 value-admission=10/0/0x7A24932689D8B089",
    "callable=invalid ownership=1 null=0 nominal=s0 typeValue=0x7A24932689D8B089 name=uint64 display=uint64 integer=u64",
    "kind=trivial-integer-convert admission=verified result=t3 resultAbi=value",
    "kind=string-convert admission=verified"
) ) {
    if ( ! $typedText.Contains($required) ) {
        throw "XLIR U64 typed identity is incomplete: $required"
    }
}

& $xl ir $source --format json --emit $typedJsonPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR U64 typed JSON IR failed" }
$typedJson = [System.IO.File]::ReadAllText($typedJsonPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$u64Type = @($typedJson.types | Where-Object { $_.cAbi -eq 34 })
$trivial = @($typedJson.intrinsicCalls | Where-Object { $_.kindName -eq "trivial-integer-convert" })
if ( $u64Type.Count -ne 1 -or $u64Type[0].ownershipAbi -ne 1 -or
     $trivial.Count -ne 2 -or
     @($trivial | Where-Object { $_.integerTargetName -ne "u64" -or
                                 $_.admissionName -ne "verified" }).Count -ne 0 ) {
    throw "XLIR U64 typed JSON has no exact execution or literal-conversion ABI"
}

& $xl ir $source --lowered --format text --emit $loweredTextPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR U64 lowered text IR failed" }
$loweredText = [System.IO.File]::ReadAllText($loweredTextPath, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "value=18446744073709551615ULL",
    "value=0ULL",
    "immediate=9223372036854775808ULL",
    "immediate=2026ULL",
    "binary opcode=add",
    "runtime-call opcode=none runtime=integer-div-checked",
    "binary opcode=ge",
    "runtime-call opcode=none runtime=integer-shr-checked",
    "binary opcode=bit-and",
    "unary opcode=bit-not",
    "runtime-call opcode=none runtime=u64-to-string",
    "extern-global x0"
) ) {
    if ( ! $loweredText.Contains($required) ) {
        throw "XLIR U64 lowered ABI is incomplete: $required"
    }
}

& $xl ir $source --lowered --format json --emit $loweredJsonPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR U64 lowered JSON IR failed" }
$loweredJson = [System.IO.File]::ReadAllText($loweredJsonPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$stringOp = @($loweredJson.instructions | Where-Object { $_.runtimeOpName -eq "u64-to-string" })
if ( $loweredJson.globalExportCount -ne 2 -or $loweredJson.externGlobalCount -ne 1 -or
     $stringOp.Count -ne 1 -or $stringOp[0].runtimeOp -le 0 ) {
    throw "XLIR U64 lowered JSON has no exact global or runtime-operation ABI"
}

& $xl cgen $source --backend xlir --emit $cPath --emit-header $headerPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR U64 Cgen failed" }
$generated = [System.IO.File]::ReadAllText($cPath, [System.Text.Encoding]::UTF8)
$header = [System.IO.File]::ReadAllText($headerPath, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "typedef struct xlir_result_u64 { bool ok; uint64_t value; } xlir_result_u64;",
    "extern bool xrtUIntFormatTo(uint64_t, xlir_string_view, char*, size_t, size_t*);",
    "static char* xlir_u64_to_string(uint64_t value)",
    "uint64_t xlir_u64_maximum = 18446744073709551615ULL;",
    "extern uint64_t xlir_u64_external;",
    "if (!result.ok) xl6ErrorScopeCaptureFailure(&entry);",
    "else pending_result = result.value;",
    "outcome = xl6ForeignCallEnd(&entry);",
    "xl6ErrorScopePropagateFailure(outcome);",
    "return (uint64_t){0};",
    "return pending_result;"
) ) {
    if ( ! $generated.Contains($required) ) {
        throw "XLIR U64 C emission is incomplete: $required"
    }
}
foreach ( $required in @(
    "extern uint64_t xlir_u64_maximum;",
    "uint64_t xlir_u64_increment(uint64_t value);",
    "uint64_t xlir_u64_bool_convert(bool value);",
    "uint64_t xlir_u64_fail(void);"
) ) {
    if ( ! $header.Contains($required) ) {
        throw "XLIR U64 public header is incomplete: $required"
    }
}
$command = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror $hostSource !XL6_CGEN_LINKS! -o $exePath && $exePath"
& $env:ComSpec /d /v:on /c $command
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR U64 generated-C runtime failed" }

& $xl ir $asyncSource --lowered --format text --emit $asyncLoweredPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR U64 async/closure lowering failed" }
$asyncLowered = [System.IO.File]::ReadAllText($asyncLoweredPath, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "async-result=copy-u64",
    "async=copy-u64",
    "cell-new",
    "cell-load",
    "cell-store"
) ) {
    if ( ! $asyncLowered.Contains($required) ) {
        throw "XLIR U64 async/closure ABI is incomplete: $required"
    }
}

& $xl cgen $asyncSource --backend xlir --emit $asyncCPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR U64 async/closure Cgen failed" }
$asyncGenerated = [System.IO.File]::ReadAllText($asyncCPath, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "xlir_future_wait_checked(",
    "uint64_t const* payload = (uint64_t const*)(xrtFutureValue(",
    "(xl6CellRuntimeV1Get()->Create)(XL6_CELL_U64, sizeof(uint64_t));",
    "(xl6CellRuntimeV1Get()->Load)(",
    "(xl6CellRuntimeV1Get()->Store)(",
    "uint64_t* value;"
) ) {
    if ( ! $asyncGenerated.Contains($required) ) {
        throw "XLIR U64 async/closure C emission is incomplete: $required"
    }
}
$asyncCommand = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror $asyncHostSource !XL6_CGEN_LINKS! -o $asyncExePath && $asyncExePath"
& $env:ComSpec /d /v:on /c $asyncCommand
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR U64 async/closure generated-C runtime failed" }

& $xl ir $genericSource --format text --emit $genericTypedPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR U64 generic typed IR failed" }
$genericTyped = [System.IO.File]::ReadAllText($genericTypedPath, [System.Text.Encoding]::UTF8)
if ( $genericTyped.Contains("admission=unsupported-abi") -or
     ([regex]::Matches($genericTyped, "kind=trivial-integer-convert admission=verified")).Count -ne 3 -or
     ([regex]::Matches($genericTyped, "kind=string-convert admission=verified")).Count -ne 1 ) {
    throw "XLIR U64 generic specialization did not preserve exact intrinsic admission"
}

& $xl ir $genericSource --lowered --format text --emit $genericLoweredPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR U64 generic specialization lowering failed" }
$genericLowered = [System.IO.File]::ReadAllText($genericLoweredPath, [System.Text.Encoding]::UTF8)
foreach ( $required in @("immediate=2026ULL", "immediate=18446744073709551615ULL",
                          "runtime=u64-to-string") ) {
    if ( ! $genericLowered.Contains($required) ) {
        throw "XLIR U64 generic lowered ABI is incomplete: $required"
    }
}

& $xl cgen $genericSource --backend xlir --emit $genericCPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR U64 generic specialization Cgen failed" }
$genericCommand = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror $genericHostSource !XL6_CGEN_LINKS! -o $genericExePath && $genericExePath"
& $env:ComSpec /d /v:on /c $genericCommand
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR U64 generic generated-C runtime failed" }

& $xl ir $dynamicSource --format text --emit $dynamicTypedPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR dynamic U64 typed IR failed" }
$dynamicTyped = [System.IO.File]::ReadAllText($dynamicTypedPath, [System.Text.Encoding]::UTF8)
if ( $dynamicTyped.Contains("admission=unsupported-abi") -or
     ([regex]::Matches($dynamicTyped, "kind=checked-integer-convert admission=verified result=t[0-9]+ resultAbi=value error=may-raise.*integer=u64")).Count -lt 3 -or
     ! $dynamicTyped.Contains("kind=xcall admission=verified") ) {
    throw "XLIR dynamic U64 typed intrinsic identity is incomplete"
}

& $xl ir $dynamicSource --lowered --format text --emit $dynamicLoweredPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR dynamic U64 lowering failed" }
$dynamicLowered = [System.IO.File]::ReadAllText($dynamicLoweredPath, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "runtime=xvalue-u64",
    "runtime=xvalue-array-at-i64-u64",
    "runtime=xvalue-int-map-at-i64-u64",
    "runtime=xvalue-object-get-literal-u64",
    "runtime=xvalue-binary-add",
    "runtime=xvalue-binary-sub",
    "runtime=xvalue-binary-mul",
    "runtime=integer-convert-checked",
    "runtime=xvalue-callable-from-closure",
    "runtime=xvalue-dynamic-call",
    "runtime=future-all-xvalues"
) ) {
    if ( ! $dynamicLowered.Contains($required) ) {
        throw "XLIR dynamic U64 lowered ABI is incomplete: $required"
    }
}

& $xl cgen $dynamicSource --backend xlir --emit $dynamicCPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR dynamic U64 Cgen failed" }
$dynamicGenerated = [System.IO.File]::ReadAllText($dynamicCPath, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "extern xvalue* xrtValueUInt(uint64_t);",
    "extern bool xrtValueGetUInt(const xvalue*, uint64_t*);",
    "extern bool xl6IntegerValueConvertV1(const xvalue*, unsigned int, uint64_t, void*)",
    "xl6ArrayReadU64V1(v",
    "static uint64_t xlir_xvalue_int_map_at_i64_u64",
    "static uint64_t xlir_xvalue_object_get_literal_u64",
    "xlir_xvalue_binary(v",
    "if (frame->ArgumentCount > 0) { f0 = frame->Arguments[0]; bound0 = true; }",
    "if (bound0) { if (!xl6ValueReadIntegerV1(f0, 10, 0x7A24932689D8B089ULL, false, &a0)) goto argument_fail; }",
    "boxed = xl6ValueBoxIntegerV1(&value, 10, 0x7A24932689D8B089ULL, false);",
    "return xrtValueUInt(*(const uint64_t*)payload);",
    "xl6FutureAggregateAllV2("
) ) {
    if ( ! $dynamicGenerated.Contains($required) ) {
        throw "XLIR dynamic U64 C emission is incomplete: $required"
    }
}
$dynamicCommand = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror $dynamicHostSource !XL6_CGEN_XLANG_LINKS! -o $dynamicExePath && $dynamicExePath"
& $env:ComSpec /d /v:on /c $dynamicCommand
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR dynamic U64 generated-C runtime failed" }

Write-Output "XLIR U64 ABI gate ok: lossless scalar, generic, async/capture, dynamic xvalue arithmetic, typed container/class projection, checked conversion, callable, strict C, and runtime ABIs."
exit 0
