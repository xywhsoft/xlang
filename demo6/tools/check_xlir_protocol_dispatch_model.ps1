param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$compiler = Join-Path $Root "release\xl.exe"
$declarationFixture = Join-Path $Root "test\cases\protocol_declaration.xl"
$declarationLoweredJson = Join-Path $Root "build\tmp\xlir_protocol_declaration.lowered.json"
$declarationGeneratedC = Join-Path $Root "build\tmp\xlir_protocol_declaration.c"
$declarationRepeatC = Join-Path $Root "build\tmp\xlir_protocol_declaration.repeat.c"
$fixture = Join-Path $Root "test\cases\protocol_dynamic_dispatch.xl"
$typedIr = Join-Path $Root "build\tmp\xlir_protocol_dynamic_dispatch.ir"
$generatedC = Join-Path $Root "build\tmp\xlir_protocol_dynamic_dispatch.c"
$overloadFixture = Join-Path $Root "test\cases\protocol_overload_dispatch.xl"
$overloadTypedIr = Join-Path $Root "build\tmp\xlir_protocol_overload_dispatch.ir"
$overloadGeneratedC = Join-Path $Root "build\tmp\xlir_protocol_overload_dispatch.c"
$genericFixture = Join-Path $Root "test\cases\protocol_generic_dispatch.xl"
$genericTypedIr = Join-Path $Root "build\tmp\xlir_protocol_generic_dispatch.ir"
$genericGeneratedC = Join-Path $Root "build\tmp\xlir_protocol_generic_dispatch.c"
$propertyFixture = Join-Path $Root "test\cases\protocol_property_dispatch.xl"
$propertyTypedIr = Join-Path $Root "build\tmp\xlir_protocol_property_dispatch.ir"
$propertyGeneratedC = Join-Path $Root "build\tmp\xlir_protocol_property_dispatch.c"
$genericPropertyFixture = Join-Path $Root "test\cases\protocol_generic_property_dispatch.xl"
$genericPropertyLoweredIr = Join-Path $Root "build\tmp\xlir_protocol_generic_property_dispatch.lowered.ir"
$genericPropertyGeneratedC = Join-Path $Root "build\tmp\xlir_protocol_generic_property_dispatch.c"
$asyncFixture = Join-Path $Root "test\cases\protocol_async_dispatch.xl"
$asyncTypedIr = Join-Path $Root "build\tmp\xlir_protocol_async_dispatch.ir"
$asyncGeneratedC = Join-Path $Root "build\tmp\xlir_protocol_async_dispatch.c"
$runtimeHost = Join-Path $Root "test\cases\xlir_default_class_layout_host.c"
$programRuntimeHost = Join-Path $Root "test\cases\xlir_program_null_args_host.c"

if ( ! (Test-Path -LiteralPath $compiler -PathType Leaf) ) {
    throw "XLIR protocol dispatch gate requires release\\xl.exe"
}
if ( ! (Test-Path -LiteralPath $fixture -PathType Leaf) ) {
    throw "XLIR protocol dispatch fixture is missing"
}
if ( ! (Test-Path -LiteralPath $declarationFixture -PathType Leaf) ) {
    throw "XLIR protocol declaration fixture is missing"
}
if ( ! (Test-Path -LiteralPath $overloadFixture -PathType Leaf) ) {
    throw "XLIR protocol overload fixture is missing"
}
if ( ! (Test-Path -LiteralPath $genericFixture -PathType Leaf) ) {
    throw "XLIR generic protocol fixture is missing"
}
if ( ! (Test-Path -LiteralPath $propertyFixture -PathType Leaf) ) {
    throw "XLIR protocol property fixture is missing"
}
if ( ! (Test-Path -LiteralPath $genericPropertyFixture -PathType Leaf) ) {
    throw "XLIR generic protocol property fixture is missing"
}
if ( ! (Test-Path -LiteralPath $asyncFixture -PathType Leaf) ) {
    throw "XLIR async protocol fixture is missing"
}

