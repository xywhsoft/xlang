param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$compiler = Join-Path $Root "release\xl.exe"
$fixture = Join-Path $Root "test\cases\xlir_async_class_method.xl"
$loweredIr = Join-Path $Root "build\tmp\xlir_async_class_method.lowered.ir"
$generatedC = Join-Path $Root "build\tmp\xlir_async_class_method.c"
$genericFixture = Join-Path $Root "test\cases\async_generic_class_method.xl"
$genericLoweredIr = Join-Path $Root "build\tmp\xlir_async_generic_class_method.lowered.ir"
$genericGeneratedC = Join-Path $Root "build\tmp\xlir_async_generic_class_method.c"
$runtimeHost = Join-Path $Root "test\cases\xlir_default_class_layout_host.c"
$sleepFixture = Join-Path $Root "test\cases\async_class_method.xl"
$sleepLoweredIr = Join-Path $Root "build\tmp\xlir_async_class_method_sleep.lowered.ir"
$sleepGeneratedC = Join-Path $Root "build\tmp\xlir_async_class_method_sleep.c"
$lifetimeFixture = Join-Path $Root "test\cases\async_class_receiver_lifetime.xl"
$lifetimeLoweredIr = Join-Path $Root "build\tmp\xlir_async_class_receiver_lifetime.lowered.ir"
$lifetimeGeneratedC = Join-Path $Root "build\tmp\xlir_async_class_receiver_lifetime.c"
$conditionalFixture = Join-Path $Root "test\cases\cgen_async_class_conditional_await.xl"
$conditionalLoweredIr = Join-Path $Root "build\tmp\xlir_async_conditional_await.lowered.ir"
$conditionalGeneratedC = Join-Path $Root "build\tmp\xlir_async_conditional_await.c"
$parameterFixture = Join-Path $Root "test\cases\cgen_async_param_ownership.xl"
$parameterLoweredIr = Join-Path $Root "build\tmp\xlir_async_param_ownership.lowered.ir"
$parameterLoweredJson = Join-Path $Root "build\tmp\xlir_async_param_ownership.lowered.json"
$parameterGeneratedC = Join-Path $Root "build\tmp\xlir_async_param_ownership.c"
$stringOwnershipFixture = Join-Path $Root "test\cases\cgen_async_string_ownership.xl"
$stringOwnershipLoweredIr = Join-Path $Root "build\tmp\xlir_async_string_ownership.lowered.ir"
$stringOwnershipGeneratedC = Join-Path $Root "build\tmp\xlir_async_string_ownership.c"

foreach ( $path in @($compiler, $fixture, $genericFixture, $sleepFixture, $lifetimeFixture, $conditionalFixture, $parameterFixture, $stringOwnershipFixture, $runtimeHost) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR async class-method gate input is missing: $path"
    }
}

Remove-Item -LiteralPath $loweredIr -Force -ErrorAction SilentlyContinue
& $compiler ir $fixture --lowered --format text --emit $loweredIr
if ( $LASTEXITCODE -ne 0 -or ! (Test-Path -LiteralPath $loweredIr -PathType Leaf) ) {
    throw "XLIR async class-method lowered-IR dump failed"
}
$irText = [System.IO.File]::ReadAllText($loweredIr, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "class-layout c0 type=t4 declaration=n2 base=t0/n0 tag=1 fields=0..1 inherited=0 create=1",
    "class-field f0 owner=t4 type=t3 declaration=n5 initializer=n7 destroy=s0 name=value",
    "kind=temporary",
    "async-start opcode=none runtime=none",
    "type=6 target=3"
) ) {
    if ( ! $irText.Contains($required) ) {
        throw "XLIR async class-method lowered ABI is incomplete: $required"
    }
}
if ( ([regex]::Matches($irText,
        'fn[0-9]+ kind=async-worker decl=n17 symbol=3 errors=may-raise')).Count -ne 1 -or
     ([regex]::Matches($irText,
        'call-target c[0-9]+ kind=4 .*symbol=3 declaration=n17 .*errors=may-raise/result-carrier .*async-result=copy-i64 worker=t3 worker-errors=may-raise dispatch=1 .*completion=retire-before-publish')).Count -ne 1 ) {
    throw "XLIR async class-method worker/effect/result-carrier ABI is incomplete"
}