# A declaration does not request executable dispatch.  In particular an
# unused generic requirement can remain compile-time metadata without an
# invented witness table; only a later PROTOCOL_CALL may name a verified slot.
& $compiler ir $declarationFixture --lowered --format json --emit $declarationLoweredJson
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR protocol declaration lowering failed" }
$declarationIr = [System.IO.File]::ReadAllText(
    $declarationLoweredJson, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$protocolDeclarations = @($declarationIr.nodes | Where-Object {
    $_.sourceKindName -eq "protocol"
})
if ( $protocolDeclarations.Count -ne 2 -or
     [int]$declarationIr.protocolDispatchSlotCount -ne 0 -or
     @($declarationIr.instructions | Where-Object {
         $_.kindName -eq "protocol-call"
     }).Count -ne 0 ) {
    throw "XLIR declaration-only protocols manufactured an executable dispatch ABI"
}
& $compiler cgen $declarationFixture --backend xlir --emit $declarationGeneratedC
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR protocol declaration C generation failed" }
& $compiler cgen $declarationFixture --backend xlir --emit $declarationRepeatC
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR protocol declaration repeated C generation failed" }
$declarationBytes = [System.IO.File]::ReadAllBytes($declarationGeneratedC)
$declarationRepeatBytes = [System.IO.File]::ReadAllBytes($declarationRepeatC)
if ( $declarationBytes.Length -ne $declarationRepeatBytes.Length -or
     -not [System.Linq.Enumerable]::SequenceEqual(
         [byte[]]$declarationBytes, [byte[]]$declarationRepeatBytes) ) {
    throw "XLIR protocol declaration C is not byte-for-byte deterministic"
}
$declarationCText = [System.IO.File]::ReadAllText(
    $declarationGeneratedC, [System.Text.Encoding]::UTF8)
if ( $declarationCText.Contains("xlir_pdispatch_") ) {
    throw "XLIR declaration-only protocol emitted an unrequested dispatcher"
}
$declarationCommand = 'call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_GENERATED_MODULE_PREINCLUDED -include build\tmp\xlir_protocol_declaration.c test\cases\xlir_program_null_args_host.c !XL6_CGEN_LINKS! -o build\tmp\xlir_protocol_declaration.exe && build\tmp\xlir_protocol_declaration.exe'
& $env:ComSpec /d /v:on /c $declarationCommand
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR protocol declaration strict-C/runtime gate failed"
}

Remove-Item -LiteralPath $typedIr -Force -ErrorAction SilentlyContinue
& $compiler ir $fixture --format text --emit $typedIr
if ( $LASTEXITCODE -ne 0 -or ! (Test-Path -LiteralPath $typedIr -PathType Leaf) ) {
    throw "XLIR protocol typed-IR dump failed"
}

$text = [System.IO.File]::ReadAllText($typedIr, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "protocolDispatchSlots=1",
    "protocolDispatchImpls=1",
    "protocol-dispatch-slot d0 protocol=t5/n2 declaration=n4 symbol=1 return=t3 params=[] name=name",
    "protocol-dispatch-impl d0 owner=t4/n12 implementation=n27 symbol=4"
) ) {
    if ( ! $text.Contains($required) ) {
        throw "XLIR protocol typed ABI is incomplete: $required"
    }
}
if ( $text -notmatch 'type t5 kind=10 collection=none resource=none persistence=xrt-clone/t0 cabi=26 callable=invalid ownership=2 null=2 nominal=s0 typeValue=0x[0-9A-F]{16} name=Named display=Named integer=none params=\[\]' ) {
    throw "XLIR protocol typed ABI has no exact nominal xvalue handle representation"
}

Write-Output "XLIR protocol typed dispatch model ok."

Remove-Item -LiteralPath $generatedC -Force -ErrorAction SilentlyContinue
& $compiler cgen $fixture --backend xlir --emit $generatedC
if ( $LASTEXITCODE -ne 0 -or ! (Test-Path -LiteralPath $generatedC -PathType Leaf) ) {
    throw "XLIR protocol C generation failed"
}
$cText = [System.IO.File]::ReadAllText($generatedC, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "xlir_pdispatch_1",
    "case 1: return xlir_fn_4(receiver);",
    "xlir_xvalue_object_get_literal_string_copy"
) ) {
    if ( ! $cText.Contains($required) ) {
        throw "XLIR protocol generated C has no verified dispatch ABI: $required"
    }
}
if ( $cText -notmatch '(?m)^\s+v\d+ = xlir_pdispatch_1\(v\d+\);$' ) {
    throw "XLIR protocol generated C has no verified dispatch call instruction"
}

if ( ! (Test-Path -LiteralPath $runtimeHost -PathType Leaf) ) {
    throw "XLIR protocol runtime host is missing"
}
$command = 'call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include build\tmp\xlir_protocol_dynamic_dispatch.c test\cases\xlir_default_class_layout_host.c !XL6_CGEN_LINKS! -o build\tmp\xlir_protocol_dynamic_dispatch.exe && build\tmp\xlir_protocol_dynamic_dispatch.exe'
& $env:ComSpec /d /v:on /c $command
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR protocol generated-C runtime gate failed"
}

# A protocol conversion is valid only with a complete witness table.  This
# fixture covers that conversion plus independent scalar VALUE and managed
# string BORROW requirement ABIs; Cgen receives both dispatch signatures from
# XLIR and never discovers the concrete method from source syntax.
Remove-Item -LiteralPath $overloadTypedIr -Force -ErrorAction SilentlyContinue
& $compiler ir $overloadFixture --format text --emit $overloadTypedIr
if ( $LASTEXITCODE -ne 0 -or ! (Test-Path -LiteralPath $overloadTypedIr -PathType Leaf) ) {
    throw "XLIR protocol overload typed-IR dump failed"
}
$overloadText = [System.IO.File]::ReadAllText($overloadTypedIr, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "protocolDispatchSlots=2",
    "protocolDispatchImpls=2",
    "protocol-dispatch-slot d0 protocol=t6/n2 declaration=n4 symbol=1 return=t3 params=[t3/value,t3/value] name=add",
    "protocol-dispatch-slot d1 protocol=t6/n2 declaration=n15 symbol=2 return=t4 params=[t4/borrow,t4/borrow] name=add"
) ) {
    if ( ! $overloadText.Contains($required) ) {
        throw "XLIR protocol overload typed ABI is incomplete: $required"
    }
}

Remove-Item -LiteralPath $overloadGeneratedC -Force -ErrorAction SilentlyContinue
& $compiler cgen $overloadFixture --backend xlir --emit $overloadGeneratedC
if ( $LASTEXITCODE -ne 0 -or ! (Test-Path -LiteralPath $overloadGeneratedC -PathType Leaf) ) {
    throw "XLIR protocol overload C generation failed"
}
$overloadCText = [System.IO.File]::ReadAllText($overloadGeneratedC, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "static int64_t xlir_pdispatch_1(xvalue* receiver, int64_t a0, int64_t a1)",
    "case 1: return xlir_fn_4(receiver, a0, a1);",
    "static char* xlir_pdispatch_2(xvalue* receiver, char* a0, char* a1)",
    "case 1: return xlir_fn_5(receiver, a0, a1);"
) ) {
    if ( ! $overloadCText.Contains($required) ) {
        throw "XLIR protocol overload generated C has no verified dispatch ABI: $required"
    }
}
$overloadCommand = 'call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include build\tmp\xlir_protocol_overload_dispatch.c test\cases\xlir_default_class_layout_host.c !XL6_CGEN_LINKS! -o build\tmp\xlir_protocol_overload_dispatch.exe && build\tmp\xlir_protocol_overload_dispatch.exe'
& $env:ComSpec /d /v:on /c $overloadCommand
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR protocol overload generated-C runtime gate failed"
}