Remove-Item -LiteralPath $generatedC -Force -ErrorAction SilentlyContinue
& $compiler cgen $fixture --backend xlir --emit $generatedC
if ( $LASTEXITCODE -ne 0 -or ! (Test-Path -LiteralPath $generatedC -PathType Leaf) ) {
    throw "XLIR async class-method C generation failed"
}
$cText = [System.IO.File]::ReadAllText($generatedC, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "typedef struct xlir_async_frame_3",
    "xvalue* p0;",
    "XL6_CAPTURE_XVALUE",
    "xlir_result_i64 worker;",
    "worker = xlir_fn_3(frame->p0);",
    "xlir_task_allocate(sizeof(*frame), xlir_async_task_3, &xlir_async_layout_3)",
    "xvalue* xlir_acquired = a0 != NULL ? xrtValueRetain(a0) : NULL;",
    "{ &(frame->p0), &(xlir_committed), sizeof(frame->p0) },",
    "xlir_async_pool_submit_language(frame, NULL, XL6_FUTURE_PAYLOAD_VALUE)"
) ) {
    if ( ! $cText.Contains($required) ) {
        throw "XLIR async class-method generated C has no retained receiver frame ABI: $required"
    }
}

$command = 'call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include build\tmp\xlir_async_class_method.c test\cases\xlir_default_class_layout_host.c !XL6_CGEN_LINKS! -o build\tmp\xlir_async_class_method.exe && build\tmp\xlir_async_class_method.exe'
& $env:ComSpec /d /v:on /c $command
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR async class-method generated-C runtime gate failed"
}

Remove-Item -LiteralPath $genericLoweredIr -Force -ErrorAction SilentlyContinue
& $compiler ir $genericFixture --lowered --format text --emit $genericLoweredIr
if ( $LASTEXITCODE -ne 0 -or ! (Test-Path -LiteralPath $genericLoweredIr -PathType Leaf) ) {
    throw "XLIR generic async class-method lowered-IR dump failed"
}
$genericIrText = [System.IO.File]::ReadAllText($genericLoweredIr, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "class-layout c0 type=t6 declaration=n2 base=t0/n0 tag=1 fields=0..1 inherited=0 create=1",
    "class-field f0 owner=t6 type=t5 declaration=n5 initializer=n0 destroy=s0 name=value",
    "generic-instance source=n7",
    "generic-instance source=n16",
    "args=[t5]",
    "async-start opcode=none runtime=none",
    "kind=async-worker"
) ) {
    if ( ! $genericIrText.Contains($required) ) {
        throw "XLIR generic async class-method lowered ABI is incomplete: $required"
    }
}
if ( ([regex]::Matches($genericIrText, "generic-instance source=n(?:7|16) declaration=n[0-9]+ symbol=[0-9]+ args=\[t5\]")).Count -ne 2 ) {
    throw "XLIR generic async class constructor/worker do not have isolated monomorphic identities"
}