# A generic protocol requirement is not a direct generic function call.  The
# concrete protocol instance owns the substituted slot signature, and its
# witness table selects the matching class method.  This catches both accidental
# requirement specialization and Cgen rediscovery of generic parameter ABI.
Remove-Item -LiteralPath $genericTypedIr -Force -ErrorAction SilentlyContinue
& $compiler ir $genericFixture --format text --emit $genericTypedIr
if ( $LASTEXITCODE -ne 0 -or ! (Test-Path -LiteralPath $genericTypedIr -PathType Leaf) ) {
    throw "XLIR generic protocol typed-IR dump failed"
}
$genericText = [System.IO.File]::ReadAllText($genericTypedIr, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "protocolDispatchSlots=1",
    "protocolDispatchImpls=1",
    "protocol-dispatch-slot d0 protocol=t5/n2 declaration=n4 symbol=1 return=t4 params=[t4/value] name=transform",
    "protocol-dispatch-impl d0 owner=t7/n15 implementation=n18 symbol=3"
) ) {
    if ( ! $genericText.Contains($required) ) {
        throw "XLIR generic protocol typed ABI is incomplete: $required"
    }
}
if ( $genericText -notmatch 'type t5 kind=6 collection=none resource=none persistence=xrt-clone/t0 cabi=26 callable=invalid ownership=2 null=2 nominal=s0 typeValue=0x[0-9A-F]{16} name=Transformer display=Transformer<int> integer=none params=\[t4\]' ) {
    throw "XLIR generic protocol instance has no canonical xvalue representation"
}

Remove-Item -LiteralPath $genericGeneratedC -Force -ErrorAction SilentlyContinue
& $compiler cgen $genericFixture --backend xlir --emit $genericGeneratedC
if ( $LASTEXITCODE -ne 0 -or ! (Test-Path -LiteralPath $genericGeneratedC -PathType Leaf) ) {
    throw "XLIR generic protocol C generation failed"
}
$genericCText = [System.IO.File]::ReadAllText($genericGeneratedC, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "static int64_t xlir_pdispatch_1(xvalue* receiver, int64_t a0)",
    "case 1: return xlir_fn_3(receiver, a0);"
) ) {
    if ( ! $genericCText.Contains($required) ) {
        throw "XLIR generic protocol generated C has no concrete witness ABI: $required"
    }
}
if ( $genericCText -notmatch
        '(?m)^\s+v\d+ = xlir_pdispatch_1\(v\d+, v\d+\);$' ) {
    throw "XLIR generic protocol generated C has no concrete witness call instruction"
}
$genericCommand = 'call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include build\tmp\xlir_protocol_generic_dispatch.c test\cases\xlir_default_class_layout_host.c !XL6_CGEN_LINKS! -o build\tmp\xlir_protocol_generic_dispatch.exe && build\tmp\xlir_protocol_generic_dispatch.exe'
& $env:ComSpec /d /v:on /c $genericCommand
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR generic protocol generated-C runtime gate failed"
}

# Getter and setter requirements are distinct witness slots.  The helper which
# invokes them also contains an assert, so its void return crosses the explicit
# MAY_RAISE carrier path; this gate prevents either property dispatch or the
# result-void control ABI from drifting back into Cgen inference.
Remove-Item -LiteralPath $propertyTypedIr -Force -ErrorAction SilentlyContinue
& $compiler ir $propertyFixture --format text --emit $propertyTypedIr
if ( $LASTEXITCODE -ne 0 -or ! (Test-Path -LiteralPath $propertyTypedIr -PathType Leaf) ) {
    throw "XLIR protocol property typed-IR dump failed"
}
$propertyText = [System.IO.File]::ReadAllText($propertyTypedIr, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "protocolDispatchSlots=2",
    "protocolDispatchImpls=2",
    "protocol-dispatch-slot d0 protocol=t5/n2 declaration=n4 symbol=1 return=t3 params=[] name=name",
    "protocol-dispatch-slot d1 protocol=t5/n2 declaration=n10 symbol=2 return=t0 params=[t3/borrow] name=name"
) ) {
    if ( ! $propertyText.Contains($required) ) {
        throw "XLIR protocol property typed ABI is incomplete: $required"
    }
}

Remove-Item -LiteralPath $propertyGeneratedC -Force -ErrorAction SilentlyContinue
& $compiler cgen $propertyFixture --backend xlir --emit $propertyGeneratedC
if ( $LASTEXITCODE -ne 0 -or ! (Test-Path -LiteralPath $propertyGeneratedC -PathType Leaf) ) {
    throw "XLIR protocol property C generation failed"
}
$propertyCText = [System.IO.File]::ReadAllText($propertyGeneratedC, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "typedef struct xlir_result_void { bool ok; } xlir_result_void;",
    "static char* xlir_pdispatch_1(xvalue* receiver)",
    "static void xlir_pdispatch_2(xvalue* receiver, char* a0)",
    "xlir_result_void r",
    ".ok) goto"
) ) {
    if ( ! $propertyCText.Contains($required) ) {
        throw "XLIR protocol property generated C has no explicit witness/result-void ABI: $required"
    }
}
$propertyCommand = 'call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include build\tmp\xlir_protocol_property_dispatch.c test\cases\xlir_default_class_layout_host.c !XL6_CGEN_LINKS! -o build\tmp\xlir_protocol_property_dispatch.exe && build\tmp\xlir_protocol_property_dispatch.exe'
& $env:ComSpec /d /v:on /c $propertyCommand
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR protocol property generated-C runtime gate failed"
}

# A generic class does not have a runtime layout until a concrete class type is
# used.  Its constructor and property witnesses must be monomorphized before
# call-target registration, and both direct property access and protocol
# dispatch must consume those same concrete callables.
Remove-Item -LiteralPath $genericPropertyLoweredIr -Force -ErrorAction SilentlyContinue
& $compiler ir $genericPropertyFixture --lowered --format text --emit $genericPropertyLoweredIr
if ( $LASTEXITCODE -ne 0 -or ! (Test-Path -LiteralPath $genericPropertyLoweredIr -PathType Leaf) ) {
    throw "XLIR generic protocol property lowered-IR dump failed"
}
$genericPropertyText = [System.IO.File]::ReadAllText($genericPropertyLoweredIr, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "protocolDispatchSlots=2",
    "protocolDispatchImpls=2",
    "class-layout c0 type=t9 declaration=n19 base=t0/n0 tag=1 fields=0..1 inherited=0 create=1",
    "class-field f0 owner=t9 type=t6 declaration=n22 initializer=n0 destroy=s0 name=stored init=scalar-zero",
    "generic-instance source=n24",
    "generic-instance source=n33",
    "generic-instance source=n42"
) ) {
    if ( ! $genericPropertyText.Contains($required) ) {
        throw "XLIR generic class/property lowered ABI is incomplete: $required"
    }
}
if ( $genericPropertyText -notmatch 'type t9 kind=2 collection=none resource=none persistence=class-schema/t0 cabi=26 callable=invalid ownership=2 null=2 nominal=s3 typeValue=0x[0-9A-F]{16} name=Box display=Box<int> integer=none params=\[t6\]' ) {
    throw "XLIR generic protocol witness class has no canonical nominal xvalue representation"
}
if ( ([regex]::Matches($genericPropertyText, "generic-instance source=n(?:24|33|42) declaration=n[0-9]+ symbol=[0-9]+ args=\[t6\]")).Count -ne 3 -or
     ([regex]::Matches($genericPropertyText, "protocol-dispatch-impl d[01] owner=t9/n19 implementation=n[0-9]+ symbol=[0-9]+")).Count -ne 2 ) {
    throw "XLIR generic class/property witnesses are not isolated concrete instances"
}