Remove-Item -LiteralPath $genericGeneratedC -Force -ErrorAction SilentlyContinue
& $compiler cgen $genericFixture --backend xlir --emit $genericGeneratedC
if ( $LASTEXITCODE -ne 0 -or ! (Test-Path -LiteralPath $genericGeneratedC -PathType Leaf) ) {
    throw "XLIR generic async class-method C generation failed"
}
$genericCText = [System.IO.File]::ReadAllText($genericGeneratedC, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "typedef struct xlir_async_frame_4294967292",
    "XL6_CAPTURE_XVALUE",
    "xlir_result_i64 worker;",
    "worker = xlir_fn_4294967292(frame->p0);",
    "xlir_task_allocate(sizeof(*frame), xlir_async_task_4294967292, &xlir_async_layout_4294967292)",
    "xvalue* xlir_acquired = a0 != NULL ? xrtValueRetain(a0) : NULL;",
    "{ &(frame->p0), &(xlir_committed), sizeof(frame->p0) },",
    "xlir_async_pool_submit_language(frame, NULL, XL6_FUTURE_PAYLOAD_VALUE)"
) ) {
    if ( ! $genericCText.Contains($required) ) {
        throw "XLIR generic async class-method generated C has no monomorphic receiver frame ABI: $required"
    }
}
$genericCommand = 'call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include build\tmp\xlir_async_generic_class_method.c test\cases\xlir_default_class_layout_host.c !XL6_CGEN_LINKS! -o build\tmp\xlir_async_generic_class_method.exe && build\tmp\xlir_async_generic_class_method.exe'
& $env:ComSpec /d /v:on /c $genericCommand
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR generic async class-method generated-C runtime gate failed"
}

Remove-Item -LiteralPath $sleepLoweredIr -Force -ErrorAction SilentlyContinue
& $compiler ir $sleepFixture --lowered --format text --emit $sleepLoweredIr
if ( $LASTEXITCODE -ne 0 -or ! (Test-Path -LiteralPath $sleepLoweredIr -PathType Leaf) ) {
    throw "XLIR async class method with foreign sleep lowered-IR dump failed"
}
$sleepIrText = [System.IO.File]::ReadAllText($sleepLoweredIr, [System.Text.Encoding]::UTF8)
if ( ! $sleepIrText.Contains("name=c_uint display=c_uint integer=none params=[]") -or
     ! $sleepIrText.Contains("scalar-convert opcode=none runtime=none") -or
     ! $sleepIrText.Contains("kind=async-worker") ) {
    throw "XLIR async class method with foreign sleep has no explicit C scalar/worker ABI"
}

Remove-Item -LiteralPath $sleepGeneratedC -Force -ErrorAction SilentlyContinue
& $compiler cgen $sleepFixture --backend xlir --emit $sleepGeneratedC
if ( $LASTEXITCODE -ne 0 -or ! (Test-Path -LiteralPath $sleepGeneratedC -PathType Leaf) ) {
    throw "XLIR async class method with foreign sleep C generation failed"
}
$sleepCText = [System.IO.File]::ReadAllText($sleepGeneratedC, [System.Text.Encoding]::UTF8)
if ( ! $sleepCText.Contains("extern void xrtSleep(unsigned int);") -or
     ! $sleepCText.Contains("(unsigned int)") -or
     ! $sleepCText.Contains("xrtSleep(") ) {
    throw "XLIR async class method does not preserve the exact xrtSleep C boundary ABI"
}
$sleepCommand = 'call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include build\tmp\xlir_async_class_method_sleep.c test\cases\xlir_default_class_layout_host.c !XL6_CGEN_LINKS! -o build\tmp\xlir_async_class_method_sleep.exe && build\tmp\xlir_async_class_method_sleep.exe'
& $env:ComSpec /d /v:on /c $sleepCommand
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR async class method with foreign sleep generated-C runtime gate failed"
}

Remove-Item -LiteralPath $lifetimeLoweredIr -Force -ErrorAction SilentlyContinue
& $compiler ir $lifetimeFixture --lowered --format text --emit $lifetimeLoweredIr
if ( $LASTEXITCODE -ne 0 -or ! (Test-Path -LiteralPath $lifetimeLoweredIr -PathType Leaf) ) {
    throw "XLIR async receiver lifetime lowered-IR dump failed"
}
$lifetimeIrText = [System.IO.File]::ReadAllText($lifetimeLoweredIr, [System.Text.Encoding]::UTF8)
if ( ! $lifetimeIrText.Contains("errors=may-raise") -or
     ! $lifetimeIrText.Contains("async-start opcode=none runtime=none") -or
     ! $lifetimeIrText.Contains("kind=async-worker") ) {
    throw "XLIR async receiver lifetime has no explicit launch/error/worker ABI"
}

Remove-Item -LiteralPath $lifetimeGeneratedC -Force -ErrorAction SilentlyContinue
& $compiler cgen $lifetimeFixture --backend xlir --emit $lifetimeGeneratedC
if ( $LASTEXITCODE -ne 0 -or ! (Test-Path -LiteralPath $lifetimeGeneratedC -PathType Leaf) ) {
    throw "XLIR async receiver lifetime C generation failed"
}
$lifetimeCText = [System.IO.File]::ReadAllText($lifetimeGeneratedC, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "typedef struct xlir_result_future { bool ok; struct xfuture* value; } xlir_result_future;",
    "XL6_CAPTURE_XVALUE",
    "(xl6TaskRuntimeV2Get()->Cleanup)(frame);",
    "xvalue* xlir_acquired = a0 != NULL ? xrtValueRetain(a0) : NULL;",
    "{ &(frame->p0), &(xlir_committed), sizeof(frame->p0) },",
    "worker = xlir_fn_3(frame->p0, frame->p1);",
    "return (xlir_result_future){ false };"
) ) {
    if ( ! $lifetimeCText.Contains($required) ) {
        throw "XLIR async receiver lifetime generated C is missing future/error ownership ABI: $required"
    }
}
$lifetimeCommand = 'call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include build\tmp\xlir_async_class_receiver_lifetime.c test\cases\xlir_default_class_layout_host.c !XL6_CGEN_LINKS! -o build\tmp\xlir_async_class_receiver_lifetime.exe && build\tmp\xlir_async_class_receiver_lifetime.exe'
& $env:ComSpec /d /v:on /c $lifetimeCommand
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR async receiver lifetime generated-C runtime gate failed"
}

Remove-Item -LiteralPath $conditionalLoweredIr -Force -ErrorAction SilentlyContinue
& $compiler ir $conditionalFixture --lowered --format text --emit $conditionalLoweredIr
if ( $LASTEXITCODE -ne 0 -or ! (Test-Path -LiteralPath $conditionalLoweredIr -PathType Leaf) ) {
    throw "XLIR conditional future await lowered-IR dump failed"
}
$conditionalIrText = [System.IO.File]::ReadAllText($conditionalLoweredIr, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "retain opcode=none runtime=none",
    "await opcode=none runtime=none",
    "null opcode=none runtime=none"
) ) {
    if ( ! $conditionalIrText.Contains($required) ) {
        throw "XLIR conditional future await is missing its owned join/null ABI: $required"
    }
}
if ( $conditionalIrText.Contains("immediate=NULL") ) {
    throw "XLIR conditional future await regressed to an untyped CONST null payload"
}

Remove-Item -LiteralPath $conditionalGeneratedC -Force -ErrorAction SilentlyContinue
& $compiler cgen $conditionalFixture --backend xlir --emit $conditionalGeneratedC
if ( $LASTEXITCODE -ne 0 -or ! (Test-Path -LiteralPath $conditionalGeneratedC -PathType Leaf) ) {
    throw "XLIR conditional future await C generation failed"
}
$conditionalCText = [System.IO.File]::ReadAllText($conditionalGeneratedC, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "!= NULL ? xrtFutureRef(",
    "{ &(xlir_released), &(s4294967295), sizeof(xlir_released) },",
    "xrtFutureDestroy(xlir_released);",
    '"await on null future"',
    'xlir_error_create_at("StateError"'
) ) {
    if ( ! $conditionalCText.Contains($required) ) {
        throw "XLIR conditional future await generated C is missing its nullable ownership/error ABI: $required"
    }
}
$conditionalCommand = 'call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include build\tmp\xlir_async_conditional_await.c test\cases\xlir_default_class_layout_host.c !XL6_CGEN_LINKS! -o build\tmp\xlir_async_conditional_await.exe && build\tmp\xlir_async_conditional_await.exe'
& $env:ComSpec /d /v:on /c $conditionalCommand
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR conditional future await generated-C runtime gate failed"
}