Remove-Item -LiteralPath $genericPropertyGeneratedC -Force -ErrorAction SilentlyContinue
& $compiler cgen $genericPropertyFixture --backend xlir --emit $genericPropertyGeneratedC
if ( $LASTEXITCODE -ne 0 -or ! (Test-Path -LiteralPath $genericPropertyGeneratedC -PathType Leaf) ) {
    throw "XLIR generic protocol property C generation failed"
}
$genericPropertyCText = [System.IO.File]::ReadAllText($genericPropertyGeneratedC, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "static int64_t xlir_pdispatch_1(xvalue* receiver)",
    "static void xlir_pdispatch_2(xvalue* receiver, int64_t a0)",
    "typedef struct xlir_result_void { bool ok; } xlir_result_void;",
    "xlir_result_void r",
    ".ok) goto"
) ) {
    if ( ! $genericPropertyCText.Contains($required) ) {
        throw "XLIR generic protocol property generated C has no concrete witness ABI: $required"
    }
}
if ( ([regex]::Matches($genericPropertyCText, "case 1: return xlir_fn_[0-9]+\(receiver\);")).Count -lt 1 -or
     ([regex]::Matches($genericPropertyCText, "case 1: xlir_fn_[0-9]+\(receiver, a0\);")).Count -lt 1 ) {
    throw "XLIR generic protocol property generated C has no concrete dispatch rows"
}
$genericPropertyCommand = 'call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include build\tmp\xlir_protocol_generic_property_dispatch.c test\cases\xlir_default_class_layout_host.c !XL6_CGEN_LINKS! -o build\tmp\xlir_protocol_generic_property_dispatch.exe && build\tmp\xlir_protocol_generic_property_dispatch.exe'
& $env:ComSpec /d /v:on /c $genericPropertyCommand
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR generic protocol property generated-C runtime gate failed"
}

# An async requirement dispatches the public Future ABI.  Its concrete witness
# remains an INTERNAL_ASYNC target with a worker result ABI, so the dispatcher
# must select the verified launcher rather than calling the worker directly.
Remove-Item -LiteralPath $asyncTypedIr -Force -ErrorAction SilentlyContinue
& $compiler ir $asyncFixture --format text --emit $asyncTypedIr
if ( $LASTEXITCODE -ne 0 -or ! (Test-Path -LiteralPath $asyncTypedIr -PathType Leaf) ) {
    throw "XLIR async protocol typed-IR dump failed"
}
$asyncText = [System.IO.File]::ReadAllText($asyncTypedIr, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "protocolDispatchSlots=1",
    "protocolDispatchImpls=1",
    "protocol-dispatch-slot d0 protocol=t5/n2 declaration=n4 symbol=1 return=t6 params=[] name=value",
    "protocol-dispatch-impl d0 owner=t4/n12 implementation=n27 symbol=4"
) ) {
    if ( ! $asyncText.Contains($required) ) {
        throw "XLIR async protocol typed ABI is incomplete: $required"
    }
}
if ( $asyncText -notmatch 'type t6 kind=6 collection=none resource=none persistence=none/t0 cabi=30 callable=invalid ownership=2 null=1 nominal=s0 typeValue=0x[0-9A-F]{16} name=future display=future<int> integer=none params=\[t3\]' ) {
    throw "XLIR async protocol result has no canonical Future representation"
}

Remove-Item -LiteralPath $asyncGeneratedC -Force -ErrorAction SilentlyContinue
& $compiler cgen $asyncFixture --backend xlir --emit $asyncGeneratedC
if ( $LASTEXITCODE -ne 0 -or ! (Test-Path -LiteralPath $asyncGeneratedC -PathType Leaf) ) {
    throw "XLIR async protocol C generation failed"
}
$asyncCText = [System.IO.File]::ReadAllText($asyncGeneratedC, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "typedef struct xlir_async_frame_4",
    "frame->p0 = a0 != NULL ? xrtValueRetain(a0) : NULL;",
    "xrtValueRelease(frame->p0);",
    "static xfuture* xlir_async_start_4(xvalue* a0)",
    "static xfuture* xlir_pdispatch_1(xvalue* receiver)",
    "case 1: return xlir_async_start_4(receiver);"
) ) {
    if ( ! $asyncCText.Contains($required) ) {
        throw "XLIR async protocol generated C has no public Future witness ABI: $required"
    }
}
$asyncCommand = 'call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include build\tmp\xlir_protocol_async_dispatch.c test\cases\xlir_default_class_layout_host.c !XL6_CGEN_LINKS! -o build\tmp\xlir_protocol_async_dispatch.exe && build\tmp\xlir_protocol_async_dispatch.exe'
& $env:ComSpec /d /v:on /c $asyncCommand
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR async protocol generated-C runtime gate failed"
}

Write-Output "XLIR protocol typed, lowered, generated-C and runtime dispatch model ok (zero-argument, overload, generic requirement, getter/setter result-void, concrete generic-class and async Future witness ABIs)."