& $compiler ir $parameterFixture --lowered --format text --emit $parameterLoweredIr
if ( $LASTEXITCODE -ne 0 -or ! (Test-Path -LiteralPath $parameterLoweredIr -PathType Leaf) ) {
    throw "XLIR async parameter ownership lowered-IR dump failed"
}
$parameterIrText = [System.IO.File]::ReadAllText($parameterLoweredIr, [System.Text.Encoding]::UTF8)
if ( ([regex]::Matches($parameterIrText, "runtime=xvalue-to-string")).Count -ne 1 -or
     ([regex]::Matches($parameterIrText, "runtime=i64-to-string")).Count -ne 2 -or
     ! $parameterIrText.Contains("kind=async-worker") -or
     ! $parameterIrText.Contains("async-start opcode=none runtime=none") ) {
    throw "XLIR async parameter lowering is missing explicit string conversion or async-worker ABI"
}

Remove-Item -LiteralPath $parameterLoweredJson -Force -ErrorAction SilentlyContinue
& $compiler ir $parameterFixture --lowered --format json --emit $parameterLoweredJson
if ( $LASTEXITCODE -ne 0 -or ! (Test-Path -LiteralPath $parameterLoweredJson -PathType Leaf) ) {
    throw "XLIR async parameter ownership lowered-IR JSON dump failed"
}
$parameterIr = Get-Content -LiteralPath $parameterLoweredJson -Raw | ConvertFrom-Json
$parameterAsyncStarts = @($parameterIr.instructions | Where-Object { $_.kindName -eq "async-start" })
if ( $parameterAsyncStarts.Count -ne 1 ) {
    throw "XLIR async parameter lowering must contain exactly one structural async-start instruction"
}
$parameterAsyncStart = $parameterAsyncStarts[0]
$parameterAsyncTargets = @($parameterIr.callTargets | Where-Object { $_.symbol -eq $parameterAsyncStart.targetSymbol })
if ( $parameterAsyncTargets.Count -ne 1 ) {
    throw "XLIR async parameter lowering has no unique async-worker target for async-start"
}
$parameterAsyncTarget = $parameterAsyncTargets[0]
if ( $parameterAsyncTarget.kind -ne 4 -or
     $parameterAsyncTarget.asyncResultAbiName -ne "owned-string" -or
     $parameterAsyncTarget.parameterCount -ne 4 -or
     (@($parameterAsyncStart.operands).Count -ne $parameterAsyncTarget.parameterCount) -or
     ((@($parameterAsyncTarget.asyncCaptureAbis) -join ",") -ne "retain-xvalue,retain-xvalue,retain-closure,retain-xvalue") ) {
    throw "XLIR async parameter lowering has an incomplete worker result/capture ABI"
}
for ( $parameterIndex = 0; $parameterIndex -lt $parameterAsyncTarget.parameterCount; $parameterIndex++ ) {
    $operandValue = $parameterAsyncStart.operands[$parameterIndex]
    $operandDefinitions = @($parameterIr.instructions | Where-Object { $_.result -eq $operandValue })
    if ( $operandDefinitions.Count -ne 1 -or
         $operandDefinitions[0].type -ne $parameterAsyncTarget.parameterTypes[$parameterIndex] ) {
        throw "XLIR async parameter operand $parameterIndex is not explicitly typed for its worker boundary"
    }
}

& $compiler cgen $parameterFixture --backend xlir --emit $parameterGeneratedC
if ( $LASTEXITCODE -ne 0 -or ! (Test-Path -LiteralPath $parameterGeneratedC -PathType Leaf) ) {
    throw "XLIR async parameter ownership C generation failed"
}
$parameterCText = [System.IO.File]::ReadAllText($parameterGeneratedC, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "typedef struct xlir_async_frame_5",
    "xlir_closure* p2;",
    "xlir_xvalue_to_string(",
    "xlir_i64_to_string(",
    "{XL6_CAPTURE_XVALUE, offsetof(xlir_async_frame_5, p0)",
    "{XL6_CAPTURE_XVALUE, offsetof(xlir_async_frame_5, p1)",
    "{XL6_CAPTURE_CLOSURE, offsetof(xlir_async_frame_5, p2)",
    "{XL6_CAPTURE_XVALUE, offsetof(xlir_async_frame_5, p3)",
    "static const XL6_CaptureLayoutV1 xlir_async_layout_5",
    "(xl6TaskRuntimeV2Get()->Cleanup)(frame);",
    "xlir_task_allocate(sizeof(*frame), xlir_async_task_5, &xlir_async_layout_5)",
    "xvalue* xlir_acquired = a0 != NULL ? xrtValueRetain(a0) : NULL;",
    "xlir_closure* xlir_acquired = a2 != NULL ? xlir_closure_retain(a2) : NULL;",
    "{ &(frame->p0), &(xlir_committed), sizeof(frame->p0) },",
    "{ &(frame->p1), &(xlir_committed), sizeof(frame->p1) },",
    "{ &(frame->p2), &(xlir_committed), sizeof(frame->p2) },",
    "{ &(frame->p3), &(xlir_committed), sizeof(frame->p3) },",
    "xlir_result_string worker;",
    "worker = xlir_fn_5(frame->p0, frame->p1, frame->p2, frame->p3);",
    "outcome.ok = worker.ok;",
    "outcome = xlir_async_frame_retire_",
    "if (worker.ok) *value = worker.value;",
    "xl6PayloadDropGet(XL6_FUTURE_PAYLOAD_STRING)(value, NULL);",
    "result->Destroy = xl6PayloadDropGet(XL6_FUTURE_PAYLOAD_STRING);",
    "xlir_async_pool_submit_language(frame, NULL, XL6_FUTURE_PAYLOAD_STRING)"
) ) {
    if ( ! $parameterCText.Contains($required) ) {
        throw "XLIR async parameter generated C is missing an explicit frame/conversion ownership ABI: $required"
    }
}
foreach ( $forbidden in @(
    "xrtValueRelease(frame->p",
    "xlir_closure_release(frame->p",
    "frame->p2 = xlir_closure_retain(a2);"
) ) {
    if ( $parameterCText.Contains($forbidden) ) {
        throw "XLIR async parameter generated C bypasses the verified capture runtime: $forbidden"
    }
}
$parameterCommand = 'call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include build\tmp\xlir_async_param_ownership.c test\cases\xlir_default_class_layout_host.c !XL6_CGEN_LINKS! -o build\tmp\xlir_async_param_ownership.exe && build\tmp\xlir_async_param_ownership.exe'
& $env:ComSpec /d /v:on /c $parameterCommand
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR async parameter ownership generated-C runtime gate failed"
}

& $compiler ir $stringOwnershipFixture --lowered --format text --emit $stringOwnershipLoweredIr
if ( $LASTEXITCODE -ne 0 -or ! (Test-Path -LiteralPath $stringOwnershipLoweredIr -PathType Leaf) ) {
    throw "XLIR async string ownership lowered-IR dump failed"
}
$stringOwnershipIrText = [System.IO.File]::ReadAllText($stringOwnershipLoweredIr, [System.Text.Encoding]::UTF8)
if ( ([regex]::Matches($stringOwnershipIrText, "runtime=xvalue-to-string")).Count -ne 1 -or
     ([regex]::Matches($stringOwnershipIrText, "runtime=string-concat")).Count -ne 4 -or
     ([regex]::Matches($stringOwnershipIrText, "await opcode=none runtime=none")).Count -ne 4 -or
     ([regex]::Matches($stringOwnershipIrText, "async-start opcode=none runtime=none")).Count -ne 3 ) {
    throw "XLIR async string ownership lacks explicit dynamic conversion, concat, launch or repeated-await IR"
}

& $compiler cgen $stringOwnershipFixture --backend xlir --emit $stringOwnershipGeneratedC
if ( $LASTEXITCODE -ne 0 -or ! (Test-Path -LiteralPath $stringOwnershipGeneratedC -PathType Leaf) ) {
    throw "XLIR async string ownership C generation failed"
}
$stringOwnershipCText = [System.IO.File]::ReadAllText($stringOwnershipGeneratedC, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "{XL6_CAPTURE_STRING, offsetof(xlir_async_frame_1, p0)",
    "{XL6_CAPTURE_XVALUE, offsetof(xlir_async_frame_2, p0)",
    "static const XL6_CaptureLayoutV1 xlir_async_layout_1",
    "static const XL6_CaptureLayoutV1 xlir_async_layout_2",
    "xlir_task_allocate(sizeof(*frame), xlir_async_task_1, &xlir_async_layout_1)",
    "xlir_task_allocate(sizeof(*frame), xlir_async_task_2, &xlir_async_layout_2)",
    "char* xlir_acquired = a0 != NULL ? xrtStrDup(a0) : NULL;",
    "xvalue* xlir_acquired = a0 != NULL ? xrtValueRetain(a0) : NULL;",
    "{ &(frame->p0), &(xlir_committed), sizeof(frame->p0) },",
    "outcome = xlir_async_frame_retire_1(frame, outcome);",
    "outcome = xlir_async_frame_retire_2(frame, outcome);",
    "result->Destroy = xl6PayloadDropGet(XL6_FUTURE_PAYLOAD_STRING);",
    "xlir_async_pool_submit_language(frame, NULL, XL6_FUTURE_PAYLOAD_STRING)",
    "xlir_xvalue_to_string(",
    "xrtStrConcat(xrtStrView("
) ) {
    if ( ! $stringOwnershipCText.Contains($required) ) {
        throw "XLIR async string ownership generated C is missing a conversion/frame ABI: $required"
    }
}
if ( ([regex]::Matches($stringOwnershipCText,
        [regex]::Escape("(xl6TaskRuntimeV2Get()->Cleanup)(frame);"))).Count -ne 2 -or
     ([regex]::Matches($stringOwnershipCText,
        [regex]::Escape("xlir_async_pool_submit_language(frame, NULL, XL6_FUTURE_PAYLOAD_STRING)"))).Count -ne 2 ) {
    throw "XLIR async string ownership must use one verified capture cleanup and payload submission per worker"
}
foreach ( $forbidden in @(
    "frame->p0 = a0 != NULL ?",
    "xrtFree(frame->p0);",
    "xrtValueRelease(frame->p0);",
    "xrtTaskSubmit(xlir_async_pool"
) ) {
    if ( $stringOwnershipCText.Contains($forbidden) ) {
        throw "XLIR async string ownership bypasses the verified capture/payload runtime: $forbidden"
    }
}
$stringOwnershipCommand = 'call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include build\tmp\xlir_async_string_ownership.c test\cases\xlir_default_class_layout_host.c !XL6_CGEN_LINKS! -o build\tmp\xlir_async_string_ownership.exe && build\tmp\xlir_async_string_ownership.exe'
& $env:ComSpec /d /v:on /c $stringOwnershipCommand
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR async string ownership generated-C runtime gate failed"
}

Write-Output "XLIR concrete/generic async receiver, conditional/repeated future and scalar/string/xvalue/closure frame ownership ABIs ok."
