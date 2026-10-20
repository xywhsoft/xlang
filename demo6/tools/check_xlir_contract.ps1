param(
    [string]$Root = "",
    [string]$PublicDirectory = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

function Read-Utf8([string]$relative)
{
    $path = Join-Path $Root $relative
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR contract source missing: $relative"
    }
    return [System.IO.File]::ReadAllText($path, [System.Text.Encoding]::UTF8)
}

$header = Read-Utf8 "src\ir\xl6_ir.h"
$source = Read-Utf8 "src\ir\xl6_ir.c"
$lower = Read-Utf8 "src\ir\xl6_ir_lower.c"
$directBridge = Read-Utf8 "src\ir\xl6_ir_direct_bridge.c"
$xlangBridge = Read-Utf8 "src\ir\xl6_ir_xlang_bridge.c"
$directBridgeUnit = Read-Utf8 "test\cases\xlir_direct_bridge_unit.c"
$cgenUnit = Read-Utf8 "test\cases\xlir_cgen.c"
$ownership = Read-Utf8 "src\ir\xl6_ir_ownership.c"
$protocolSlot = [regex]::Match($header, '(?s)typedef struct XL6_IrProtocolDispatchSlot.*?} XL6_IrProtocolDispatchSlot;').Value
foreach ($required in @('parameterAbis', 'errorAbi', 'errorPhysicalAbi')) {
    if (!$protocolSlot.Contains($required)) { throw "Protocol slots must own their callable contract: $required" }
}
$virtualSlot = [regex]::Match($header, '(?s)typedef struct XL6_IrClassDispatchSlot.*?} XL6_IrClassDispatchSlot;').Value
foreach ($required in @('parameterAbis', 'errorAbi', 'errorPhysicalAbi')) {
    if (!$virtualSlot.Contains($required)) { throw "Virtual slots must own their callable contract: $required" }
}
$callCompletion = [regex]::Match($lower, '(?ms)^static int xl6IrLowerCompleteCall\(.*?^\}').Value
if (!$callCompletion.Contains('xl6IrCallInstructionErrorAbi') -or $callCompletion.Contains('target->errorAbi')) {
    throw 'Call completion must observe the emitted operation, not one implementation of a dispatch slot'
}
foreach ($renderer in @('instruction_emit', 'async_instruction', 'generator')) {
    $body = Read-Utf8 "src\cgen\xl6_ir_cgen_$renderer.c"
    if (!$body.Contains('xl6IrCgenEmitDispatchCallInstruction') -or !$body.Contains('xl6IrCgenEmitCallErrorTest')) {
        throw "All execution modes must share virtual/protocol calls and physical outcome tests: $renderer"
    }
}
foreach ($gate in @('xlir_dynamic_member_set', 'xlir_dynamic_bound_method', 'xlir_dynamic_member_get', 'xlir_property_assignment_transaction', 'xlir_virtual_call_outcome', 'xlir_virtual_activation', 'xlir_owned_activation', 'xlir_module_task_pool', 'xlir_checked_projection', 'xlir_owned_projection', 'xlir_evaluation_cleanup', 'xlir_match_cleanup', 'xlir_async_terminal_outcome', 'xlir_async_pool_outcome', 'xlir_callback_boundary', 'xlir_task_value_types', 'xlir_task_byval', 'xlir_callable_byval', 'xlir_callable_result', 'xlir_protocol_call_outcome', 'xlir_callsite_evaluation', 'xlir_indexed_accessor', 'xlir_property_update')) {
    if (!(Read-Utf8 'test\suites\03_core_function_abi.bat').Contains("tools\check_$gate.ps1") -or
        !(Read-Utf8 "tools\check_$gate.ps1").Contains("test/cases/$($gate)_contract.c")) {
        throw "Virtual/property ownership must retain a current-source runtime and mutation gate: $gate"
    }
}
foreach ($required in @('evaluationErrorAbi', 'evaluationScope', 'evaluationResult')) {
    if (!$header.Contains($required) -or !$source.Contains($required) -or !$lower.Contains($required)) {
        throw "Evaluation cleanup needs explicit typed effects and transaction identity: $required"
    }
}
if (!$source.Contains('xl6IrVerifyEvaluationCleanupRegions') -or
    !$source.Contains('xl6IrNodeEvaluationErrorAbi') -or
    !$callCompletion.Contains('xl6IrLowerCompleteOwnedCleanup') -or
    !([regex]::Match($lower, '(?ms)^static int xl6IrLowerCompleteEvaluationBorrowRange\([^;{]*\)\s*\{.*?^\}').Value).Contains('xl6IrLowerCompleteOwnedCleanup') -or
    !([regex]::Match($lower, '(?ms)^static int xl6IrLowerCompleteEvaluationBorrows\([^;{]*\)\s*\{.*?^\}').Value).Contains('xl6IrLowerCompleteEvaluationBorrowRange')) {
    throw 'Calls and non-consuming evaluations must share verified result commit and cleanup failure handling'
}
if (!$source.Contains('xl6IrVerifyExpressionPatternBindings') -or !$header.Contains('xl6IrPatternBindingPayload') -or
    !$source.Contains('XLIR SSA values are immutable; branch joins require explicit typed storage') -or
    !$lower.Contains('xl6IrLowerReadJoinStorage') -or $lower.Contains('instruction->targetValue = target;') -or
    !(Read-Utf8 'tools\check_xlir_match_cleanup.ps1').Contains('test/cases/xlir_match_cleanup_async_runtime.c')) {
    throw 'Match bindings and immutable branch joins need verified storage, frame lifetimes and real suspension coverage'
}
$taskSem = Read-Utf8 'src\sem\xl6_sem.c'
$taskContract = Read-Utf8 'src\cgen\xl6_ir_cgen_future_contract.c'
foreach ($required in @('xl6IrTaskSpawnParameterAbiSupported', 'xl6IrTaskSpawnArgumentCanBind')) {
    if (!$header.Contains($required) -or !$source.Contains($required) -or !$lower.Contains($required)) {
        throw "Task admission and lowering must share exact IR argument and capture contracts: $required"
    }
}
if (!$taskContract.Contains('xl6IrTaskSpawnParameterAbiSupported') -or
    $lower.Contains('xl6IrLowerTaskSpawnArgumentCanBind') -or
    $lower.Contains('xl6IrLowerTaskSpawnParameterAbiSupported') -or
    $taskContract.Contains('xl6IrCgenTaskSpawnParameterAbiSupported') -or
    $taskSem.Contains('xl6SemTypeSupportsTaskSpawnXValueBridge') -or
    $taskSem.Contains('xl6SemExprSupportsTaskSpawnXValueBridge') -or
    !$taskSem.Contains('xl6SemTypeCanBeFunctionResult') -or
    !$taskSem.Contains('task.spawn cannot capture byref or out caller storage')) {
    throw 'Task language admission must not depend on a legacy xvalue whitelist or duplicate backend ABI guesses'
}
$taskValueGate = Read-Utf8 'tools\check_xlir_task_value_types.ps1'
if (!$taskValueGate.Contains('--admission') -or
    !$taskValueGate.Contains('test/cases/xlir_task_value_types_release_runtime.c') -or
    !(Read-Utf8 'test\cases\xlir_task_value_types_contract.c').Contains('generator signatures semantic-only (backend incomplete)') -or
    !(Read-Utf8 'test\cases\xlir_task_value_types_release_runtime.c').Contains('xrtValueSetHas(set, key)')) {
    throw 'Task types need language admission and a deterministic cross-thread runtime ownership contract'
}
$callbackEmitter = Read-Utf8 'src\cgen\xl6_ir_cgen_task_helpers.c'
foreach ($required in @('callbackBoundary', 'callbackCaptures', 'invokeAbi', 'XL6_IR_CALLBACK_TRANSPORT_FUTURE_READY_QUEUE_V1', 'XL6_IR_CALLBACK_CLEANUP_TRANSACTIONAL')) {
    if (!$header.Contains($required) -or !$source.Contains($required) -or !$callbackEmitter.Contains($required)) {
        throw "Callback activations require explicit capture and completion contracts: $required"
    }
}
$futureCallbackRuntime = Read-Utf8 'src/runtime/xl6_future_callback.c'
if (!$lower.Contains('xl6IrBuildCallbackBoundaries') -or !$source.Contains('xl6IrVerifyCallbackBoundaries') -or
    !$futureCallbackRuntime.Contains('xrtFutureWatchInitPhased') -or !$futureCallbackRuntime.Contains('xrtFutureWatchAdd') -or
    !$callbackEmitter.Contains('xl6FutureCallbackRuntimeV5Get()->Start') -or !$callbackEmitter.Contains('xrtPromiseForward') -or
    $callbackEmitter.Contains('xrtFutureWatchAdd') -or $callbackEmitter.Contains('xfuturewatch watch;') -or
    $callbackEmitter.Contains('future = xrtFutureContinueOwned(')) {
    throw 'A runtime continuation adapter must not bypass the language capture cleanup/publication barrier'
}
foreach ($required in @('XL6_IR_CALLBACK_STORAGE_RESIDENT_LAYOUT_TASK_FRAME_V3','XL6_IR_CALLBACK_STORAGE_RESIDENT_QUEUED_FUTURE_FRAME_V1','storageAbi')) {
    if (!$header.Contains($required) -or !$source.Contains($required) -or !$callbackEmitter.Contains($required)) {
        throw "Callback physical storage must be an exact verified IR contract: $required"
    }
}
if (!$callbackEmitter.Contains('XL6_CaptureLayoutV2') -or !$callbackEmitter.Contains('xl6IrCgenEmitCaptureSlotV2') -or
    !$callbackEmitter.Contains('xl6TaskRuntimeV3Get') -or !$callbackEmitter.Contains('xl6FutureCallbackRuntimeV5Get') -or
    $callbackEmitter.Contains('static void %s_frame_drop_') -or $callbackEmitter.Contains('static bool %s_frame_trace_')) {
    throw 'Callback captures must use closed resident layouts, not generated Drop/Trace functions'
}
foreach ($required in @('xl6CodeLeaseAcquireScoped','xl6CodeScopeEnter','xl6CodeScopeLeave','frame_begin',
    'frame->publishing','frame->retiring','xrtCancelOwnership','watch_trace','detach_transport')) {
    if (!$futureCallbackRuntime.Contains($required)) { throw "Future callback residency/ownership is incomplete: $required" }
}
$byvalGate = Read-Utf8 'tools\check_xlir_task_byval.ps1'
$byvalContract = Read-Utf8 'test\cases\xlir_task_byval_contract.c'
if (!$byvalGate.Contains("@('direct', 'full')") -or
    !$byvalContract.Contains('xl6IrVerifyOwnerConsumption') -or
    !$byvalContract.Contains('XL6_IR_RUNTIME_ERROR_CAUSE_REF') -or
    !$lower.Contains('Default temporaries use the same commit as incoming parameter slots.') -or
    !$callbackEmitter.Contains('xl6IrCgenEmitCallbackInvocationCommit')) {
    throw 'Task BYVAL needs independent frame/invoke ownership, direct and boxed callable gates, and affine named adapter verification'
}
$callableBridge = Read-Utf8 'src\ir\xl6_ir_callable_bridge.c'
$callableEmitter = Read-Utf8 'src\cgen\xl6_ir_cgen_callable_adapters.c'
foreach ($required in @('decodeAbi', 'encodeAbi', 'invokeAbi', 'decodedDestroyAbi', 'inputDestroyAbi')) {
    if (!$header.Contains($required) -or !$callableBridge.Contains($required) -or !$callableEmitter.Contains($required)) {
        throw "Runtime-callable bridges need verified parameter programs: $required"
    }
}
if (!$lower.Contains('xl6IrBuildCallableBridges') -or !$source.Contains('xl6IrVerifyCallableBridges') -or
    !$callableEmitter.Contains('xl6IrCallableBridgeGet') -or
    !(Read-Utf8 'test\cases\xlir_callable_byval_runtime.c').Contains('xrtMemDebugFailAfter') -or
    !$byvalContract.Contains('xlirCallableBridgeMutations')) {
    throw 'Callable bridge generation must require complete IR plans and retain fault/mutation coverage'
}
$bridgeMutations = Read-Utf8 'test\cases\xlir_callable_bridge_mutations.h'
foreach ($required in @('callableBridgeCount', 'callableBridgeParameterCount', 'decodeAbi', 'encodeAbi', 'integerDomain.rule', 'integerDomain.identity', 'integerDomain.nominal', 'invokeAbi', 'decodedDestroyAbi', 'inputDestroyAbi')) {
    if (!$bridgeMutations.Contains($required)) { throw "Callable bridge mutation coverage is missing: $required" }
}
$callableResults = Read-Utf8 'src\cgen\xl6_ir_cgen_callable_results.c'
foreach ($required in @('XL6_IR_CALLABLE_BRIDGE_F32', 'XL6_IR_CALLABLE_BRIDGE_TIME')) {
    if (!$header.Contains($required) -or !$callableBridge.Contains($required) -or
        !$callableEmitter.Contains($required) -or !$callableResults.Contains($required)) {
        throw "Callable scalar transport must use closed bidirectional opcodes: $required"
    }
}
if ((Read-Utf8 'src\cgen\xl6_ir_cgen_callable_contract.c').Contains('xl6IrCgenRuntimeCallableScalarAbi') -or
    !$callableResults.Contains('xl6ValueReadF32V1') -or
    !(Read-Utf8 'src\compile\xl6_compile_unit.c').Contains('xl6IrCgenVisitResidentSymbols') -or
    !(Read-Utf8 'test\cases\stdlib_callable_scalars_contract.c').Contains('XL6_ARTIFACT_VALUE_ADMISSION')) {
    throw 'Callable scalars need resident exact F32 reads, closed programs and emission-aligned pruning, not a Cgen type whitelist'
}
foreach ($required in @('xl6ValueReadIntegerV1', 'xl6ValueBoxIntegerV1')) {
    if (!$callableResults.Contains($required) -or
        !(Read-Utf8 'src\cgen\xl6_ir_cgen_native_dependencies.c').Contains($required)) {
        throw "Callable integer domains must use resident range/identity codecs and pruning: $required"
    }
}
foreach ($required in @('resultCommitAbi', 'frameCleanupAbi', 'callableBridgeResults')) {
    if (!$header.Contains($required) -or !$source.Contains($required) -or
        !$callableBridge.Contains($required) -or !$callableResults.Contains($required)) {
        throw "Callable results require verified transport/commit programs: $required"
    }
}
foreach ($required in @('xl6ErrorScopeBeginCleanup', 'xl6ErrorScopeCaptureFailure',
    'xrtCallResultMove', 'xrtCallResultSet', 'xrtValueNull')) {
    if (!$callableResults.Contains($required)) { throw "Callable result cleanup lost its public-API transaction: $required" }
}
if (!(Read-Utf8 'test\cases\xlir_callable_result_contract.c').Contains('check_language_boundary') -or
    !(Read-Utf8 'test\cases\xlir_callable_result_runtime.c').Contains('xrtMemDebugFailAfter') -or
    !(Read-Utf8 'src\cgen\xl6_ir_cgen_emit_xvalue_ops.c').Contains('xlir_xvalue_project_closure_try_copy')) {
    throw 'Callable result programs need language boundary tests and allocation-fallible closure decoding'
}
$poolPrelude = Read-Utf8 'src\cgen\xl6_ir_cgen_emit_prelude.c'
foreach ($required in @('futureOutcomeAbi', 'XL6_IR_FUTURE_OUTCOME_XRT_ERROR_KIND')) {
    if (!$header.Contains($required) -or !$source.Contains($required) -or !$lower.Contains($required)) {
        throw "Future outcomes must be explicit lowered and verified IR: $required"
    }
}
foreach ($required in @('XL6_IR_ASYNC_MACHINE_ENTRY_RESUME_CANCELLED', 'XL6_IR_ASYNC_MACHINE_ENTRY_RESUME_CLOSED')) {
    if (!$header.Contains($required) -or !$source.Contains($required)) {
        throw "Continuation terminal resume edge is missing: $required"
    }
}
if (!$poolPrelude.Contains('xl6PayloadObserveOutcome') -or
    !(Read-Utf8 'src\runtime\xl6_future_payload.c').Contains('case XFUTURE_CANCELLED:') -or
    !(Read-Utf8 'src\runtime\xl6_future_payload.c').Contains('xrtSetErrorTake(xrtErrorRef(error))') -or
    !(Read-Utf8 'src\cgen\xl6_ir_cgen_async_machine.c').Contains('xl6PayloadObserveOutcome') -or
    !$poolPrelude.Contains('xlir_future_complete_error') -or
    !$poolPrelude.Contains('static void xlir_async_result_fail(xpromise* promise)') -or
    !$poolPrelude.Contains('(void)xlir_future_complete_error(promise, error)') -or
    !(Read-Utf8 'src\cgen\xl6_ir_cgen_async_instruction.c').Contains('xlir_async_result_fail') -or
    !(Read-Utf8 'tools\check_xlir_async_terminal_outcome.ps1').Contains('test/cases/xlir_async_terminal_outcome_runtime.c')) {
    throw 'Future callbacks and blocking awaits need shared terminal observation and a current-source runtime gate'
}
$taskResident=Read-Utf8 'src/runtime/xl6_task.c'
$taskPoolResident=Read-Utf8 'src/runtime/xl6_task_pool.c'
foreach ($required in @('xrtFutureWatchInitPhased', 'xrtFutureWatchAdd', 'xrtPromiseForward', 'xrtPromiseCancelToken',
    'xrtTaskSubmitTraced','xl6CodeScopeEnter','xl6CodeScopeLeave','frame_ops')) {
    if (!$taskResident.Contains($required)) { throw "Resident task completion requires the preallocated scoped ownership/cancellation bridge: $required" }
}
foreach ($required in @('xrtTaskPoolCreate','xrtTaskPoolDestroy','submitting','xl6TaskPoolControlV1Get')) {
    if (!$taskPoolResident.Contains($required)) { throw "Resident task pool lifetime/control provider is incomplete: $required" }
}
foreach ($required in @('taskStorageAbi','XL6_IR_TASK_STORAGE_RESIDENT_LAYOUT_FRAME_POOL_V3','asyncCaptureTraceAbis','asyncCaptureDestroyAbis',
    'valueIteratorStorageAbi','XL6_IR_VALUE_ITERATOR_STORAGE_RESIDENT_CHECKED_SNAPSHOT_V2')) {
    if (!$header.Contains($required) -or !$source.Contains($required) -or !$lower.Contains($required)) { throw "Task activation IR lacks exact resident storage/trace ABI: $required" }
}
$taskLauncher=Read-Utf8 'src/cgen/xl6_ir_cgen_async_machine.c'
$iteratorEmitter=Read-Utf8 'src/cgen/xl6_ir_cgen_emit_xvalue_core.c'
if (!$taskLauncher.Contains('xl6IrCgenEmitActivationLayout') -or
    !$taskLauncher.Contains('xl6ContinuationFrameLaunchV1(frame,') -or
    $taskLauncher.Contains('static bool xlir_async_frame_trace_') -or $taskLauncher.Contains('static void xlir_async_frame_drop_') -or
    $iteratorEmitter.Contains('state->cursor') -or $iteratorEmitter.Contains('xlir_xvalue_iterator_ownership_ops') -or
    !$iteratorEmitter.Contains('xl6ValueIteratorRuntimeV2Get()->View') -or
    !(Read-Utf8 'src/runtime/xl6_capture.c').Contains('CAPTURE_DROP(XL6_CAPTURE_VALUE_ITERATOR') -or
    !(Read-Utf8 'src/api/xl_api_symbols.c').Contains('"xl6ValueIteratorRuntimeV2Get"')) {
    throw 'Task captures and iterator lifetime must consume closed IR/resident contracts, not private Cgen Drop/Trace storage'
}
$managedIterator=Read-Utf8 'src/runtime/xl6_value_iterator.c'
foreach($required in @('xrtValueCursorCreate','xrtValueCursorAdvance','xrtValueCursorRelease','xrtValueCursorOwnership')) {
    if(!$managedIterator.Contains($required)){throw "Language iterator lacks managed native cursor lifecycle: $required"}
}
$ownershipCatalog = Read-Utf8 'src/runtime/xl6_ownership_catalog.c'
if(!(Read-Utf8 'src/api/xl_api.c').Contains('xl6OwnershipCatalogResolveV1(ref,module->ownershipCatalog,') -or
    !$ownershipCatalog.Contains('xrtValueCursorOwnershipAdapterV1(ref)')) {
    throw 'Module resolver must independently admit the actual managed cursor child'
}
if (!$poolPrelude.Contains('xl6TaskSubmitPayloadV1') -or $poolPrelude.Contains('xl6TaskRuntimeV1Get()->Submit') -or $poolPrelude.Contains('xlir_task_outcome_watch') -or
    $poolPrelude.Contains('xlir_async_pool_gate') -or !(Read-Utf8 'tools/check_xlir_task_storage.ps1').Contains('xlir_task_storage_runtime.c')) {
    throw 'Cgen must render resident task ABI, not duplicate transport lifecycle or skip native ownership coverage'
}
$payloadRuntime=Read-Utf8 'src/runtime/xl6_future_payload.c'
$payloadEmitter=Read-Utf8 'src/cgen/xl6_ir_cgen_task_helpers.c'
if (!$taskResident.Contains('xrtTaskSubmitOwnedJobV1(native,frame,args.Cancel,&native_data_policy,policy)') -or !$payloadRuntime.Contains('xrtPromiseResolveOwnedPolicyV1') -or
    !$payloadRuntime.Contains('xrtFutureOwnershipAdapterV1') -or !$payloadEmitter.Contains('XL6_FUTURE_PAYLOAD_CLOSURE') -or
    !$payloadRuntime.Contains('xl6PayloadTraceGenerator') -or !$payloadEmitter.Contains('XL6_FUTURE_PAYLOAD_GENERATOR') -or
    $payloadEmitter.Contains('xrtPromiseResolveOwnedTraced') -or $poolPrelude.Contains('xlir_async_closure_trace')) {
    throw 'Verified result ABIs must select one closed resident lifecycle before publication or executor admission'
}
$taskOwnershipDomain=Read-Utf8 'src/runtime/xl6_ownership_domain_task.c'
foreach ($required in @('xl6TaskNativeDataPolicyV1Get','xl6TaskWatchPolicyV1Get','xl6TaskOwnershipAdapterV1')) {
    if (!$taskResident.Contains($required) -or !$taskOwnershipDomain.Contains($required) -or
        !(Read-Utf8 'src/api/xl_api_symbols.c').Contains($required) -or
        !(Read-Utf8 'test/cases/xlir_one_shot_tcc_host.c').Contains($required)) {
        throw "Task lifecycle identities must reach the actual runtime, collector and source TCC hosts: $required"
    }
}
foreach ($required in @('xrtFutureWatchInitOwnershipV1','!frame->layout',
    'Validate(frame->layout,frame->dataSize)','!frame->payloadLive&&!frame->active&&!frame->runEntered&&!frame->source&&!frame->output')) {
    if (!$taskResident.Contains($required)) { throw "Task graph admission must validate closed layout and preserve pending duties: $required" }
}
if (!$taskOwnershipDomain.Contains('xrtTaskProducerPolicyV1Get') -or !$taskOwnershipDomain.Contains('xrtTaskOwnershipAdapterV1') -or
    !(Read-Utf8 'tools/check_xlir_task_storage.ps1').Contains('xlir_task_storage_ownership.c') -or
    !(Read-Utf8 'test/cases/xlir_task_storage_ownership.c').Contains('xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_ROOTED')) {
    throw 'Pending native jobs need real producer/data edges and a complete Plan lifecycle test with executor-root deferral'
}
if (!$source.Contains('XLIR async target has no exact Future completion outcome ABI') -or
    !(Read-Utf8 'src\cgen\xl6_ir_cgen_async_machine.c').Contains('XLIR scheduled async target has no verified ready-queue machine') -or
    !(Read-Utf8 'src\cgen\xl6_ir_cgen_async_contract.c').Contains('xlir_future_complete_error') -or
    !(Read-Utf8 'tools\check_xlir_async_pool_outcome.ps1').Contains('test/cases/xlir_async_pool_outcome_runtime.c')) {
    throw 'Every async dispatch needs a verified completion ABI and an exact frame program'
}
foreach ($required in @('taskPoolAbi', 'XL6_IR_MODULE_TASK_POOL_XRT_GUARDED_QUIESCE')) {
    if (!$header.Contains($required) -or !$source.Contains($required) -or !$lower.Contains($required)) {
        throw "Module task resources need an explicit lowered and verified ABI: $required"
    }
}
if (!$source.Contains('xl6IrModuleTaskPoolRequirement') -or
    !(Read-Utf8 'src/runtime/xl6_task_pool.c').Contains('XL6_ModulePrepareResult xl6TaskPoolModulePrepareV1(') -or
    !(Read-Utf8 'src/cgen/xl6_ir_cgen_function_emit.c').Contains('xl6TaskPoolModulePrepareV1(xlir_async_pool, teardown, wait)')) {
    throw 'Module global cleanup must follow canonical task resource quiescence'
}
foreach ($renderer in @('task_helpers')) {
    $body = Read-Utf8 "src\cgen\xl6_ir_cgen_$renderer.c"
    if (!$body.Contains('xlir_async_pool_submit') -or $body.Contains('xlir_async_pool_once') -or
        $body.Contains('xrtTaskSubmit(xlir_async_pool,')) {
        throw "All module task submissions must use the guarded resource: $renderer"
    }
}
$receiverIdentity = [regex]::Match($lower, '(?ms)^static XL6_IrTypeId xl6IrLowerClassReceiverType\(.*?^\}').Value
foreach ($renderer in @('async_machine', 'generator')) {
    $body = Read-Utf8 "src\cgen\xl6_ir_cgen_$renderer.c"
    foreach ($required in @('xl6IrCgenEmitActivationInputRollback', 'xl6IrCgenEmitActivationCapture')) {
        if (!$body.Contains($required)) { throw "Activation renderer must share input/capture/drop handling: $renderer $required" }
    }
    if ($renderer -eq 'generator') {
        $resident = Read-Utf8 'src/runtime/xl6_generator.c'
        if (!$body.Contains('xlir_generator_frame_discard(frame)') -or
            !$resident.Contains('xl6ErrorScopeBeginCleanup') -or
            !$resident.Contains('xl6ActivationCaptureRuntimeV1Get()->Cleanup')) {
            throw 'Closed generator cleanup and its outcome boundary must be resident'
        }
    } elseif (!$body.Contains('xl6ContinuationFrameRuntimeV2Get()->Cleanup') -or
        !(Read-Utf8 'src/runtime/xl6_continuation_frame.c').Contains('xl6ErrorScopeBeginCleanup')) {
        throw 'Async activation cleanup must share the resident payload interpreter'
    }
}
if (!$header.Contains('XL6_IR_ASYNC_FRAME_TRANSFER_TAKE_PARAMETER') -or
    !$source.Contains('xl6IrFunctionParameterBindingOwnsStorage') -or
    !$lower.Contains('xl6IrFunctionParameterBindingOwnsStorage')) {
    throw 'Consuming activation parameters require shared IR capture and worker cleanup contracts'
}
if (!$header.Contains('declarationOwnerType') -or !$source.Contains('xl6IrVerifyDeclarationOwners') -or
    !$receiverIdentity.Contains('node->declarationOwnerType') -or
    $receiverIdentity.Contains('classLayouts') -or $receiverIdentity.Contains('FindClassReceiverInSubtree')) {
    throw 'Member receiver identity must be imported and verified, not inferred from a body or the first class layout'
}
$propertyPlan = [regex]::Match($lower, '(?ms)^static int xl6IrLowerPreparePropertyTargetPlan\(.*?^\}').Value
$propertyEmit = [regex]::Match($lower, '(?ms)^static int xl6IrLowerEmitPropertyAccessor\(.*?^\}').Value
if (!$header.Contains('XL6_IR_CALL_ARGUMENT_PROPERTY_UPDATE') -or
    !$source.Contains('xl6IrVerifyPropertyUpdateSite') -or
    !$propertyPlan.Contains('xl6IrCallSiteGet') -or !$propertyPlan.Contains('xl6IrGetPropertyAccessorSignature') -or
    $propertyPlan.Contains('keys->children') -or $propertyPlan.Contains('property->firstChild') -or
    !$propertyEmit.Contains('XL6_IR_INSTR_PROTOCOL_CALL') -or
    !$propertyEmit.Contains('xl6IrLowerCompleteEvaluationCall')) {
    throw 'Property updates must share canonical CallSite access bindings and checked class/protocol completion'
}
$callSite = [regex]::Match($header, '(?s)typedef struct XL6_IrCallSite.*?} XL6_IrCallSite;').Value
foreach ($required in @('dispatchKind', 'dispatchSlot', 'receiverEvaluationIndex', 'preparedGetter')) {
    if (!$callSite.Contains($required)) { throw "CallSite must own dispatch and receiver evaluation: $required" }
}
foreach ($consumer in @('xl6IrLowerResolvedSyntaxCall', 'xl6IrLowerProtocolInvocation')) {
    $body = [regex]::Match($lower, "(?ms)^static XL6_IrValueId $consumer\(.*?^\}").Value
    if (!$body.Contains('xl6IrCallSiteEvaluationArgument') -or !$body.Contains('receiverEvaluationIndex')) {
        throw "Resolved syntax/protocol calls must consume normalized evaluation bindings: $consumer"
    }
}
if (!$source.Contains('xl6IrCallArgumentBindingIsExact') -or !$lower.Contains('xl6IrLowerOrderCallCleanup')) {
    throw 'CallSite source binding proof and reverse acquisition cleanup must remain explicit'
}
$stringConversion = [regex]::Match($lower, '(?ms)^static int xl6IrLowerIntrinsicStringConvert\(.*?^\}').Value
$stringValueConversion = [regex]::Match($lower, '(?ms)^static XL6_IrValueId xl6IrLowerConvertValueToString\(.*?^\}').Value
$interpolationConversion = [regex]::Match($lower, '(?ms)^static XL6_IrValueId xl6IrLowerInterpolatedStringPart\(.*?^\}').Value
if (!$stringConversion.Contains('xl6IrLowerConvertValueToString') -or
    !$stringConversion.Contains('xl6IrLowerCompleteEvaluationBorrows(ctx, id, first, result)') -or
    $stringConversion.Contains('source->valueCategory') -or
    !$stringValueConversion.Contains('xl6IrClassifyStringConversionAbi') -or
    !$stringValueConversion.Contains('xl6IrLowerProtectOwnedBorrow(ctx, failureOrigin, operand)') -or
    !$stringValueConversion.Contains('XL6_IR_INSTR_CALL') -or
    !$stringValueConversion.Contains('xl6IrLowerEmitRuntimeErrorEdge(ctx, failureOrigin,') -or
    !$interpolationConversion.Contains('xl6IrNodeMaterializedValueType') -or
    !$interpolationConversion.Contains('xl6IrLowerConvertValueToString') -or
    !$interpolationConversion.Contains('xl6IrLowerCompleteEvaluationBorrows(ctx, origin, first, &result)') -or
    $interpolationConversion.Contains('XL6_IR_RUNTIME_XVALUE_TO_STRING')) {
    throw 'String conversion must protect and retire producers through the shared evaluation owner transaction'
}
if (!$header.Contains('XL6_IR_DYNAMIC_CALL_MEMBER_GET') -or !$source.Contains('xl6IrNodeIsDynamicMemberRead')) {
    throw 'Dynamic property reads require an explicit typed call category and exceptional continuation'
}
$dynamicMethods = Read-Utf8 'src\cgen\xl6_ir_cgen_dynamic_methods.c'
foreach ($required in @('XL6_IR_DYNAMIC_CALL_MEMBER_SET', 'XL6_IR_DYNAMIC_CALL_RESULT_OWNED_UNIT_VALUE', 'XL6_IR_DYNAMIC_CALL_ARGUMENTS_FIRST')) {
    if (!$header.Contains($required) -or !$source.Contains($required) -or
        ($required -ne 'XL6_IR_DYNAMIC_CALL_RESULT_OWNED_UNIT_VALUE' -and !$lower.Contains($required))) {
        throw "Dynamic writes require explicit setter/result/evaluation ABI in Typed IR, verifier and lowering: $required"
    }
}
$dynamicLower = [regex]::Match($lower, '(?ms)^static XL6_IrValueId xl6IrLowerDynamicCall\(.*?^\}').Value
$dynamicResultPolicy = [regex]::Match($source, '(?ms)^XL6_IrDynamicCallResultAbi xl6IrDynamicCallExpectedResultAbi\(.*?^\}').Value
foreach ($required in @('XL6_IR_DYNAMIC_CALL_MEMBER_SET','XL6_IR_DYNAMIC_CALL_RESULT_OWNED_UNIT_VALUE',
    'XL6_IR_DYNAMIC_CALL_RESULT_DISCARD_TO_OWNED_UNIT','XL6_IR_DYNAMIC_CALL_RESULT_OWNED_FIRST_VALUE','XL6_AST_EXPR_STMT')) {
    if (!$dynamicResultPolicy.Contains($required)) { throw "Dynamic results require a closed source-demand policy: $required" }
}
if (!$dynamicLower.Contains('site->resultAbi != xl6IrDynamicCallExpectedResultAbi(ctx->module, site)') -or
    [regex]::Matches($source, 'site->resultAbi != xl6IrDynamicCallExpectedResultAbi\(module, site\)').Count -ne 2) {
    throw 'Typed and lowered verifiers plus lowering must validate the same dynamic result-demand policy'
}
$dynamicEmitter = Read-Utf8 'src\cgen\xl6_ir_cgen_runtime_async_instruction.c'
if (!$dynamicEmitter.Contains('site->resultAbi == XL6_IR_DYNAMIC_CALL_RESULT_DISCARD_TO_OWNED_UNIT') -or
    $dynamicEmitter.Contains('xl6IrDynamicCallExpectedResultAbi')) {
    throw 'Dynamic C emission must consume verified result demand, not recover statement context'
}
foreach ($required in @('xl6IrLowerCommitEvaluationOwnerRange', 'xl6IrLowerCompleteEvaluationCall')) {
    if (!$dynamicLower.Contains($required)) { throw "Dynamic calls must share operand ownership and outcome-first completion: $required" }
}
if (!$dynamicMethods.Contains('xl6ForeignCallBegin(&invocationScope)') -or
    !$dynamicMethods.Contains('xl6ErrorScopeCaptureFailure(&invocationScope)')) {
    throw 'Dynamic dispatch must isolate and preserve call and cleanup failure outcomes'
}
if (!$header.Contains('XL6_IR_DYNAMIC_MEMBER_BINDING_RETAIN_RECEIVER') -or
    !$source.Contains('xl6IrDynamicMethodMatchesCallSite') -or
    !$dynamicMethods.Contains('xl6IrCgenEmitDynamicMethodDispatcher') -or
    !$dynamicMethods.Contains('xlir_bound_method_entry') -or
    !$source.Contains('XLIR async mutable parameter has no owned activation storage')) {
    throw 'Bound method values require retained receiver IR, shared dispatch and verified mutable activation storage'
}
foreach ($retiredIndexHelper in @('RuntimeXValueIntegerSetI64', 'RuntimeXValueObjectSetLiteral',
                                 'RuntimeXValueObjectSetString', 'RuntimeXValueSetXValueKey',
                                 'RuntimeXValueObjectGetString', 'RuntimeXValueIndexXValueKey',
                                 'RuntimeXValueMemberSetLiteral', 'ClassFieldCompoundAssign',
                                 'ManagedStringCompoundAssign', 'CollectionAlgebraCompoundAssign',
                                 'OverloadedDirectCompoundAssign')) {
    if ($lower.Contains("xl6IrLower$retiredIndexHelper(")) {
        throw "Index evaluation must use shared transactions, not duplicate helper $retiredIndexHelper"
    }
}
$indexAssignment = [regex]::Match($lower,
    '(?ms)^static int xl6IrLowerElementAssign\(.*?^\}').Value
foreach ($required in @('XL6_IrLowerElementTargetPlan', 'xl6IrLowerAcquireErasedElementValue',
    'plan->memberAddress', 'plan->nativeDestroySymbol', 'plan->callableSignature')) {
    if (!$lower.Contains($required)) { throw "Field/index transactions lack the shared ownership ABI: $required" }
}
if (!(Read-Utf8 'test\suites\03_core_function_abi.bat').Contains('tools\check_xlir_member_assignment_transaction.ps1') -or
    !(Read-Utf8 'tools\check_xlir_null_abi_boundary.ps1').Contains('tools/check_xlir_member_assignment_transaction.ps1')) {
    throw 'Field and static writes must retain their source-built runtime gate'
}
if ($indexAssignment -notmatch '(?s)xl6IrLowerProtectOwnedBorrow.*xl6IrLowerPrepareElementTargetPlan.*xl6IrLowerWriteElementTargetPlan.*xl6IrLowerReleaseEvaluationBorrows') {
    throw 'Ordinary index assignment must protect RHS before the shared receiver/key transaction'
}
$indexKeyRead = [regex]::Match($lower,
    '(?ms)^static XL6_IrValueId xl6IrLowerRuntimeXValueKeyRead\([^;{]*\)\s*\{.*?^\}').Value
if ($indexKeyRead -notmatch '(?s)operands\[0\] = xl6IrLowerProtectOwnedBorrow.*operands\[1\] = xl6IrLowerNode.*xl6IrLowerProjectBorrowedIndexResult.*xl6IrLowerCompleteEvaluationBorrows\(ctx, origin, evaluationFirst, &result\)') {
    throw 'Non-literal index reads must protect the receiver before key evaluation and own the result before cleanup'
}
$conditionalIndex = [regex]::Match($lower,
    '(?ms)^static int xl6IrLowerElementAssignIfNull\(.*?^\}').Value
foreach ($required in @('xl6IrLowerPrepareElementTargetPlan', 'xl6IrLowerBorrowElementTargetPlan',
                        'xl6IrLowerEmitElementTargetWrite', 'xl6IrLowerReleaseElementTargetPlan')) {
    if (!$conditionalIndex.Contains($required)) { throw "Conditional index writes must share the address transaction: $required" }
}
if ($conditionalIndex -match 'xl6IrLowerSubtreeMayEscapeRaise|receiver->kind|retainedReceiver' -or
    !(Read-Utf8 'test\suites\03_core_function_abi.bat').Contains('tools\check_xlir_index_conditional_assign.ps1') -or
    !(Read-Utf8 'tools\check_xlir_null_abi_boundary.ps1').Contains('tools/check_xlir_index_conditional_assign.ps1')) {
    throw 'Lazy index writes must support raising RHS and expression receivers through a wired source/runtime contract'
}
$conditionalStorage = [regex]::Match($lower,
    '(?ms)^static int xl6IrLowerStorageAssignIfNull\(.*?^\}').Value
foreach ($required in @('XL6_IR_NULL_ABI_TAGGED_VALUE', 'xl6IrLowerPrepareStorageTargetPlan',
    'xl6IrLowerReadStorageTargetPlan', 'xl6IrLowerEmitStorageAssignment', 'XL6_IR_INSTR_NULL_TEST')) {
    if (!$conditionalStorage.Contains($required)) { throw "Conditional storage must separate null and owner/cell ABIs: $required" }
}
$storageCommit = [regex]::Match($lower,
    '(?ms)^static int xl6IrLowerWriteStorageTargetPlan\(.*?^\}').Value
$storageCompound = [regex]::Match($lower,
    '(?ms)^static int xl6IrLowerStorageCompoundAssign\(.*?^\}').Value
$storageReplacement = [regex]::Match($lower,
    '(?ms)^static int xl6IrLowerReplaceStorageValue\([^;]*?\{.*?^\}').Value
if (!$storageCommit.Contains('xl6IrLowerReplaceStorageValue') -or
    $storageReplacement -notmatch '(?s)xl6IrLowerAcquireValueOwner.*XL6_IR_INSTR_CELL_EXCHANGE.*XL6_IR_INSTR_LOAD.*XL6_IR_INSTR_MOVE.*XL6_IR_INSTR_STORE.*xl6IrLowerCompleteOwnedCleanup' -or
    $storageCompound -notmatch '(?s)xl6IrLowerPrepareStorageTargetPlan.*xl6IrLowerReadStorageTargetPlan.*xl6IrLowerCompoundValue.*xl6IrLowerWriteStorageTargetPlan.*xl6IrLowerReleaseEvaluationBorrows' -or
    !(Read-Utf8 'test\suites\03_core_function_abi.bat').Contains('tools\check_xlir_storage_assignment_transaction.ps1')) {
    throw 'Direct/cell/static updates require one source-tested read-before-RHS and publish-before-cleanup transaction'
}
if (!$header.Contains('XL6_IR_INSTR_CELL_EXCHANGE') -or
    !$source.Contains('XLIR managed cell replacement requires explicit exchange and cleanup') -or
    !$ownership.Contains('case XL6_IR_INSTR_CELL_EXCHANGE:') -or
    !(Read-Utf8 'test\suites\03_core_function_abi.bat').Contains('tools\check_xlir_storage_exchange.ps1') -or
    !(Read-Utf8 'tools\check_xlir_storage_exchange.ps1').Contains('xlir_storage_exchange_contract.c')) {
    throw 'Managed cell exchange must expose the previous owner and have a wired invalid-IR/reentry runtime gate'
}
$cellPrelude = Read-Utf8 'src\cgen\xl6_ir_cgen_emit_prelude.c'
if (!$header.Contains('XL6_IR_ASYNC_FRAME_TRANSFER_BORROW_STORAGE') -or
    !$source.Contains('xl6IrContinuationBorrowFrameSlot') -or
    !$source.Contains('XLIR async frame loan has no exact resident owner') -or
    !$source.Contains('XLIR generator frame loan has no exact resident owner') -or
    !(Read-Utf8 'test\suites\03_core_function_abi.bat').Contains('tools\check_xlir_product_await_owner.ps1') -or
    !(Read-Utf8 'tools\check_xlir_product_await_owner.ps1').Contains('xlir_product_await_owner_contract.c')) {
    throw 'Continuation frame loans require verified resident owners and a wired genuine-suspension failure gate'
}
$cellHeader = Read-Utf8 'src/runtime/xl6_cell.h'
$cellRuntime = Read-Utf8 'src/runtime/xl6_cell.c'
$cellTypeCallEmitter = Read-Utf8 'src/cgen/xl6_ir_cgen_type_call.c'
$cellInstruction = Read-Utf8 'src/cgen/xl6_ir_cgen_identity_closure.c'
foreach ($operation in @('initialize','exchange')) {
    $transferBody=[regex]::Match($cellRuntime,"(?ms)^static void cell_${operation}_body\(.*?^\}").Value
    if (!$transferBody -or $transferBody -match 'xrtMalloc|xrtFree|xrtValueRelease' -or !$transferBody.Contains('memcpy')) {
        throw "Resident cell transfer must not hide destruction or a failure boundary: $operation"
    }
}
if ($cellPrelude -match 'struct xlir_cell|xlir_cell_ownership_count|xlir_cell_new_' -or
    !$cellPrelude.Contains('__xl_runtime/xl6_cell.h') -or !$header.Contains('XL6_IR_CELL_STORAGE_RESIDENT_TRANSFER_V1') -or
    !$source.Contains('cell storage/resident transfer ABI is unresolved') -or
    !$cellInstruction.Contains('xl6CellRuntimeV1Get()->Exchange') -or
    !$cellRuntime.Contains('cell_owning(cell)') -or !$cellRuntime.Contains('XL6_OWNERSHIP_RETURN_VOID(cell_exchange_body')) {
    throw 'Verified cell storage must use a resident, complete owning exchange with no generated lifecycle replica'
}
$compoundValue = [regex]::Match($lower,
    '(?ms)^static XL6_IrValueId xl6IrLowerCompoundValue\(.*?^\}').Value
if ($compoundValue -notmatch '(?s)xl6IrLowerAcquireValueOwner.*xl6IrLowerProtectOwnedBorrow.*xl6IrLowerNodeAsType') {
    throw 'All compound destinations must preserve the previous value through the shared typed operation'
}
foreach ($consumer in @('StorageCompoundAssign', 'ElementCompoundAssign', 'ResolvedPropertyCompoundAssign')) {
    $body = [regex]::Match($lower, "(?ms)^static int xl6IrLower$consumer\(.*?^\}").Value
    if (!$body.Contains('xl6IrLowerCompoundValue') -or $body.Contains('xl6IrLowerScalarCompoundValue')) {
        throw "Compound destinations must not duplicate operator selection: $consumer"
    }
}
$propertyEmit = [regex]::Match($lower,
    '(?ms)^static int xl6IrLowerEmitPropertyAccessor\(.*?^\}').Value
if (!$propertyEmit.Contains('xl6IrLowerCompleteEvaluationCall') -or !$propertyEmit.Contains('xl6IrLowerCompleteCall') -or
    !$propertyEmit.Contains('xl6IrLowerProjectCallTargetPhysicalOperands')) {
    throw 'Property calls require shared physical projection, outcome-first completion and whole-transaction cleanup'
}
foreach ($consumer in @('OwnedManagedStringValue', 'OwnedRefCountedPointerValue')) {
    $body = [regex]::Match($lower, "(?ms)^static XL6_IrValueId xl6IrLower$consumer\(.*?^\}").Value
    if (!$body.Contains('xl6IrLowerAcquireValueOwner') -or $body -match 'sourceNode->ownership|sourceNode->valueCategory') {
        throw "Managed boundaries must recognize explicit post-cleanup owner transfers: $consumer"
    }
}
$ownedReturn = [regex]::Match($lower,
    '(?ms)^static XL6_IrValueId xl6IrLowerOwnedReturnExpression\(.*?^\}').Value
if ($lower -notmatch 'xl6IrLowerOwnedReturnExpression\(ctx, id,\s*ctx->module->children\[node->firstChild\], returned, deferred->type\)' -or
    $lower -notmatch 'xl6IrLowerOwnedReturnExpression\(ctx, id,\s*ctx->module->children\[node->firstChild\], returned, returnType\)' -or
    $ownedReturn -notmatch '(?s)xl6IrLowerNodeAsType\(ctx, origin, sourceId, source, targetType\).*xl6IrLowerOwnedValueBoundary\(ctx, origin, targetType, source, value\)' -or
    $ownedReturn -match 'source->ownership|source->valueCategory' -or
    !(Read-Utf8 'tools\check_xlir_callable_contextual.ps1').Contains('xlir_callable_contextual_host.c')) {
    throw 'Ordinary and deferred returns must share conversion followed by producer-based ownership at the normalized return TypeId, with runtime regression coverage'
}
$ownerAcquisition = [regex]::Match($lower,
    '(?ms)^static XL6_IrValueId xl6IrLowerAcquireValueOwner\(.*?^\}').Value
if ($ownerAcquisition -match 'valueCategory|sourceNode->ownership' -or
    !$ownerAcquisition.Contains('xl6IrLowerValueHasOwner')) {
    throw 'Storage snapshots must use the lowered producer contract, never frontend ownership hints'
}
if ($lower.Contains('xl6IrLowerManagedAssignIfNull(') -or $conditionalStorage -match 'xvalueSentinel|pointerSentinel' -or
    !(Read-Utf8 'test\suites\03_core_function_abi.bat').Contains('tools\check_xlir_storage_conditional_assign.ps1') -or
    !(Read-Utf8 'tools\check_xlir_null_abi_boundary.ps1').Contains('tools/check_xlir_storage_conditional_assign.ps1')) {
    throw 'Conditional direct/cell storage must use a wired source/runtime contract, not managed-sentinel-only admission'
}
foreach ($renderer in @('instruction_emit', 'async_instruction', 'generator')) {
    $rendererSource = Read-Utf8 "src\cgen\xl6_ir_cgen_$renderer.c"
    if (!$rendererSource.Contains('xl6IrCgenEmitCellInstruction(') -or
        !$rendererSource.Contains('xl6IrCgenEmitMoveInstruction(') -or
        $rendererSource -match 'xlir_cell_(new|load|store)_') {
        throw "Cell instructions must use one shared payload renderer: $renderer"
    }
    $runtimeRender = [regex]::Matches($rendererSource,
        '(?ms)^\s*case XL6_IR_INSTR_RUNTIME_CALL:.*?(?=^\s*case XL6_IR_INSTR_)')
    $sharedRenders = @($runtimeRender | Where-Object { $_.Value.Contains('xl6IrCgenEmitRuntimeCallInstruction') })
    if ($sharedRenders.Count -ne 1 -or
        @($runtimeRender | Where-Object { $_.Value -match 'XL6_IR_RUNTIME_' }).Count -ne 0) {
        throw "Sync, async and generator RuntimeOps must use the same renderer and global ABI contract: $renderer"
    }
}
if (!$source.Contains('!xl6IrVerifyValueLifetimes(module, diags)') -or
    !$source.Contains('#include "xl6_ir_lifetime.c"') -or
    !(Read-Utf8 "tools\check_xlir_value_lifetime.ps1").Contains("xlir_value_lifetime_contract.c") -or
    !(Read-Utf8 "test\suites\03_core_function_abi.bat").Contains("tools\check_xlir_value_lifetime.ps1")) {
    throw 'Borrow protection must be verified before C/header emission and wired into the core suite'
}
if (!(Read-Utf8 'src/ir/xl6_ir_lifetime.c').Contains('case XL6_IR_INSTR_YIELD:') -or
    !(Read-Utf8 'tools/check_xlir_generator_frame_loan.ps1').Contains('xlir_generator_frame_loan_contract.c') -or
    !(Read-Utf8 'test/suites/03_core_function_abi.bat').Contains('tools\check_xlir_generator_frame_loan.ps1') -or
    !(Read-Utf8 'src/cgen/xl6_ir_cgen_async_contract.c').Contains('xl6ErrorScopePropagateFailure(capture_outcome)')) {
    throw 'Continuation loans and full activation cleanup outcomes need a source-built generator gate'
}
if (!(Read-Utf8 'test/suites/03_core_function_abi.bat').Contains('tools\check_xlir_continuation_owner_views.ps1') -or
    !(Read-Utf8 'tools/check_xlir_continuation_owner_views.ps1').Contains('xlir_continuation_owner_views_contract.c') -or
    !(Read-Utf8 'test/suites/03_core_function_abi.bat').Contains('tools\check_xlir_continuation_owner_cfg.ps1') -or
    !(Read-Utf8 'tools/check_xlir_continuation_owner_cfg.ps1').Contains('xlir_continuation_owner_cfg_contract.c') -or
    !(Read-Utf8 'test/suites/03_core_function_abi.bat').Contains('tools\check_xlir_continuation_owner_calls.ps1') -or
    !(Read-Utf8 'tools/check_xlir_continuation_owner_calls.ps1').Contains('check_xlir_continuation_owner_calls.py') -or
    !(Read-Utf8 'tools/check_xlir_continuation_owner_calls.py').Contains('xlir_continuation_owner_calls_contract.c') -or
    !$source.Contains('xl6IrContinuationVerifyLiveSets(module, function, 0, machineIndex)') -or
    !$source.Contains('xl6IrContinuationVerifyLiveSets(module, function, 1, machineIndex)')) {
    throw 'Continuation owner/view safety regression must remain wired into the core suite'
}
foreach ($case in @('views', 'cfg', 'calls')) {
    if (!(Read-Utf8 "test/cases/xlir_continuation_owner_${case}_contract.c").Contains('syntheticCloseOwnerExits') -or
        !(Read-Utf8 "test/cases/xlir_continuation_owner_${case}_runtime.c").Contains('xlir_owner_fault_runtime.h')) {
        throw "Synthetic continuation $case owners require explicit failure sinks and runtime allocation coverage"
    }
}
if (!(Read-Utf8 'test/cases/xlir_continuation_owner_test.h').Contains('xlir_synthetic_owner_edit.h') -or
    !(Read-Utf8 'test/cases/xlir_synthetic_owner_edit.h').Contains('XL6_IR_INSTR_VALUE_RELEASE') -or
    !(Read-Utf8 'test/cases/xlir_synthetic_owner_edit.h').Contains('XL6_IR_INSTR_GENERATOR_CLOSE_TEST') -or
    !(Read-Utf8 'test/cases/xlir_owner_fault_runtime.h').Contains('xrtMemDebugFailTriggered')) {
    throw 'Synthetic IR arena edits and real allocation-fault checks must remain shared test infrastructure'
}
if (!(Read-Utf8 'src/cgen/xl6_ir_cgen_async_instruction.c').Contains('suspension->firstHandoffAction') -or
    (Read-Utf8 'src/cgen/xl6_ir_cgen_async_instruction.c').Contains('module->asyncLiveSlots') -or
    !$header.Contains('XL6_IR_ASYNC_FRAME_ACTION_HANDOFF_RESIDENT') -or
    !$source.Contains('"handoff-resident"')) {
    throw 'Nested activations must execute verified unique resident handoff actions, not consume live view memberships'
}
if (!$header.Contains('XL6_IR_ASYNC_FRAME_ACTION_ROLLBACK_HANDOFF_RESIDENT') -or
    !$header.Contains('setupFailureEntry') -or
    !$source.Contains('XLIR async setup failure has no exact failed-await entry') -or
    !(Read-Utf8 'src/cgen/xl6_ir_cgen_async_instruction.c').Contains('suspension->firstRollbackAction') -or
    !(Read-Utf8 'src/cgen/xl6_ir_cgen_async_contract.c').Contains('suspension->setupFailureEntry')) {
    throw 'Async setup failure must use a verified failed-await entry and explicit resident rollback'
}
if (!$header.Contains('XL6_IR_ASYNC_RESULT_COMMIT_OWNED_TRANSACTION') -or
    !$source.Contains('XLIR async result commit transaction is unresolved') -or
    !(Read-Utf8 'src/cgen/xl6_ir_cgen_async_instruction.c').Contains('xl6IrCgenEmitAsyncResultCommit') -or
    (Read-Utf8 'src/cgen/xl6_ir_cgen_async_instruction.c').Contains('xl6IrTypeGet(&module->types, target->workerReturnType)->cAbi') -or
    !(Read-Utf8 'test/suites/03_core_function_abi.bat').Contains('check_xlir_async_result_commit.ps1')) {
    throw 'Async RETURN must use a verified shared result transaction and its core allocation-failure gate'
}
if (!$header.Contains('XL6_IR_ASYNC_COMPLETION_RETIRE_BEFORE_PUBLISH') -or
    !$source.Contains('XLIR async completion cleanup barrier is unresolved') -or
    !$source.Contains('completionAbiName') -or
    !(Read-Utf8 'src/cgen/xl6_ir_cgen_async_machine.c').Contains('xlir_async_machine_frame_retire_') -or
    !(Read-Utf8 'src/cgen/xl6_ir_cgen_async_instruction.c').Contains('machine->completionAbi') -or
    !(Read-Utf8 'test/cases/xlir_async_frame_completion_contract.c').Contains('machine->destroyActionCount') -or
    !(Read-Utf8 'test/suites/03_core_function_abi.bat').Contains('check_xlir_async_frame_completion.ps1')) {
    throw 'Async completion must retire verified frame residents before Future publication and preserve the first failure'
}
if (!$header.Contains('XL6_IR_ASYNC_CANCELLATION_RESIDENT_AWAIT_ONCE_V2') -or
    !$source.Contains('XLIR async cancellation delivery transport is unresolved') -or
    !$source.Contains('xl6IrContinuationHandoffResident') -or
    !(Read-Utf8 'src/cgen/xl6_ir_cgen_async_machine.c').Contains('machine->transportAbi') -or
    !(Read-Utf8 'src/cgen/xl6_ir_cgen_async_machine.c').Contains('xl6ContinuationWatchRegister') -or
    !(Read-Utf8 'src/runtime/xl6_continuation_future.c').Contains('xrtFutureWatchAdd') -or
    (Read-Utf8 'src/cgen/xl6_ir_cgen_async_instruction.c').Contains('xrtFutureContinueOwned(') -or
    !(Read-Utf8 'test/suites/03_core_function_abi.bat').Contains('check_xlir_async_cancel_watch.ps1')) {
    throw 'Async machines must own cancellation delivery and all resident owners through the verified Watch handoff'
}
$asyncMachineEmitter = Read-Utf8 'src/cgen/xl6_ir_cgen_async_machine.c'
if (!$header.Contains('XL6_IR_AWAIT_OUTCOME_STATUS_DIAGNOSTIC_NULL_STATE_ERROR') -or
    !$source.Contains('XLIR instruction has an ambiguous await status/diagnostic ABI') -or
    !$lower.Contains('XL6_IR_AWAIT_OUTCOME_STATUS_DIAGNOSTIC_NULL_STATE_ERROR') -or
    !$asyncMachineEmitter.Contains('await_ok_%u = observed.ok') -or
    !$asyncMachineEmitter.Contains('\"await on null future\"') -or
    !(Read-Utf8 'src/cgen/xl6_ir_cgen_future_contract.c').Contains('xl6IrCgenEmitAwaitPayload') -or
    (Read-Utf8 'src/cgen/xl6_ir_cgen_emit_prelude.c').Contains('xlir_future_await_')) {
    throw 'Await needs one verified status/diagnostic acquisition contract, including source cleanup, without ambiguous per-type wrappers'
}
foreach ($required in @('XL6_IR_ASYNC_EXECUTION_READY_QUEUE_ROOT_V2', 'XL6_IR_MODULE_CONTINUATION_FIXED_WORKERS_V1')) {
    if (!$header.Contains($required) -or !$source.Contains($required)) {
        throw "Async execution/module service requires a verified IR ABI: $required"
    }
}
foreach ($required in @('machine->executionAbi','xl6ContinuationFrameLaunchV1',
    'xl6ContinuationReserveSuccessor','xl6ContinuationAbort','xl6ContinuationWatchRegister')) {
    if (!$asyncMachineEmitter.Contains($required)) { throw "Async owned admission is missing: $required" }
}
if ($asyncMachineEmitter.Contains('xrtFutureWatchAdd') -or $asyncMachineEmitter.Contains('xrtFutureWatchInit') -or
    !(Read-Utf8 'src/cgen/xl6_ir_cgen_async_instruction.c').Contains('xl6ContinuationCommit') -or
    !(Read-Utf8 'src/cgen/xl6_ir_cgen_async_instruction.c').Contains('xl6ContinuationComplete(&frame->ticket)') -or
    !(Read-Utf8 'src/cgen/xl6_ir_cgen_function_emit.c').Contains('xl6ContinuationExecutorCreateFixedV1') -or
    !(Read-Utf8 'src/cgen/xl6_ir_cgen_module_emit.c').Contains('xl6ContinuationExecutorDestroy')) {
    throw 'Generated async must use resident ticket transport and a fallible module-owned executor, never inline Future callbacks'
}
if (!$header.Contains('XL6_IR_ASYNC_SOURCE_LEASE_CAPTURE_THEN_RELEASE') -or
    !$source.Contains('XLIR async source lease lifetime is unresolved') -or
    !(Read-Utf8 'src/cgen/xl6_ir_cgen_async_machine.c').Contains('machine->sourceLeaseAbi') -or
    !(Read-Utf8 'src/cgen/xl6_ir_cgen_async_machine.c').Contains('xl6IrCgenEmitAsyncSourceLeaseRelease(out, module, suspension)') -or
    !(Read-Utf8 'test/suites/03_core_function_abi.bat').Contains('check_xlir_async_source_lease.ps1')) {
    throw 'Async source transport leases must end after result acquisition and before the verified resume CFG'
}
if (!$source.Contains('!xl6IrVerifyOwnerConsumption(module, diags)') -or
    !(Read-Utf8 "tools\check_xlir_owner_consumption.ps1").Contains("xlir_owner_consumption_contract.c") -or
    !(Read-Utf8 "test\suites\03_core_function_abi.bat").Contains("tools\check_xlir_owner_consumption.ps1")) {
    throw 'Affine owner consumption must be wired into module verification and the core suite'
}
if (!(Read-Utf8 'test/suites/03_core_function_abi.bat').Contains('check_xlir_string_allocation_outcome.ps1') -or
    !(Read-Utf8 'tools/check_xlir_string_allocation_outcome.ps1').Contains('xlir_string_allocation_outcome_contract.c') -or
    !(Read-Utf8 'test/cases/xlir_string_allocation_outcome_contract.c').Contains('XL6_IR_CALL_ERROR_PHYSICAL_NULL_RESULT') -or
    !(Read-Utf8 'test/cases/xlir_string_allocation_outcome_runtime.c').Contains('xrtMemDebugFailTriggered') -or
    !$header.Contains('xl6IrRuntimeStringAllocationErrorPhysicalAbi') -or
    !$source.Contains('xl6IrRuntimeStringAllocationErrorPhysicalAbi') -or
    !$lower.Contains('xl6IrLowerEmitRuntimeCallPayload')) {
    throw 'Checked string allocations must share an IR outcome family, complete producer builder and real allocation-failure runtime coverage'
}
if (!$ownership.Contains('owner reaches function exit without transfer or destruction') -or
    !$ownership.Contains('owner overwritten by repeated acquisition without a sink') -or
    !(Read-Utf8 "tools\check_xlir_owner_completion.ps1").Contains("xlir_owner_consumption_contract.c") -or
    !(Read-Utf8 "test\suites\03_core_function_abi.bat").Contains("tools\check_xlir_owner_completion.ps1")) {
    throw 'Non-product SSA owner completion must be an IR verifier obligation with current-source/runtime admission'
}
if (!(Read-Utf8 'test/suites/03_core_function_abi.bat').Contains('check_xlir_enum_allocation_outcome.ps1') -or
    !(Read-Utf8 'tools/check_xlir_enum_allocation_outcome.ps1').Contains('xlir_enum_allocation_outcome_contract.c') -or
    !(Read-Utf8 'test/cases/xlir_enum_allocation_outcome_contract.c').Contains('XL6_IR_CALL_ERROR_PHYSICAL_FALSE_RESULT') -or
    !(Read-Utf8 'test/cases/xlir_enum_allocation_outcome_runtime.c').Contains('xlir_owner_fault_runtime.h') -or
    !(Read-Utf8 'src/cgen/xl6_ir_cgen_emit_xvalue_containers.c').Contains('xlir_tagged_enum_new')) {
    throw 'Enum allocation and consuming insertion require explicit outcomes and a source-built fault-injection gate'
}
if (!(Read-Utf8 'test/suites/03_core_function_abi.bat').Contains('check_xlir_boxing_allocation_outcome.ps1') -or
    !(Read-Utf8 'tools/check_xlir_boxing_allocation_outcome.ps1').Contains('xlir_boxing_allocation_outcome_contract.c') -or
    !(Read-Utf8 'test/cases/xlir_boxing_allocation_outcome_contract.c').Contains('XL6_IR_CALL_ERROR_PHYSICAL_NULL_RESULT') -or
    !(Read-Utf8 'test/cases/xlir_boxing_allocation_outcome_runtime.c').Contains('xlir_owner_fault_runtime.h') -or
    !$lower.Contains('xl6IrRefreshConversionAbis(module)') -or
    !$source.Contains('xl6IrRuntimeBoxingErrorPhysicalAbi')) {
    throw 'Allocating boxing requires explicit outcomes, canonical specialization refresh and a source-built fault-injection gate'
}
if (!$header.Contains('XL6_IR_EXIT_POINT_FUNCTION_FAILURE') -or
    !$source.Contains('XLIR adapter cleanup must observe failure at its own function boundary') -or
    $lower.Contains('xl6IrLowerNamedAdapterDefaultCleanup') -or
    !(Read-Utf8 'test/suites/03_core_function_abi.bat').Contains('check_xlir_adapter_cleanup_outcome.ps1') -or
    !(Read-Utf8 'tools/check_xlir_adapter_cleanup_outcome.ps1').Contains('xlir_adapter_cleanup_outcome_contract.c') -or
    !(Read-Utf8 'test/cases/xlir_adapter_cleanup_outcome_runtime.c').Contains('xlir_owner_fault_runtime.h')) {
    throw 'Generated adapters require an independent failure boundary and shared outcome-bearing owner cleanup'
}
if (!$source.Contains('XLIR closure allocation requires an immediate exact outcome branch') -or
    !$source.Contains('xl6IrExitPlanEvaluationOrigin') -or
    !(Read-Utf8 'test/suites/03_core_function_abi.bat').Contains('check_xlir_closure_allocation_outcome.ps1') -or
    !(Read-Utf8 'tools/check_xlir_closure_allocation_outcome.ps1').Contains('xlir_closure_allocation_outcome_contract.c') -or
    !(Read-Utf8 'test/cases/xlir_closure_allocation_outcome_runtime.c').Contains('xlir_owner_fault_runtime.h')) {
    throw 'Closure construction requires checked outcomes, creator-owned failure routes and a source-built fault-injection gate'
}
if (!$header.Contains('XL6_IR_INSTR_CELL_INIT') -or
    !$source.Contains('xl6IrVerifyCellConstruction') -or
    !$lower.Contains('xl6IrLowerConstructCell') -or
    !(Read-Utf8 'test/suites/03_core_function_abi.bat').Contains('check_xlir_cell_allocation_outcome.ps1') -or
    !(Read-Utf8 'tools/check_xlir_cell_allocation_outcome.ps1').Contains('xlir_cell_allocation_outcome_contract.c') -or
    !(Read-Utf8 'test/cases/xlir_cell_allocation_outcome_runtime.c').Contains('xlir_owner_fault_runtime.h')) {
    throw 'Cell allocation requires checked empty-owner results, success-only installation and a source-built entry fault gate'
}
if (!$header.Contains('loopTargetCount') -or !$lower.Contains('xl6IrLowerInitializeIterationTarget') -or
    !$lower.Contains('xl6IrLowerFinishIteration') -or
    !(Read-Utf8 'test/suites/03_core_function_abi.bat').Contains('check_xlir_cell_loop_binding.ps1') -or
    !(Read-Utf8 'tools/check_xlir_cell_loop_binding.ps1').Contains('xlir_cell_loop_binding_contract.c') -or
    !(Read-Utf8 'test/cases/xlir_cell_loop_binding_runtime.c').Contains('xlir_owner_fault_runtime.h')) {
    throw 'Loop binding cells require explicit initialized prefixes, shared installation/cleanup and real allocation-failure verification'
}
if (!(Read-Utf8 'test/suites/03_core_function_abi.bat').Contains('check_xlir_dynamic_iteration_allocation.ps1') -or
    !(Read-Utf8 'tools/check_xlir_dynamic_iteration_allocation.ps1').Contains('xlir_dynamic_iteration_allocation_contract.c') -or
    !(Read-Utf8 'test/cases/xlir_dynamic_iteration_allocation_contract.c').Contains('XL6_IR_CALL_ERROR_PHYSICAL_NULL_RESULT') -or
    !(Read-Utf8 'test/cases/xlir_dynamic_iteration_allocation_runtime.c').Contains('source_close_case') -or
    !(Read-Utf8 'test/cases/xlir_dynamic_iteration_allocation_runtime.c').Contains('finalized == (unsigned)entered')) {
    throw 'Dynamic iteration must retain real allocation, cursor, and source-level early-close finalization assertions'
}
if (!(Read-Utf8 'test/suites/03_core_function_abi.bat').Contains('check_xlir_generator_close_routes.ps1') -or
    !(Read-Utf8 'tools/check_xlir_generator_close_routes.ps1').Contains('xlir_generator_close_routes_contract.c') -or
    !(Read-Utf8 'test/cases/xlir_generator_close_routes_contract.c').Contains('cleanup-only unwind prefix') -or
    !(Read-Utf8 'test/cases/xlir_generator_close_routes.xl').Contains('closePending') -or
    !(Read-Utf8 'test/cases/xlir_generator_close_routes_runtime.c').Contains('continuation_owner_fault_balanced') -or
    !(Read-Utf8 'tools/check_xlir_generator_close_routes.ps1').Contains('xlir_generator_close_abrupt_finally_contract.c') -or
    !(Read-Utf8 'test/cases/xlir_generator_close_abrupt_finally_contract.c').Contains('closing path escapes its cleanup boundary') -or
    !(Read-Utf8 'test/cases/xlir_generator_close_abrupt_finally_runtime.c').Contains('continuation_owner_fault_balanced') -or
    !(Read-Utf8 'tools/check_xlir_generator_close_routes.ps1').Contains('xlir_generator_close_catch_dispatch_contract.c') -or
    !(Read-Utf8 'test/cases/xlir_generator_close_catch_dispatch_runtime.c').Contains('continuation_owner_fault_balanced') -or
    !(Read-Utf8 'tools/check_xlir_generator_close_routes.ps1').Contains('xlir_generator_close_first_outcome_contract.c') -or
    !(Read-Utf8 'test/cases/xlir_generator_close_first_outcome_runtime.c').Contains('close_outcome_repeated')) {
    throw 'Generator close requires explicit verified unwind, active-finally suspension, pending outcomes and source-real runtime coverage'
}
if (!(Read-Utf8 'test/suites/03_core_function_abi.bat').Contains('check_xlir_cell_continuation_outcome.ps1') -or
    !(Read-Utf8 'tools/check_xlir_cell_continuation_outcome.ps1').Contains('xlir_cell_continuation_outcome_contract.c') -or
    !(Read-Utf8 'test/cases/xlir_cell_continuation_outcome_runtime.c').Contains('xlir_owner_fault_runtime.h') -or
    !(Read-Utf8 'test/cases/xlir_cell_continuation_outcome_runtime.c').Contains('managed_case')) {
    throw 'Suspended capture cells require source-real allocation faults, every generator close and managed-cell destructor/terminal precedence'
}
if (!(Read-Utf8 'test/suites/03_core_function_abi.bat').Contains('check_xlir_cell_thread_lifetime.ps1') -or
    !(Read-Utf8 'tools/check_xlir_cell_thread_lifetime.ps1').Contains('xlir_cell_thread_lifetime_contract.c') -or
    !$source.Contains('captures of one source binding must share the same cell identity') -or
    !$cellRuntime.Contains('xrtRefRetain(&cell->refs)') -or
    !$cellRuntime.Contains('xrtRefRelease(&cell->refs)') -or
    $cellRuntime.Contains('++cell->refs') -or $cellRuntime.Contains('--cell->refs')) {
    throw 'Shared binding capture identity and atomic cell ownership require a source-built concurrent lifetime gate'
}
if (!(Read-Utf8 'test/suites/03_core_function_abi.bat').Contains('check_xlir_cell_cancel_race.ps1') -or
    !(Read-Utf8 'tools/check_xlir_cell_cancel_race.ps1').Contains('xlir_cell_cancel_race_contract.c') -or
    !(Read-Utf8 'test/cases/xlir_cell_cancel_race_host.c').Contains('xrtFutureWatchAdd(source, watch)') -or
    !(Read-Utf8 'test/cases/xlir_cell_cancel_race_runtime.c').Contains('registrationReturn') -or
    !(Read-Utf8 'test/cases/xlir_cell_cancel_race_runtime.c').Contains('continuation_owner_fault_balanced')) {
    throw 'Cell cancellation requires real transport overlap, independent producers, once-only delivery and balanced cleanup verification'
}
# The compiled enum expands all X-macro families and excludes COUNT. Compare
# production effects with independent consume-position expectations; reject
# NONE, COUNT and unknown operations instead of matching source case labels.
& "$PSScriptRoot/check_xlir_runtime_operand_catalog.ps1" -Root $Root -PublicDirectory $PublicDirectory -CatalogOnly
if (!$?) { throw 'Compiled RuntimeOp input ownership catalog failed' }
if (!$header.Contains('XL6_IR_OPERAND_OWNERSHIP_CONSUME_FIELD') -or
    !$ownership.Contains('instruction->elementIndex != field') -or
    $source.Contains('size_t wholeTransfers = 0;') -or $source.Contains('elementConsumes[element] != 1') -or
    !$lower.Contains('xl6IrLowerProductBindings') -or
    !(Read-Utf8 "test\cases\xlir_owner_consumption_contract.c").Contains('xlir_product_owner_graphs.h') -or
    !(Read-Utf8 "test\suites\03_core_function_abi.bat").Contains('tools\check_xlir_product_owner.ps1')) {
    throw 'Product field ownership must use CFG completion and protected bindings, not global consumption counts'
}
if (!$source.Contains("xl6IrVerifyManagedStorageInputs") -or !$lower.Contains("xl6IrLowerReadJoinStorage") -or
    !$lower.Contains("xl6IrLowerProtectOwnedBorrow") -or
    !(Read-Utf8 "tools\check_xlir_managed_storage_inputs.ps1").Contains("xlir_managed_storage_inputs_contract.c") -or
    !(Read-Utf8 "test\suites\03_core_function_abi.bat").Contains("tools\check_xlir_managed_storage_inputs.ps1")) {
    throw "Every managed storage write requires independent acquisition and explicit match/borrow lifetimes"
}
if (!(Read-Utf8 "src\sem\xl6_sem.c").Contains("xl6SemInferConditionalType") -or
    !$lower.Contains("xl6IrLowerOwnedValueBoundary") -or
    !(Read-Utf8 "tools\check_xlir_conditional_nullable_join.ps1").Contains("xlir_conditional_nullable_join_contract.c") -or
    !(Read-Utf8 "test\suites\03_core_function_abi.bat").Contains("tools\check_xlir_conditional_nullable_join.ps1")) {
    throw "Conditional joins require one nullable type-domain rule, scoped branch flow and explicit owned merge inputs"
}
if (!$header.Contains("XL6_IR_INSTR_CLEANUP_BEGIN") -or
    !$header.Contains("XL6_IR_INSTR_CLEANUP_END") -or
    !$source.Contains("xl6IrTypeReleaseEffectDepth") -or
    !$lower.Contains("xl6IrLowerCallCleanupMayRaise") -or
    !(Read-Utf8 "test\cases\xlir_error_unwind_contract.c").Contains("release_effect_contract") -or
    !(Read-Utf8 "tools\check_xlir_error_unwind.ps1").Contains("normal_owned_result")) {
    throw "Normal call cleanup requires explicit release effects, pending owners and diagnostic-independent outcomes"
}
$cleanupEmitter = Read-Utf8 "src\cgen\xl6_ir_cgen_error_runtime.c"
if (!$cleanupEmitter.Contains("if (!cleanup.ok) xrtSetErrorTake(cleanup.error)") -or
    $cleanupEmitter.Contains("v%u = cleanup.ok; xrtSetErrorTake(cleanup.error)")) {
    throw "Successful normal cleanup must preserve the ambient diagnostic restored by ErrorScopeEnd"
}
foreach ($renderer in @('instruction_emit', 'async_instruction', 'async_contract',
                         'generator', 'verify_instructions', 'error_runtime')) {
    if (!(Read-Utf8 "src\cgen\xl6_ir_cgen_$renderer.c").Contains("XL6_IR_INSTR_CLEANUP_END")) {
        throw "Normal cleanup outcome is missing from a backend contract: $renderer"
    }
}
if (!$source.Contains("XLIR managed return has no adjacent pending-owner LOAD/MOVE commit") -or
    !$lower.Contains("xl6IrLowerPrepareReturnValue") -or
    !$lower.Contains("xl6IrLowerFinishDeferredReturn") -or
    !$lower.Contains("xl6IrLowerEmitParameterCleanup") -or
    !$source.Contains("xl6IrVerifyValueLifetimes") -or
    !(Read-Utf8 "src\ir\xl6_ir_lifetime.c").Contains("xl6IrVerifyStorageActivationFlow") -or
    (Read-Utf8 "tools\check_xlir_return_commit.ps1").Contains("xlir_return_cleanup_paths_contract.c") -or
    !(Read-Utf8 "tools\check_xlir_return_commit.ps1").Contains("xlir_return_commit_contract.c") -or
    !(Read-Utf8 "test\suites\03_core_function_abi.bat").Contains("tools\check_xlir_return_commit.ps1")) {
    throw "Managed returns must remain explicit pending owners until cleanup and terminal commit"
}
if (!$header.Contains("xl6IrValueOwnershipGet") -or !$source.Contains("xl6IrOwnedInput") -or
    !$source.Contains("xl6IrVerifyManagedStorageInputs") -or
    !$lower.Contains("xl6IrLowerNamePhysicalStorageValue(ctx, origin, sourceId, source)") -or
    $lower.Contains("xl6IrLowerValueIsOwnedXValueProjection") -or $source.Contains("xl6IrAsyncValueIsOwned") -or
    !(Read-Utf8 "tools\check_xlir_return_projection_owner.ps1").Contains("xlir_return_projection_owner_contract.c") -or
    !(Read-Utf8 "test\suites\03_core_function_abi.bat").Contains("tools\check_xlir_return_projection_owner.ps1")) {
    throw "Owned return/storage reads and continuation spills need one SSA producer contract and independently acquired product fields"
}
if (!$header.Contains("XL6_IR_EXIT_POINT_AFTER_SCOPE") -or
    !$header.Contains("XL6_IR_CLEANUP_END_STORAGE") -or
    !$source.Contains("xl6IrVerifyScopeExitActions") -or
    !$lower.Contains("xl6IrScopeExitPlanGet(ctx->module, blockId)") -or
    !(Read-Utf8 "test\suites\03_core_function_abi.bat").Contains("tools\check_xlir_scope_exit_plan.ps1")) {
    throw "Normal lexical exits must consume verified IR lifetime plans, including promoted scalar cells"
}
if (!$header.Contains("Result transport is transactional") -or
    !(Read-Utf8 "src\cgen\xl6_ir_cgen_callable_adapters.c").Contains("xl6IrCgenEmitCallableResultCleanup") -or
    !(Read-Utf8 "test\suites\03_core_function_abi.bat").Contains("tools\check_xlir_xcall_result_failure.ps1")) {
    throw "Runtime callable results require typed rollback and failure-injection coverage"
}
$generatorStorage=Read-Utf8 'src/runtime/xl6_generator.c'
$generatorEmitter=Read-Utf8 'src/cgen/xl6_ir_cgen_generator.c'
$generatorPrelude=Read-Utf8 'src/cgen/xl6_ir_cgen_emit_prelude.c'
if (!$source.Contains('XLIR generator storage ABI is unresolved') -or
    !$generatorPrelude.Contains('xl6GeneratorRuntimeV3Get()->Create') -or
    $generatorPrelude.Contains('struct xlir_generator {') -or
    $generatorEmitter -match 'generator->(?:done|frame|has_item|running|state)\s*=' -or
    $generatorEmitter.Contains('xlir_generator_frame_ops_') -or
    !$generatorEmitter.Contains('xlir_generator_frame_discard(frame)') -or
    $generatorEmitter -match 'xlir_generator_(frame|item)_(trace|free)_' -or
    !$header.Contains('XL6_IR_GENERATOR_STORAGE_RESIDENT_CLOSED_FRAME_V3') -or
    !$generatorStorage.Contains('xl6GeneratorOwnershipAdapterV1') -or
    !$generatorStorage.Contains('frame->owner->running') -or
    !$generatorStorage.Contains('xl6CodeLeaseAcquireScoped(owner,&lease)') -or
    !$generatorStorage.Contains('xl6CodeScopeLeave(&code)') -or
    !(Read-Utf8 'tools/run_xlir_ownership_batch.ps1').Contains('check_xlir_generator_storage.ps1')) {
    throw 'Generator control/frame/item lifetime requires a verified resident ABI, busy physical-frame refusal and real callback code tails'
}
$slotEmitter=Read-Utf8 'src/cgen/xl6_ir_cgen_slots.c'
$asyncStorage=Read-Utf8 'src/runtime/xl6_frame_storage.c'
$asyncFrameEmitter=Read-Utf8 'src/cgen/xl6_ir_cgen_async_machine.c'
$asyncInstructionEmitter=Read-Utf8 'src/cgen/xl6_ir_cgen_async_instruction.c'
if (!$header.Contains('XL6_IR_ASYNC_STORAGE_RESIDENT_CLOSED_FRAME_V3') -or
    !$source.Contains('XLIR async physical storage ABI is unresolved') -or
    !$lower.Contains('module->abi.asyncStorageAbi = XL6_IR_ASYNC_STORAGE_RESIDENT_CLOSED_FRAME_V3') -or
    !$generatorPrelude.Contains('xl6ContinuationFrameRuntimeV2Get()->Discard') -or
    !$asyncStorage.Contains('XL6_OWNERSHIP_RETURN(void*, xrtMalloc(size))') -or
    !$asyncStorage.Contains('XL6_OWNERSHIP_RETURN_VOID(xrtFree(data))') -or
    $asyncFrameEmitter.Contains('xrtFree(frame);') -or $asyncInstructionEmitter.Contains('xrtFree(next);') -or
    !$asyncFrameEmitter.Contains('xlir_async_frame_allocate(sizeof(*frame), &xlir_async_machine_storage_') -or
    !$asyncInstructionEmitter.Contains('xlir_async_frame_allocate(sizeof(*next), &xlir_async_machine_storage_') -or
    !$asyncFrameEmitter.Contains('XL6_ContinuationFrameLayoutV3') -or
    $asyncFrameEmitter.Contains('xlir_async_machine_frame_trace_') -or
    !(Read-Utf8 'src/runtime/xl6_continuation_frame.c').Contains('xl6ContinuationFrameOwnershipAdapterV1') -or
    !(Read-Utf8 'tools/run_xlir_ownership_batch.ps1').Contains('check_xlir_async_storage.ps1')) {
    throw 'Async machine/task physical storage requires explicit resident allocation/disposal without duplicate graph identities'
}
$slotRuntime=Read-Utf8 'src/runtime/xl6_slot.c'
if (!$header.Contains('XL6_IR_SLOT_TRANSFER_RESIDENT_ORDERED_WRITES_V1') -or
    !$source.Contains('XLIR physical slot transfer ABI is unresolved') -or
    !$slotEmitter.Contains('xl6SlotRuntimeV1Get()->Apply') -or
    !$slotEmitter.Contains('XL6_SlotWrite xlir_writes[]') -or
    !$slotRuntime.Contains('XL6_OWNERSHIP_RETURN_VOID(slot_apply_body(writes,count))') -or
    $slotRuntime -match 'xrt(?:Malloc|Calloc|ValueRelease|FutureDestroy)\(' -or
    !(Read-Utf8 'tools/run_xlir_ownership_batch.ps1').Contains('check_xlir_slot_transfers.ps1')) {
    throw 'Physical slot transfers require a verified ABI and one allocation/callback-free resident ordered commit'
}
foreach ($file in @('xl6_ir_cgen_instruction_emit.c','xl6_ir_cgen_async_instruction.c','xl6_ir_cgen_generator.c')) {
    $emitter=Read-Utf8 "src/cgen/$file"
    if (!$emitter.Contains('xl6IrCgenEmitStorageCommit') -or $emitter.Contains('frame->f%lu = v%u')) {
        throw "Storage writes must share verified payload/activation commit: $file"
    }
}
if (!$header.Contains("detaching and zeroing the storage BEFORE") -or
    !(Read-Utf8 "src\cgen\xl6_ir_cgen_release.c").Contains("xl6IrCgenEmitDetachedRelease") -or
    !(Read-Utf8 "test\suites\03_core_function_abi.bat").Contains("tools\check_xlir_cleanup_detach.ps1") -or
    !(Read-Utf8 "tools\check_xlir_foreign_error_scope.ps1").Contains("xlir_cleanup_error_scope_runtime.c")) {
    throw "Cleanup needs explicit detach-before-destroy ownership and nested boundary regression coverage"
}
if (!$header.Contains("XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE") -or
    !$header.Contains("XL6_IR_CLOSURE_RAW_CALLBACK_FOREIGN_SCOPE_THUNK") -or
    !$source.Contains("xl6IrClosureRawCallbackAbiHasEntry") -or
    !$lower.Contains("XL6_IR_CLOSURE_RAW_CALLBACK_FOREIGN_SCOPE_THUNK") -or
    !(Read-Utf8 "src\api\xl_foreign_error.c").Contains("xl6ForeignCallbackFail") -or
    !(Read-Utf8 "test\suites\03_core_function_abi.bat").Contains("tools\check_xlir_foreign_error_scope.ps1") -or
    !(Read-Utf8 "tools\check_xlir_foreign_error_scope.ps1").Contains("xlir_foreign_error_peer.dll")) {
    throw "Foreign callback errors require verified IR effects and shared-runtime cross-DLL coverage"
}
if (!(Read-Utf8 "test\cases\xlir_cleanup_error_scope_runtime.c").Contains("first_missing_matrix") -or
    !(Read-Utf8 "tools\check_xlir_foreign_error_scope.ps1").Contains("xlir_cleanup_error_scope_runtime.aot.exe")) {
    throw "Cleanup first-failure precedence needs allocation-fault and JIT/AOT regression lanes"
}
if (!$header.Contains("Detach and clear the source BEFORE allocation") -or
    !(Read-Utf8 "tools\check_xlir_native_take_reentry.ps1").Contains("xlir_native_take_reentry_smoke.c") -or
    !(Read-Utf8 "test\suites\03_core_function_abi.bat").Contains("tools\check_xlir_native_take_reentry.ps1")) {
    throw "Native handle TAKE must detach before allocation and preserve reentrant replacement owners"
}
if (!$header.Contains("XL6_IR_INSTR_FOREIGN_RESULT_ROLLBACK") -or
    !$lower.Contains("XL6_IR_INSTR_FOREIGN_RESULT_ROLLBACK") -or
    !$source.Contains("xl6IrForeignResultOwnershipMatches") -or
    !(Read-Utf8 "tools\check_xlir_foreign_result_rollback.ps1").Contains("xlir_foreign_result_rollback_contract.c") -or
    !(Read-Utf8 "test\suites\03_core_function_abi.bat").Contains("tools\check_xlir_foreign_result_rollback.ps1")) {
    throw "Owned foreign results require an exclusive verified rollback / handle-TAKE partition and fault tests"
}
if (!$header.Contains("XL6_IR_EXPORT_ERROR_SHARED_SCOPE_TRANSACTION") -or
    !$lower.Contains("exported->bodyErrorAbi = target->errorAbi") -or
    !$source.Contains("exported->bodyErrorAbi != target->errorAbi") -or
    !(Read-Utf8 "tools\check_xlir_export_transaction.ps1").Contains("xlir_export_transaction_contract.c") -or
    !(Read-Utf8 "test\suites\03_core_function_abi.bat").Contains("tools\check_xlir_export_transaction.ps1")) {
    throw "Export errors require a verified whole-entry transaction independent of diagnostic allocation"
}
if (!$header.Contains("XL6_IR_INSTR_ERROR_UNWIND_BEGIN") -or
    !$header.Contains("XL6_IR_INSTR_ERROR_UNWIND_END") -or
    !$lower.Contains("xl6IrLowerBeginErrorUnwind") -or
    !$source.Contains("xl6IrVerifyErrorUnwindRegions") -or
    !(Read-Utf8 "tools\check_xlir_error_unwind.ps1").Contains("xlir_error_unwind_contract.c") -or
    !(Read-Utf8 "test\suites\03_core_function_abi.bat").Contains("tools\check_xlir_error_unwind.ps1")) {
    throw "Exceptional cleanup must preserve first failure in explicit activation-local verified regions"
}
if (!$lower.Contains("xl6IrLowerCompleteCall") -or
    !$lower.Contains("XL6_IrLowerCallCleanup") -or
    !$source.Contains("XLIR call outcome must be tested before caller cleanup or other work") -or
    !(Read-Utf8 "test\cases\xlir_error_unwind_contract.c").Contains("immediate outcome mutations") -or
    !(Read-Utf8 "tools\check_xlir_error_unwind.ps1").Contains("unwind_constructor")) {
    throw "Call completion requires shared owner cleanup lowering and mandatory adjacent outcome verification"
}
if (!$lower.Contains("XLIR closure call argument has no exact materialized parameter TypeId") -or
    !(Read-Utf8 "test\suites\03_core_function_abi.bat").Contains("tools\check_xlir_closure_argument_conversion.ps1") -or
    !(Read-Utf8 "tools\check_xlir_closure_argument_conversion.ps1").Contains("sem_closure_argument_cint_variable_invalid.xl")) {
    throw "First-class arguments must materialize exact SSA types without broadening language conversions"
}
if (!$header.Contains("XL6_IR_EXPORT_VALUE_RAW_C_CALLBACK_BORROW") -or
    !$source.Contains("xl6IrExportParameterValueAbiForType") -or
    !$lower.Contains("xl6IrExportParameterValueAbiForType") -or
    !(Read-Utf8 "test\suites\03_core_function_abi.bat").Contains("tools\check_xlir_export_callback.ps1") -or
    !(Read-Utf8 "tools\check_xlir_export_callback.ps1").Contains("xlir_export_callback_contract.c")) {
    throw "Public C callbacks must retain canonical export ownership and verifier/runtime coverage"
}
if (!$header.Contains("XL6_IR_FUNCTION_MODULE_FINI") -or
    !$header.Contains("XL6_IR_INSTR_CLEANUP_INHERIT_BEGIN") -or
    !$header.Contains("XL6_IR_INSTR_CLEANUP_CAPTURE") -or
    !(Read-Utf8 "src\api\xl_foreign_error.c").Contains("xl6ErrorScopeBeginCleanup") -or
    !(Read-Utf8 "src\api\xl_api.c").Contains("xl_script_main_checked") -or
    !(Read-Utf8 "src\api\xl_api.c").Contains("xl_script_module_unit_checked") -or
    !(Read-Utf8 "tools\check_xlir_module_cleanup_outcome.ps1").Contains("xlir_module_cleanup_api.c") -or
    !(Read-Utf8 "test\suites\03_core_function_abi.bat").Contains("tools\check_xlir_module_cleanup_outcome.ps1") -or
    !$header.Contains("finalizerCleanupCount") -or
    !$source.Contains("xl6IrVerifyModuleFinalizer") -or
    !$lower.Contains("xl6IrLowerModuleFinalizer") -or
    !(Read-Utf8 "test\suites\03_core_base_10.bat").Contains("tools\check_xlir_module_finalization.ps1") -or
    !(Read-Utf8 "test\cases\xlir_module_init_cgen.c").Contains("rejectFinalizerMutation")) {
    throw "Module cleanup must remain verified IR with lifecycle and malformed-IR regression gates"
}
if (!$header.Contains("XL6_IR_FUNCTION_CLASS_FINI") -or
    !$header.Contains("XL6_IR_INSTR_CLEANUP_FAIL") -or
    !$source.Contains("xl6IrVerifyClassFinalizers") -or
    !$lower.Contains("xl6IrLowerClassFinalizers") -or
    !(Read-Utf8 "tools\check_xlir_class_cleanup_outcome.ps1").Contains("xlir_class_cleanup_contract.c") -or
    !(Read-Utf8 "test\suites\03_core_function_abi.bat").Contains("tools\check_xlir_class_cleanup_outcome.ps1")) {
    throw "Class destruction requires an ordered borrowed-receiver IR program with explicit failure joins and fault coverage"
}
$classAdapter = Read-Utf8 "src\cgen\xl6_ir_cgen_dispatch_persistence.c"
$classAdapter = $classAdapter.Substring($classAdapter.IndexOf('static int xl6IrCgenEmitClassDestroyHelpers'))
if ($classAdapter.Contains('classDestroySteps') -or $classAdapter.Contains('destroyStepCount') -or
    !$classAdapter.Contains('layout->destroyFunction') -or !$classAdapter.Contains('outcome.ok')) {
    throw "Cgen may only adapt the verified class-fini result, not reconstruct per-step destruction"
}
if (!$lower.Contains("xl6IrLowerManagedBinary") -or
    !$lower.Contains("xl6IrLowerAnchorEvaluationBorrow") -or
    !(Read-Utf8 "test\suites\03_core_base_10.bat").Contains("tools\check_xlir_binary_evaluation_lifetime.ps1") -or
    !(Read-Utf8 "test\cases\xlir_named_callable_owner_lifetime_host.c").Contains("XLIR_ALLOCATION_BINARY")) {
    throw "Managed binary transaction and physical allocation regression must stay wired"
}
if (!$lower.Contains("xl6IrLowerReleaseEvaluationBorrows") -or
    !(Read-Utf8 "test\suites\03_core_base_09.bat").Contains("tools\check_xlir_membership_evaluation_lifetime.ps1") -or
    !(Read-Utf8 "test\cases\xlir_named_callable_owner_lifetime_host.c").Contains("#define xrtStrConcat tracked_string_concat")) {
    throw "Membership source-order failure cleanup and string allocation regression must stay wired"
}
if (!$header.Contains("xl6IrRuntimeDirectBridgeTargetMatches") -or
    !$source.Contains("xl6IrRuntimeDirectBridgeTargetMatches") -or
    !$lower.Contains("xl6IrRuntimeDirectBridgeTargetMatches") -or
    !$directBridgeUnit.Contains("testDynamicFieldsBridgeAbi()") -or
    !$directBridge.Contains('nativeHandlePolicy="xl6DynamicFieldsHandlePolicyV1Get"') -or
    !$directBridge.Contains('"xl6_xrt_dynamic_fields_destroy"') -or
    $lower.Contains("xl6IrLowerResolveNativeFactoryCName")) {
    throw "DynamicFields factories and methods must share one verified physical adapter ABI"
}
if (!$header.Contains("xl6IrVerifyOwnedReadResults") -or
    !$source.Contains("xl6IrBuildOwnedReadResults") -or
    !$source.Contains("if ( !xl6IrVerifyOwnedReadResults(module, diags) ) return 0;") -or
    !$directBridgeUnit.Contains("testOwnedReadResults()") -or
    $source.Contains("xl6IrBuildOwnedXValueIndexResults")) {
    throw "Managed getter and collection read results must share a verified owned projection contract"
}
if (!$source.Contains("if ( node->type == XL6_IR_TYPE_INVALID ) node->type = signature;") -or
    !$source.Contains("if ( node->storageType == XL6_IR_TYPE_INVALID ) node->storageType = signature;") -or
    !$source.Contains("return xl6IrBuildOwnedReadResults(module);") -or
    !$directBridgeUnit.Contains("nodes[0].namedCallableAbi = XL6_IR_NAMED_CALLABLE_CAPTURE_FREE_ADAPTER;") -or
    !$directBridgeUnit.Contains("nodes[0].memberAbi = XL6_IR_MEMBER_ABI_CLASS_FIELD_CLOSURE;")) {
    throw "Named callable and copied member owners must be typed and independently verified"
}
if (!$header.Contains("XL6_IrDeclarationBinding") -or
    !$source.Contains("xl6IrDeclarationBindingAt") -or
    !$source.Contains("xl6IrVerifyScopeExitActions") -or
    !$lower.Contains("xl6IrScopeExitPlanGet(ctx->module, blockId)") -or
    !$directBridgeUnit.Contains("testDeclarationBindings()") -or
    !(Read-Utf8 "test\suites\03_core_base_05.bat").Contains("tools\check_xlir_evaluation_lifetime.ps1")) {
    throw "Declaration bindings and evaluation lifetime regression must remain shared and wired"
}
if (!$cgenUnit.Contains('#include "test/cases/xlir_direct_bridge_unit.c"') -or
    !$cgenUnit.Contains("!testDirectBridgeAbi()") -or
    !$directBridgeUnit.Contains("xl6IrRuntimeDirectBridgeTargetMatches") -or
    !$directBridgeUnit.Contains("target.resultPhysicalDestroySymbol = 7")) {
    throw "Direct bridge malformed-IR ABI unit must remain wired into the Cgen unit"
}
$sharedRuntime = Read-Utf8 "src\cgen\xl6_cgen_shared_runtime.c"
if (!$source.Contains('#include "src/ir/xl6_ir_direct_bridge.c"') -or
    !$lower.Contains("xl6IrRuntimeDirectBridgeTargetMatches") -or
    !$source.Contains("xl6IrRuntimeDirectBridgeTargetMatches")) {
    throw "Direct runtime bridges must share one registry between lowering and verification"
}
foreach ($required in @("xl6IrDirectBridgeContracts", "parameterPhysicalFieldDeclarations",
    "parameterConstraintAbis", "XL6_IR_PARAMETER_ABI_BORROW", "XL6_IR_PARAMETER_ABI_VALUE",
    "XL6_IR_CALL_RESULT_PHYSICAL_DIRECT", "XL6_IR_ERROR_ABI_NO_RAISE",
    "xl6_xrt_dir_list", "xl6_xrt_dir_glob", "xl6_xrt_dir_entries", "xl6_xrt_dir_is_empty",
    "xl6_xrt_dir_count", "xl6_xrt_dir_size", "xl6ProcessRunV1", "xl6ProcessResultDestroyV1",
    "xl6ProcessCommandShellV1")) {
    if (!$directBridge.Contains($required)) { throw "Direct runtime bridge registry lost ABI evidence: $required" }
}
foreach ($component in @("dir")) {
    if (!$sharedRuntime.Contains('#include "src/cgen/xl6_cgen_' + $component + '.c"')) {
        throw "Directory/process emitter must be a shared runtime component: $component"
    }
}
$xlangRuntime = Read-Utf8 "src\cgen\xl6_cgen_xlang_runtime.c"
if ($sharedRuntime.Contains("xl6_cgen_process_run") -or
    (Read-Utf8 "src/cgen/xl6_ir_cgen_needs_call_targets.c").Contains("PROCESS_EXEC")) {
    throw "Process implementations must stay out of Cgen"
}
$xlangStdlib = Read-Utf8 "src\stdlib\xl6_stdlib_data.c"
$xlangNeeds = Read-Utf8 "src\cgen\xl6_ir_cgen_needs_call_targets.c"
$xlangModuleEmit = Read-Utf8 "src\cgen\xl6_ir_cgen_module_emit.c"
if (!$header.Contains("XL6_IR_RUNTIME_BRIDGE_XLANG") -or
    !$source.Contains('#include "src/ir/xl6_ir_xlang_bridge.c"') -or
    !$lower.Contains("xl6IrRuntimeXlangBridgeTargetMatches") -or
    !$xlangNeeds.Contains("needsXlangRuntime = 1") -or
    !$xlangModuleEmit.Contains("xl6EmitXlangRuntimeComponent(out)") -or
    !$sharedRuntime.Contains('#include "src/cgen/xl6_cgen_xlang_runtime.c"')) {
    throw "Xlang compiler embedding must be selected from one verified XLIR bridge component"
}
foreach ($required in @(
    "XL6_IR_CALL_RESULT_PHYSICAL_NATIVE_CLASS_FIELD_TAKE",
    "XL6_IR_CALL_ERROR_PHYSICAL_XRT_THREAD_ERROR",
    "XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT",
    "XL6_IR_C_ABI_BOOL", "XL6_IR_C_ABI_C_INT",
    "XL6_IR_C_ABI_MANAGED_STRING", "XL6_IR_C_ABI_RUNTIME_XVALUE_PTR")) {
    if (!$xlangBridge.Contains($required)) {
        throw "Xlang compiler bridge lost exact physical ABI evidence: $required"
    }
}
foreach ($symbol in @(
    "xl6_xlang_context_create", "xl6_xlang_context_destroy",
    "xl6_xlang_context_last_error_copy", "xl6_xlang_context_set_work_dir",
    "xl6_xlang_context_set_temp_dir", "xl6_xlang_context_set_save_temps",
    "xl6_xlang_context_run_source", "xl6_xlang_context_run_file",
    "xl6_xlang_context_run_source_args", "xl6_xlang_context_run_file_args",
    "xl6_xlang_context_clear_last_error", "xl6_xlang_handler_wrap",
    "xl6_xlang_handler_create", "xl6_xlang_handler_destroy",
    "xl6_xlang_handler_run_args", "xl6_xlang_handler_last_error_copy",
    "xl6_xlang_run_source", "xl6_xlang_run_file")) {
    if (!$xlangBridge.Contains('"' + $symbol + '"') -or
        !$xlangRuntime.Contains($symbol)) {
        throw "Xlang compiler bridge registry/emitter is incomplete: $symbol"
    }
}
if (!$xlangRuntime.Contains("#include <__xl_compiler/include/xl.h>") -or
    !$xlangRuntime.Contains("#include <xl.h>") -or
    !$xlangRuntime.Contains("XL_Context*") -or
    !$xlangRuntime.Contains("XL_Module*") -or
    !$xlangRuntime.Contains("bool xl6_xlang_handler_run_args") -or
    !$xlangRuntime.Contains("void xl6_xlang_handler_destroy") -or
    $xlangRuntime.Contains("xlirAbi") -or
    $xlangRuntime.Contains("long long xl6_xlang_handler_destroy") -or
    $xlangRuntime.Contains("typedef struct xvalue xvalue;") -or
    $xlangRuntime.Contains("void* xlCreateContext")) {
    throw "Generated xlang embedding must consume the canonical public API types without handwritten prototypes"
}
foreach ($legacySymbol in @(
    'extern \"xlSetWorkDir\"', 'extern \"xlSetTempDir\"',
    'extern \"xlSetSaveTemps\"', 'extern \"xlRunScriptString\"',
    'extern \"xlRunScriptFile\"', 'extern \"xlRunScriptStringArg\"',
    'extern \"xlRunScriptFileArg\"', 'extern \"xlClearLastError\"')) {
    if ($xlangStdlib.Contains($legacySymbol)) {
        throw "Xlang standard-library surface bypasses its exact compiler-private ABI: $legacySymbol"
    }
}
$xlangDeterminism = Read-Utf8 "tools\check_xlir_cgen_determinism.ps1"
foreach ($fixture in @("bad_xlir_xlang_bridge_input.xl", "bad_xlir_xlang_bridge_result.xl")) {
    if (!$xlangDeterminism.Contains($fixture)) {
        throw "Malformed xlang compiler bridge is not fail-closed in regression coverage: $fixture"
    }
}
$sharedCollection = Read-Utf8 "src\cgen\xl6_ir_cgen_shared_collection.c"
$sharedCompare = Read-Utf8 "src\cgen\xl6_ir_cgen_shared_compare.c"
$sharedDispatch = Read-Utf8 "src\cgen\xl6_ir_cgen_sync_emit.c"
$moduleEmit = Read-Utf8 "src\cgen\xl6_ir_cgen_module_emit.c"
$core04 = Read-Utf8 "test\suites\03_core_base_04.bat"
if (!$source.Contains("xl6IrDynamicCompareRuntimeInstructionCompatible") -or
    !$header.Contains("xl6IrDynamicCompareRuntimeInstructionCompatible") -or
    !$sharedCompare.Contains("xl6IrDynamicCompareRuntimeInstructionCompatible") -or
    !$sharedDispatch.Contains("xl6IrCgenEmitDynamicCompare") -or
    !$directBridgeUnit.Contains("testDynamicCompareAbi()") -or
    !$core04.Contains("tools\check_xlir_dynamic_compare_contract.ps1") -or
    !$core04.Contains("tools\check_xlir_core_expression_contract.ps1")) {
    throw "Dynamic comparison must retain one verified physical contract and core runtime gates"
}
foreach ($path in @("src\cgen\xl6_ir_cgen_runtime_scalar_instruction.c",
    "src\cgen\xl6_ir_cgen_async_instruction.c")) {
    if ((Read-Utf8 $path) -match '\bxlir_xvalue_(equal|order|approx)\(') {
        throw "Dynamic comparison returned to an execution-specific emitter: $path"
    }
    if ((Read-Utf8 $path) -match 'strtod') {
        throw "Special f64 constants returned to execution-specific CRT parsing: $path"
    }
}
if ($moduleEmit.IndexOf("xl6IrCgenAnalyzeRuntimeNeeds") -gt
    $moduleEmit.IndexOf("xl6IrCgenNeedsXrtDeclarations")) {
    throw "Runtime feature discovery must finish before selecting public declarations"
}
$dynamicRuntime = Read-Utf8 "src\cgen\xl6_ir_cgen_emit_xvalue_ops.c"
if ($dynamicRuntime -match 'typedef struct xrtcall(frame|result)' -or
    !$dynamicRuntime.Contains("typedef xstrview xlir_xcall_name_view;") -or
    !(Read-Utf8 "src\cgen\xl6_ir_cgen_needs_model.c").Contains("needs->needsXCallRuntime ||")) {
    throw "Dynamic callable frames must use the public declaration ABI, not mirrored layouts"
}
if (!(Read-Utf8 "src\cgen\xl6_ir_cgen_emit_xvalue_core.c").Contains("typedef xbytesview xlir_bytes_view;") -or
    (Read-Utf8 "src\cgen\xl6_ir_cgen_emit_template.c").Contains("typedef struct xtemplateconfig")) {
    throw "Union bytes and template configuration must use public declaration types"
}
if (!(Read-Utf8 "src\cgen\xl6_ir_cgen_emit_xvalue_core.c").Contains("XVALUE_MERGE_REPLACE") -or
    (Read-Utf8 "src\cgen\xl6_ir_cgen_emit_xvalue_core.c") -match
        'extern bool xrtValue(?:IntMap|Object)Merge') {
    throw "Collection merge policy must come from the public enum ABI"
}
$sharedValue = Read-Utf8 "src\cgen\xl6_ir_cgen_shared_value.c"
if (!$source.Contains("xl6IrCollectionReadRuntimeInstructionCompatible") -or
    !$sharedCollection.Contains("xl6IrCollectionReadRuntimeInstructionCompatible") -or
    !$sharedCollection.Contains("XL6_IR_RUNTIME_XVALUE_ARRAY_AT_I64") -or
    !$sharedCollection.Contains("XL6_IR_RUNTIME_XVALUE_COUNT") -or
    !$sharedValue.Contains("case XL6_IR_RUNTIME_XVALUE_RETAIN:") -or
    !$directBridgeUnit.Contains("testCollectionReadAbi()")) {
    throw "Collection reads and same-TypeId retain must use verified shared continuation emission"
}
$cEmitter = Read-Utf8 "src\cgen\xl6_ir_cgen.c"
foreach ( $include in [regex]::Matches(
    $cEmitter, '#include "(?<path>src/cgen/xl6_ir_cgen[^\"]+\.c)"') ) {
    $cEmitter += "`n" + (Read-Utf8 ($include.Groups["path"].Value -replace '/', '\'))
}
$compileUnit = Read-Utf8 "src\compile\xl6_compile_unit.c"
$api = Read-Utf8 "src\api\xl_api.c"
$shell = Read-Utf8 "src\driver\xl6_xlc_shell.c"
$publicApi = Read-Utf8 "release\include\xlc.h"
$sem = Read-Utf8 "src\sem\xl6_sem.c"
$determinism = Read-Utf8 "tools\check_xlir_cgen_determinism.ps1"
$runtimeMatrix = Read-Utf8 "tools\check_xlir_cgen_runtime_matrix.ps1"
$defaultClassGate = Read-Utf8 "tools\check_xlir_default_class_layout.ps1"
$instanceClassMethodGate = Read-Utf8 "tools\check_xlir_class_instance_method.ps1"
$implicitClassReceiverGate = Read-Utf8 "tools\check_xlir_class_implicit_receiver.ps1"
$explicitConstructorFieldGate = Read-Utf8 "tools\check_xlir_explicit_constructor_field_initializer.ps1"
$classReferenceFieldGate = Read-Utf8 "tools\check_xlir_class_reference_field_initializer.ps1"
$persistenceJsonGate = Read-Utf8 "tools\check_xlir_persistence_json_abi.ps1"
$taskSpawnClosureGate = Read-Utf8 "tools\check_xlir_task_spawn_closure_abi.ps1"
$taskNestedFutureGate = Read-Utf8 "tools\check_xlir_task_nested_future_abi.ps1"
$threadResourceGate = Read-Utf8 "tools\check_xlir_thread_resource_abi.ps1"
$threadPublicGate = Read-Utf8 "tools\check_xlir_runtime_operand_catalog.py"
$threadLanguageGate = Read-Utf8 "tools\check_stdlib_thread_language.py"
$channelResourceGate = Read-Utf8 "tools\check_xlir_channel_resource_abi.ps1"
$channelOwnedGate = Read-Utf8 "tools\check_stdlib_channel_regressions.py"
$channelReceivePlan = Read-Utf8 "src\ir\xl6_ir_channel_receive.c"
$channelNativeEmitter = Read-Utf8 "src\cgen\xl6_ir_cgen_channel_emit.c"
$syncResourceGate = Read-Utf8 "tools\check_xlir_sync_resource_abi.ps1"
$specialQueueResourceGate = Read-Utf8 "tools\check_xlir_special_queue_resource_abi.ps1"
$stackResourceGate = Read-Utf8 "tools\check_xlir_stack_resource_abi.ps1"
$avlResourceGate = Read-Utf8 "tools\check_xlir_avl_resource_abi.ps1"
$futureContinuationGate = Read-Utf8 "tools\check_xlir_future_continuation_abi.ps1"
$staticClassGate = Read-Utf8 "tools\check_xlir_class_static_storage.ps1"
$callSiteBindingGate = Read-Utf8 "tools\check_xlir_callsite_binding.ps1"
$callSiteVariadicGate = Read-Utf8 "tools\check_xlir_callsite_variadic.ps1"
$callSiteCVarargGate = Read-Utf8 "tools\check_xlir_callsite_c_vararg.ps1"
$callSiteGenericGate = Read-Utf8 "tools\check_xlir_callsite_generic_specialization.ps1"
$classProtocolSyntaxGate = Read-Utf8 "tools\check_xlir_class_protocol_syntax.ps1"
$intrinsicCallGate = Read-Utf8 "tools\check_xlir_intrinsic_calls.ps1"
$integerConversionAbiGate = Read-Utf8 "tools\check_xlir_integer_conversion_abi.ps1"
$operatorExecutionTypeGate = Read-Utf8 "tools\check_xlir_operator_execution_type.ps1"
$weakAbiGate = Read-Utf8 "tools\check_xlir_weak_abi.ps1"
$generatorMachineGate = Read-Utf8 "tools\check_xlir_generator_machine.ps1"
$checkedCastAbiGate = Read-Utf8 "tools\check_xlir_checked_cast_abi.ps1"
$nullableCastAbiGate = Read-Utf8 "tools\check_xlir_nullable_cast_abi.ps1"
$nullableCoalesceAbiGate = Read-Utf8 "tools\check_xlir_nullable_coalesce_abi.ps1"
$nullableFunctionValueAbiGate = Read-Utf8 "tools\check_xlir_nullable_function_value_abi.ps1"
$callableFieldFrameAbiGate = Read-Utf8 "tools\check_xlir_callable_field_frame_abi.ps1"
$internalPropertyCallAbiGate = Read-Utf8 "tools\check_xlir_internal_property_call_abi.ps1"
$borrowedCallProvenanceGate = Read-Utf8 "tools\check_xlir_borrowed_call_provenance.ps1"
$nominalCastAbiGate = Read-Utf8 "tools\check_xlir_nominal_cast_abi.ps1"
$enumAbiGate = Read-Utf8 "tools\check_xlir_enum_abi.ps1"
$scalarConversionAbiGate = Read-Utf8 "tools\check_xlir_scalar_conversion_abi.ps1"
$timeConversionAbiGate = Read-Utf8 "tools\check_xlir_time_conversion_abi.ps1"
$nativeNominalReceiverGate = Read-Utf8 "tools\check_xlir_native_nominal_receiver.ps1"
$nativeHandleBindGate = Read-Utf8 "tools\check_xlir_native_handle_bind.ps1"
$valueInitializerGate = Read-Utf8 "tools\check_xlir_value_initializer.ps1"
$reachabilityGate = Read-Utf8 "tools\check_xlir_reachability.ps1"
$productResultGate = Read-Utf8 "tools\check_xlir_product_result.ps1"
$namedFunctionAdapterGate = Read-Utf8 "tools\check_xlir_named_function_adapter.ps1"
$openFunctionGate = Read-Utf8 "tools\check_xlir_open_function_abi.ps1"
$typedCallableContainerGate = Read-Utf8 "tools\check_xlir_typed_callable_container_abi.ps1"
$typedSafeIndexNominalGate = Read-Utf8 "tools\check_xlir_typed_container_safe_index_objects_functions_abi.ps1"
$typedIndexAssignNominalGate = Read-Utf8 "tools\check_xlir_typed_container_index_assign_objects_functions_abi.ps1"
$dynamicKeyScalarProjectionGate = Read-Utf8 "tools\check_xlir_dynamic_key_scalar_projection.ps1"
$typedPointProjectionGate = Read-Utf8 "tools\check_xlir_typed_point_projection_abi.ps1"
$xvalueScalarBridgeGate = Read-Utf8 "tools\check_xlir_xvalue_scalar_bridge_abi.ps1"
$collectionMemberGate = Read-Utf8 "tools\check_xlir_collection_member_intrinsics.ps1"
$collectionAlgebraGate = Read-Utf8 "tools\check_xlir_collection_algebra.ps1"
$dynamicCollectionMethodGate = Read-Utf8 "tools\check_xlir_dynamic_collection_methods.ps1"
$typeValueAbiGate = Read-Utf8 "tools\check_xlir_type_value_abi.ps1"
$stringConversionAbiGate = Read-Utf8 "tools\check_xlir_string_conversion_abi.ps1"
$classValueIdentityAbiGate = Read-Utf8 "tools\check_xlir_class_value_identity_abi.ps1"
$nativeClassObjectIdentityAbiGate = Read-Utf8 "tools\check_xlir_native_class_object_identity_abi.ps1"
$managedNativeExternReceiverGate = Read-Utf8 "tools\check_xlir_managed_native_extern_receiver_abi.ps1"
$classLifecycleGate = Read-Utf8 "tools\check_xlir_class_lifecycle_abi.ps1"
$xcallGate = Read-Utf8 "tools\check_xlir_xcall.ps1"
$foreignFunctionValueGate = Read-Utf8 "tools\check_xlir_foreign_function_value_abi.ps1"
$asyncGenericPlannerGate = Read-Utf8 "tools\check_xlir_async_generic_planner.ps1"
$asyncCfgLivenessGate = Read-Utf8 "tools\check_xlir_async_cfg_liveness.ps1"
$asyncStorageAddressGate = Read-Utf8 "tools\check_xlir_async_storage_address.ps1"
$asyncMultiAwaitMachineGate = Read-Utf8 "tools\check_xlir_async_machine_multi_await.ps1"
$asyncMachineAdmissionGate = Read-Utf8 "tools\check_xlir_async_machine_admission.ps1"
$indexWriteAbiGate = Read-Utf8 "tools\check_xlir_index_write_abi.ps1"
$memberAbiGate = Read-Utf8 "tools\check_xlir_member_abi.ps1"
$lengthProtocolMemberGate = Read-Utf8 "tools\check_xlir_length_protocol_member.ps1"
$classFieldMemberAbiGate = Read-Utf8 "tools\check_xlir_class_field_member_abi.ps1"
$rangeForAbiGate = Read-Utf8 "tools\check_xlir_range_for_abi.ps1"
$arrayI64ForAbiGate = Read-Utf8 "tools\check_xlir_array_i64_for_abi.ps1"
$collectionContinuationGate = Read-Utf8 "tools\check_stdlib_collection_continuations.py"
$xvalueIteratorAbiGate = Read-Utf8 "tools\check_xlir_xvalue_iterator_abi.ps1"
$setI64IteratorAbiGate = Read-Utf8 "tools\check_xlir_set_i64_iterator_abi.ps1"
$iteratorScalarProjectionGate = Read-Utf8 "tools\check_xlir_xvalue_iterator_scalar_projection.ps1"
$iteratorManagedProjectionGate = Read-Utf8 "tools\check_xlir_xvalue_iterator_managed_projection.ps1"
$arrayIteratorProjectionGate = Read-Utf8 "tools\check_xlir_xvalue_array_iterator_projection.ps1"
$dictIteratorProjectionGate = Read-Utf8 "tools\check_xlir_dict_iterator_projection.ps1"
$erasedCollectionIteratorGate = Read-Utf8 "tools\check_xlir_erased_collection_iterator_abi.ps1"
$dynamicVarIteratorGate = Read-Utf8 "tools\check_xlir_dynamic_var_iterator_abi.ps1"
$stringIteratorGate = Read-Utf8 "tools\check_xlir_string_iterator_abi.ps1"
$charBytesAbiGate = Read-Utf8 "tools\check_xlir_char_bytes_abi.ps1"
$protocolIteratorGate = Read-Utf8 "tools\check_xlir_protocol_iterator_abi.ps1"
$protocolDispatchModelGate = Read-Utf8 "tools\check_xlir_protocol_dispatch_model.ps1"
$protocolWitnessAdapterGate = Read-Utf8 "tools\check_xlir_protocol_witness_adapter_abi.ps1"
$collectionTypeRebindGate = Read-Utf8 "tools\check_xlir_collection_type_rebind.ps1"
$nullableContainerProjectionGate = Read-Utf8 "tools\check_xlir_nullable_container_projection.ps1"
$languageFrontendSuite = Read-Utf8 "test\suites\02_language_frontend.bat"
$generatedRuntime = Read-Utf8 "tools\xrt_single_header_impl.c"

# Source `as` is a closed IR conversion family.  Dynamic non-null casts,
# nullable try-casts, representation-explicit lifts and checked unwraps retain
# separate identities/effects through Typed XLIR, CFG lowering and mechanical C.
foreach ( $required in @(
    "XL6_IrCastAbi", "XL6_IR_RUNTIME_XVALUE_CAST_CHECKED",
    "XL6_IR_RUNTIME_XVALUE_CAST_NULLABLE_TAGGED",
    "XL6_IR_RUNTIME_XVALUE_CAST_NULLABLE_STRING",
    "XL6_IR_CAST_XVALUE_TO_U64_CHECKED",
    "XL6_IR_CAST_XVALUE_TO_NULLABLE_TIME_TRY",
    "XL6_IR_CAST_VALUE_TO_TAGGED_NULLABLE",
    "XL6_IR_CAST_MANAGED_TO_POINTER_NULLABLE",
    "XL6_IR_CAST_TAGGED_NULLABLE_TO_VALUE_CHECKED",
    "XL6_IR_CAST_NULLABLE_POINTER_TO_MANAGED_CHECKED",
    "XL6_IR_CAST_XVALUE_TO_NOMINAL_CHECKED",
    "XL6_IR_CAST_XVALUE_TO_NULLABLE_NOMINAL_TRY",
    "XL6_IR_CAST_NULLABLE_NOMINAL_TO_NOMINAL_CHECKED",
    "XL6_IR_RUNTIME_XVALUE_NOMINAL_TEST",
    "XL6_IrNominalIdentitySet",
    "castAbi", "castErrorAbi"
) ) {
    if ( ! $header.Contains($required) ) {
        throw "XLIR header is missing the closed Cast ABI: $required"
    }
}
foreach ( $required in @(
    "xl6IrClassifyCastAbi", "xl6IrClassifyCastErrorAbi",
    "xvalue-to-nullable-u64-try", "xvalue-to-nullable-time-try",
    "managed-to-pointer-nullable", "nullable-pointer-to-managed-checked",
    "XLIR nullable xvalue try-cast has no exact source",
    "XLIR instruction has an ambiguous closed cast ABI",
    "XLIR nullable string try-cast is not immediately followed by ERROR_TEST",
    "xl6IrRefreshNominalIdentitySets", "xl6IrVerifyNominalIdentitySets",
    "xvalue-nominal-cast-checked", "nominal identity-set arena is malformed"
) ) {
    if ( ! $source.Contains($required) ) {
        throw "XLIR verifier/dump is missing the closed Cast ABI: $required"
    }
}
foreach ( $required in @(
    "xl6IrLowerCheckedXValueCast", "xl6IrLowerDynamicNullableCast",
    "xl6IrLowerNullableLiftCast", "xl6IrLowerNullableUnwrapCast",
    "xl6IrLowerCast", "XL6_IR_INSTR_NULLABLE_WRAP",
    "XL6_IR_INSTR_NULLABLE_UNWRAP", 'CanonicalCStringBytes("TypeError")',
    "xl6IrLowerNominalCast", "xl6IrLowerNominalNullableUnwrapCast",
    "XL6_IR_RUNTIME_XVALUE_NOMINAL_TEST"
) ) {
    if ( ! $lower.Contains($required) ) {
        throw "XLIR lowering is missing closed Cast ABI consumption: $required"
    }
}
foreach ( $required in @(
    "xlir_xvalue_cast_checked", "xlir_xvalue_try_cast_",
    "xlir_xvalue_try_cast_nullable_string",
    "xlir_xvalue_type_id(item) != exactTypeId",
    "nullable string cast result allocation failed",
    "xl6IrCgenEmitNominalIdentityRuntime", "xlir_nominal_set_",
    "static const XL6_NominalAdmissionV1", "xl6ValueAdmitsNominalV1(",
    "__xl_runtime/xl6_nominal_admission_abi.h", "XL6_IR_RUNTIME_XVALUE_NOMINAL_CAST_NULLABLE"
) ) {
    if ( ! $cEmitter.Contains($required) ) {
        throw "XLIR C emitter is missing mechanical closed Cast ABI emission: $required"
    }
}
foreach ($retired in @('static bool xlir_nominal_set_', 'xlir_nominal_runtime_tag')) {
    if ($cEmitter.Contains($retired)) { throw "Retired generated nominal matcher remains: $retired" }
}
foreach ($required in @('collectionAdmissionAbi','collectionAdmissionIdentitySet','collectionAdmissionNullable',
    'XL6_IR_COLLECTION_ADMISSION_PRIMITIVE','XL6_IR_COLLECTION_ADMISSION_NOMINAL')) {
    if (!$header.Contains($required)) { throw "Missing closed collection element admission: $required" }
}
foreach ($required in @('xl6IrCollectionAdmissionForPayload','XLIR element admission is attached to an unrelated instruction')) {
    if (!$source.Contains($required)) { throw "Missing independent collection admission verifier: $required" }
}
foreach ( $required in @(
    "xlir_checked_cast_abi.xl", "xvalue-cast-checked", "adjacent ERROR_TEST",
    "ReadAllBytes", "-Werror"
) ) {
    if ( ! $checkedCastAbiGate.Contains($required) ) {
        throw "XLIR checked-cast quality gate is incomplete: $required"
    }
}
foreach ( $required in @(
    "xlir_nullable_cast_abi.xl", "xvalue-cast-nullable-tagged",
    "xvalue-cast-nullable-string", "mismatch-null semantics",
    "explicit TypeError CFG", "owned string transfer", "ReadAllBytes", "-Werror"
) ) {
    if ( ! $nullableCastAbiGate.Contains($required) ) {
        throw "XLIR nullable-cast quality gate is incomplete: $required"
    }
}
foreach ( $required in @(
    "nullable_expression_semantics.xl", "nested nullable-coalesce pair",
    "explicit unwrap-then-wrap CFG", "fallback evaluation is lazy",
    "byte-for-byte deterministic", "strict-C/runtime gate failed",
    "strict-C/runtime gate failed"
) ) {
    if ( ! $nullableCoalesceAbiGate.Contains($required) ) {
        throw "XLIR nullable-coalesce quality gate is incomplete: $required"
    }
}
foreach ( $required in @(
    "nullable_function_frames", "nullable_function_lifecycle",
    "pointer-sentinel nullable closure ABI", "explicit NULLABLE_WRAP boundary",
    "no-raise null-sentinel ABI", "spurious error edge",
    "callable signature TypeId", "byte-for-byte deterministic",
    "strict-C/runtime gate failed"
) ) {
    if ( ! $nullableFunctionValueAbiGate.Contains($required) ) {
        throw "XLIR nullable function-value quality gate is incomplete: $required"
    }
}
foreach ( $required in @(
    "cgen_member_function_class_return",
    "cgen_member_function_multi_return",
    "cgen_typed_container_for_in_returned",
    "void, managed-string or nominal-xvalue slots",
    "nested-closure multi-result field frame",
    "adjacent temporary STORE/RELEASE sink",
    "byte-for-byte deterministic",
    "strict-C/runtime gate failed",
    "strict-C/runtime gate failed"
) ) {
    if ( ! $callableFieldFrameAbiGate.Contains($required) ) {
        throw "XLIR callable-field frame quality gate is incomplete: $required"
    }
}
foreach ( $required in @(
    "xlir_internal_property_call_abi",
    "xlir_internal_property_error_propagation",
    "Container-property/indexed-property origin partition",
    "frontend-owned throw cleanup plan",
    "exact may-raise result carriers",
    "adjacent cleanup-complete ERROR_TESTs",
    "byte-for-byte deterministic",
    "strict-C/runtime gate failed",
    "strict-C/runtime gate failed"
) ) {
    if ( ! $internalPropertyCallAbiGate.Contains($required) ) {
        throw "XLIR internal property CallTarget quality gate is incomplete: $required"
    }
}
foreach ( $required in @(
    "xl6IrBorrowedCallArgumentIsExplicit",
    "xl6IrValueOwnershipGet"
) ) {
    if ( ! $source.Contains($required) ) {
        throw "XLIR verifier is missing transparent borrowed-call provenance: $required"
    }
}
$loanSource = Read-Utf8 "src\ir\xl6_ir_lifetime.c"
foreach ($required in @('xl6IrLifetimeSourceForValue', 'XL6_IR_INSTR_VALUE',
                         'XL6_IR_INSTR_NULLABLE_WRAP', 'XL6_IR_INSTR_NULLABLE_UNWRAP',
                         'depth > module->instructionCount', 'xl6IrVerifyStorageActivationFlow')) {
    if (!$loanSource.Contains($required)) {
        throw "XLIR loan verifier is missing finite transparent provenance or storage activation: $required"
    }
}
foreach ( $required in @(
    "xl6IrLowerScalarCompoundValue",
    "xl6IrLowerElementCompoundAssign",
    "one typed read-modify-write operation",
    "xl6IrLowerPrepareElementTargetPlan",
    "xl6IrLowerClassFieldProjectValue"
) ) {
    if ( ! $lower.Contains($required) ) {
        throw "XLIR lowering is missing canonical class-field read-modify-write ABI: $required"
    }
}
foreach ( $required in @(
    "xlir_borrowed_call_provenance.xl",
    "LOAD -> NULLABLE_WRAP -> CALL",
    "NULLABLE_UNWRAP -> CLOSURE_CALL",
    "receiver-anchor/GET/BINARY/SET transaction",
    "Class-field receiver lifetime",
    "extlib_hash_progress",
    "byte-for-byte deterministic",
    "strict-C/runtime gate failed",
    "strict-C/runtime gate failed"
) ) {
    if ( ! $borrowedCallProvenanceGate.Contains($required) ) {
        throw "XLIR borrowed-call provenance quality gate is incomplete: $required"
    }
}
foreach ( $required in @(
    "xlir_nominal_cast_abi.xl", "nominalIdentitySets",
    "xvalue-nominal-test", "inheritance/witness identity sets",
    "explicit TypeError CFG", "ReadAllBytes", "-Werror"
) ) {
    if ( ! $nominalCastAbiGate.Contains($required) ) {
        throw "XLIR nominal-cast quality gate is incomplete: $required"
    }
}
foreach ( $gate in @(
    "tools\check_xlir_checked_cast_abi.ps1",
    "tools\check_xlir_nullable_cast_abi.ps1",
    "tools\check_xlir_nullable_coalesce_abi.ps1",
    "tools\check_xlir_nullable_function_value_abi.ps1",
    "tools\check_xlir_callable_field_frame_abi.ps1",
    "tools\check_xlir_internal_property_call_abi.ps1",
    "tools\check_xlir_borrowed_call_provenance.ps1",
    "tools\check_xlir_nominal_cast_abi.ps1"
) ) {
    if ( ! $languageFrontendSuite.Contains($gate) ) {
        throw "XLIR closed Cast ABI quality gate is not wired into the language frontend suite: $gate"
    }
}

# Enums are a closed nominal XLIR layout family. Numeric enums retain an exact
# scalar width plus TypeValueId on erasure; tagged enums retain exact variant
# and payload arena rows through lowering so Cgen never resolves source syntax.
foreach ( $required in @(
    "XL6_IrEnumLayout", "XL6_IrEnumVariant", "XL6_IrEnumPayload",
    "XL6_IrEnumPayloadAbi", "enumVariant", "enumPayload",
    "XL6_IR_TYPE_NUMERIC_ENUM", "XL6_IR_TYPE_TAGGED_ENUM",
    "XL6_IR_RUNTIME_TAGGED_ENUM_NEW",
    "XL6_IR_RUNTIME_TAGGED_ENUM_PAYLOAD_SET_NEW",
    "XL6_IR_RUNTIME_TAGGED_ENUM_TAG_EQUAL",
    "XL6_IR_RUNTIME_TAGGED_ENUM_PAYLOAD_PROJECT"
) ) {
    if ( ! $header.Contains($required) ) {
        throw "XLIR header is missing the closed enum ABI: $required"
    }
}
foreach ( $required in @(
    "xl6IrBuildEnumLayouts", "xl6IrVerifyEnumLayouts",
    "xl6IrEnumLayoutGet", "xl6IrEnumVariantGet",
    '\"enumLayouts\": [', '\"enumVariants\": [', '\"enumPayloads\": [',
    '\"enumVariant\":%ld', '\"enumPayload\":%ld'
) ) {
    if ( ! $source.Contains($required) ) {
        throw "XLIR verifier/dump is missing the closed enum ABI: $required"
    }
}
foreach ( $required in @(
    "xl6IrLowerNumericEnumVariantMember",
    "xl6IrLowerTaggedEnumVariantCall", "xl6IrLowerTaggedEnumMatch",
    "XLIR tagged enum match is not exhaustive for its EnumLayout",
    "XL6_IR_RUNTIME_TAGGED_ENUM_PAYLOAD_PROJECT",
    "xl6IrLowerBindRuntimeTypeIdentity"
) ) {
    if ( ! $lower.Contains($required) ) {
        throw "XLIR lowering is missing closed enum ABI consumption: $required"
    }
}
foreach ( $required in @(
    '$xlir.enum.variant', '$xlir.enum.payload.%u',
    "instruction->enumVariant", "instruction->enumPayload",
    "xl6IrCgenTaggedEnumRuntimeAbi"
) ) {
    if ( ! $cEmitter.Contains($required) ) {
        throw "XLIR C emitter is missing mechanical enum ABI emission: $required"
    }
}
  foreach ( $required in @(
    "enum_match_matrix.xl", "enum_numeric_runtime.xl",
    "cgen_generic_enum_match_top_level.xl", "enum_nested_generic.xl",
    "enum_recursive_container.xl", "enum_tagged_lifecycle.xl",
    "enum_async_closure.xl", "enumLayouts",
    "tagged-enum-payload-project", "xvalue-type-bind",
    "nominal type identity", "release-xvalue",
    "canonicalize repeated integer and generic enum identities", "-Werror"
  ) ) {
    if ( ! $enumAbiGate.Contains($required) ) {
        throw "XLIR enum ABI quality gate is incomplete: $required"
    }
}
if ( ! $languageFrontendSuite.Contains(
         "tools\check_xlir_enum_abi.ps1") ) {
    throw "XLIR enum ABI quality gate is not wired into the language frontend suite"
}

# Every source call is normalized once into a typed CallSite.  Fixed/default/
# named bindings, erased ARRAY/DICT tails and native C ellipsis tails are
# separate verified ABIs; lowering consumes them and Cgen sees only final CALL
# operands plus the already-resolved CallTarget variadic convention.
foreach ( $required in @(
    "XL6_IrCallArgumentSource", "XL6_IrCallVariadicAbi",
    "XL6_IrCallArgument", "XL6_IrCallSite",
    "XL6_IR_CALL_ARGUMENT_DEFAULT", "XL6_IR_CALL_ARGUMENT_ARRAY_VARARG_ITEM",
    "XL6_IR_CALL_ARGUMENT_DICT_VARARG_ITEM", "XL6_IR_CALL_ARGUMENT_C_VARARG_ITEM",
    "XL6_IR_CALL_ARGUMENT_IMPLICIT_STRING",
    "XL6_IR_CALL_VARIADIC_ARRAY", "XL6_IR_CALL_VARIADIC_DICT", "XL6_IR_CALL_VARIADIC_C",
    "firstArgument", "fixedParameterCount", "variadicAbi",
    "xl6IrCallVariadicAbiForParameter", "xl6IrTypeSupportsCVararg"
) ) {
    if ( ! $header.Contains($required) ) {
        throw "XLIR header is missing canonical CallSite ABI: $required"
    }
}
foreach ( $required in @(
    "xl6IrBuildCallSite", "xl6IrBuildCallSites", "xl6IrCallSiteGet",
    "XLIR call argument has no canonical parameter binding ABI",
    "XLIR C variadic call tail has no verified native value ABI",
    '\"callTargets\": [', '\"callSites\": [', '\"callArguments\": [',
    "call-target c%lu", "variadic=%s"
) ) {
    if ( ! $source.Contains($required) ) {
        throw "XLIR verifier/dump is missing canonical CallSite ABI: $required"
    }
}
foreach ( $required in @(
    "xl6IrNodeIsResolvedSyntaxCall", "xl6IrBuildSyntaxCallSite",
    "XLIR member CallSite has no canonical implicit string argument",
    "XLIR setter CallSite origin has no canonical assignment shape"
) ) {
    if ( ! $source.Contains($required) ) {
        throw "XLIR verifier is missing resolved syntax CallSite ABI: $required"
    }
}
foreach ( $required in @(
    "xl6IrLowerCallSiteAggregate", "xl6IrCallSiteGet",
    "xl6IrLowerSpecializationCloneCallSites", "xl6IrLowerRetargetCallSite",
    "XLIR CallSite variadic ABI does not match its target",
    "XLIR constructor CallSite has no fixed argument ABI",
    "XLIR C variadic tail item has no native scalar or pointer ABI",
    "XL6_IR_RUNTIME_XVALUE_ARRAY_APPEND_NEW",
    "XL6_IR_RUNTIME_XVALUE_OBJECT_SET_LITERAL_NEW"
) ) {
    if ( ! $lower.Contains($required) ) {
        throw "XLIR lowering is missing canonical CallSite consumption: $required"
    }
}
foreach ( $required in @(
    "xl6IrLowerEmitImplicitStringArgument",
    "XL6_IR_CALL_ARGUMENT_IMPLICIT_STRING",
    "XLIR resolved member protocol has no canonical CallSite",
    "XLIR resolved protocol setter has no canonical CallSite"
) ) {
    if ( ! $lower.Contains($required) ) {
        throw "XLIR lowering is missing resolved class syntax CallSite consumption: $required"
    }
}
foreach ( $required in @(
    "target->variadicAbi == XL6_IR_CALL_VARIADIC_C",
    'xl6OutputPuts(", ...", out)',
    "XLIR C emitter call target has no canonical variadic ABI"
) ) {
    if ( ! $cEmitter.Contains($required) ) {
        throw "XLIR C emitter is missing resolved CallTarget variadic ABI: $required"
    }
}
foreach ( $gate in @($callSiteBindingGate, $callSiteVariadicGate, $callSiteCVarargGate, $callSiteGenericGate) ) {
    foreach ( $required in @("--lowered", "--format json", "-Werror", "runtime failed") ) {
        if ( ! $gate.Contains($required) ) {
            throw "XLIR CallSite quality gate is incomplete: $required"
        }
    }
}
foreach ( $required in @(
    "cgen_protocol_ops.xl", "cgen_protocol_complex_receiver.xl",
    "xlir_class_protocol_syntax_generic.xl", "cgen_protocol_overload_index.xl",
    "--lowered", "implicit-string", "__setItem__", "-Werror", "runtime failed"
) ) {
    if ( ! $classProtocolSyntaxGate.Contains($required) ) {
        throw "XLIR resolved class syntax CallSite quality gate is incomplete: $required"
    }
}

# Language/runtime builtins without a callable declaration have a separate
# typed identity and operand/result/effect arena.  Lowering consumes that arena
# and must never reclassify these operations from a callee/member spelling.
foreach ( $required in @(
    "XL6_IrIntrinsicKind", "XL6_IrIntrinsicAdmission", "XL6_IrIntrinsicOperandAbi", "XL6_IrIntrinsicResultAbi",
    "XL6_IrIntrinsicOperand", "XL6_IrIntrinsicCall",
    "XL6_IR_INTRINSIC_ADMISSION_VERIFIED", "XL6_IR_INTRINSIC_ADMISSION_UNSUPPORTED_ABI",
    "XL6_IR_INTRINSIC_CHECKED_INTEGER_CONVERT", "XL6_IR_INTRINSIC_STRING_CONVERT",
    "XL6_IR_INTRINSIC_SCALAR_CONVERT", "XL6_IR_INTRINSIC_COPY",
    "XL6_IR_INTRINSIC_CLONE", "XL6_IR_INTRINSIC_ARRAY_APPEND",
    "XL6_IR_INTRINSIC_RANGE", "XL6_IR_INTRINSIC_XCALL",
    "XL6_IR_INTRINSIC_TRIVIAL_IDENTITY_CONVERT",
    "intrinsicCalls", "intrinsicOperands", "xl6IrIntrinsicCallGet"
) ) {
    if ( ! $header.Contains($required) ) {
        throw "XLIR header is missing typed IntrinsicCall ABI: $required"
    }
}
foreach ( $required in @(
    "xl6IrBuildIntrinsicCall", "xl6IrBuildIntrinsicCalls",
    "XLIR intrinsic call has no canonical identity, result, effect or operand slice ABI",
    "XLIR intrinsic operand ownership ABI does not match its typed operation",
    "xl6IrIntrinsicAdmissionName", '\"admissionName\":\"%s\"',
    '\"intrinsicCalls\": [', '\"intrinsicOperands\": [',
    "intrinsic-call q%lu", "intrinsic-operand o%lu"
) ) {
    if ( ! $source.Contains($required) ) {
        throw "XLIR verifier/dump is missing typed IntrinsicCall ABI: $required"
    }
}
foreach ( $required in @(
    "xl6IrLowerSpecializationCloneIntrinsicCalls",
    "xl6IrLowerIntrinsicAdmissionSupported", "XL6_IR_INTRINSIC_ADMISSION_VERIFIED",
    "xl6IrLowerIntrinsicCheckedIntegerConvert", "xl6IrLowerIntrinsicStringConvert",
    "xl6IrLowerIntrinsicScalarConvert", "xl6IrLowerIntrinsicCopyClone",
    "xl6IrLowerIntrinsicCollectionInsert", "xl6IrLowerIntrinsicXCall",
    "xl6IrLowerIntrinsicTrivialIdentityConvert",
    "intrinsic->kind", "XL6_IR_INTRINSIC_RESULT_DESCRIPTOR"
) ) {
    if ( ! $lower.Contains($required) ) {
        throw "XLIR lowering is missing typed IntrinsicCall consumption: $required"
    }
}
foreach ( $spelling in @("int8", "uint8", "int16", "uint16", "int32", "uint32", "int64", "string", "copy", "clone", "append", "push", "add", "contains", "remove", "range", "xcall") ) {
    if ( $lower -match ('strcmp\s*\([^\r\n]*"' + [regex]::Escape($spelling) + '"') ) {
        throw "XLIR lowering must not classify intrinsic spelling: $spelling"
    }
}
if ( $cEmitter.Contains("intrinsicCalls") -or $cEmitter.Contains("intrinsicOperands") -or
     $cEmitter.Contains("XL6_IrIntrinsicCall") ) {
    throw "XLIR C emitter must consume only lowered instructions, not Typed IntrinsicCall metadata"
}
foreach ( $required in @("--lowered", "--format json", "-Werror", "runtime failed",
                           "kindName", "admissionName", "resultAbiName", "errorAbiName",
                           "typed IntrinsicCall has no verified admission",
                           "Cgen must not consume Typed IntrinsicCall metadata",
                           "xlir_intrinsic_trivial_identity_convert.xl",
                           "trivial-identity-convert", "exact point ABI") ) {
    if ( ! $intrinsicCallGate.Contains($required) ) {
        throw "XLIR IntrinsicCall quality gate is incomplete: $required"
    }
}

# Nominal receiver identity is a Type ABI, not a class-name/self-use search.
foreach ( $required in @("nominalSymbol", "Canonical declaration identity for nominal types") ) {
    if ( ! $header.Contains($required) ) {
        throw "XLIR header is missing nominal receiver identity: $required"
    }
}
foreach ( $required in @(
    "nominalDeclaration", "XLIR nominal type has no exact declaration identity ABI",
    "owner->nominalSymbol != classNode->symbol", "xl6IrVerifyDeclarationOwners",
    'nominal=s%u', '\"nominalSymbol\":%u'
) ) {
    if ( ! $source.Contains($required) ) {
        throw "XLIR type verifier/dump is missing nominal receiver identity: $required"
    }
}
foreach ( $required in @(
    "node->declarationOwnerType",
    "type->nominalSymbol == classNode->symbol",
    "XL6_IR_PARAMETER_ABI_BORROW", "xl6IrLowerNativeHandleFieldProject",
    "xl6IrLowerSpecializationCloneNativeHandleProjects",
    "XL6_IR_INSTR_NATIVE_HANDLE_PROJECT"
) ) {
    if ( ! $lower.Contains($required) ) {
        throw "XLIR lowering is missing nominal borrowed receiver ABI: $required"
    }
}
foreach ( $required in @(
    "XL6_IrNativeHandleProject", "XL6_IR_INSTR_NATIVE_HANDLE_PROJECT",
    "nativeHandleProjects", "xl6IrNativeHandleProjectGet"
) ) {
    if ( ! $header.Contains($required) ) {
        throw "XLIR header is missing native handle projection ABI: $required"
    }
}
foreach ( $required in @(
    "xl6IrBuildNativeHandleProjects", "XLIR native handle member has no unique canonical projection ABI",
    'nativeHandleProjects=%lu', '\"nativeHandleProjects\": ['
) ) {
    if ( ! $source.Contains($required) ) {
        throw "XLIR verifier/dump is missing native handle projection ABI: $required"
    }
}
if ( ! $cEmitter.Contains("case XL6_IR_INSTR_NATIVE_HANDLE_PROJECT:") -or
     $cEmitter.Contains("nativeHandleProjects") ) {
    throw "XLIR C emitter must consume only the lowered native handle projection instruction"
}
if (!(Read-Utf8 'src/runtime/xl6_dynamic_fields.c').Contains('xl6_xrt_dynamic_fields_native_handle') -or
    !(Read-Utf8 'src/runtime/xl6_text_symbols.inc').Contains('XL6_TEXT_EXPORT(xl6_xrt_dynamic_fields_native_handle, DYNAMIC_FIELDS)') -or
    $generatedRuntime.Contains('static void* xl6_xrt_dynamic_fields_native_handle')) {
    throw "DynamicFields handle facade must remain resident and explicitly registered, not generated"
}
foreach ( $required in @("--lowered", "--format json", "cgen_native_class.xl",
                           "nativeHandleProjectCount",
                           "xvalue-object-get-literal-native-handle-borrow",
                           "owned-point MemberAbi", "-Werror", "runtime failed") ) {
    if ( ! $nativeNominalReceiverGate.Contains($required) ) {
        throw "XLIR native nominal receiver quality gate is incomplete: $required"
    }
}

# Raw point -> native-backed class is an explicit representation contract. The
# Typed arena distinguishes transitional direct borrow/take handles from a
# managed class-field borrow; lowering owns allocation, transfer and cleanup.
foreach ( $required in @(
    "XL6_IrNativeHandleBindAbi", "XL6_IrNativeHandleBind",
    "XL6_IR_NATIVE_HANDLE_BIND_BORROW", "XL6_IR_NATIVE_HANDLE_BIND_TAKE",
    "XL6_IR_NATIVE_HANDLE_BIND_CLASS_FIELD_BORROW", "fieldDeclaration",
    "XL6_IR_INSTR_NATIVE_HANDLE_BIND", "nativeHandleBinds"
) ) {
    if ( ! $header.Contains($required) ) {
        throw "XLIR header is missing native handle bind ownership ABI: $required"
    }
}
foreach ( $required in @(
    "xl6IrBuildNativeHandleBinds", "xl6IrNativeHandleBindGet",
    "xl6IrManagedNativeConstructorField", "class-field-borrow",
    "XLIR native handle constructor has no unique canonical bind ABI",
    '\"nativeHandleBinds\": [', "native-handle-bind h%lu"
) ) {
    if ( ! $source.Contains($required) ) {
        throw "XLIR verifier/dump is missing native handle bind ownership ABI: $required"
    }
}
foreach ( $required in @(
    "xl6IrLowerNativeHandleBind", "xl6IrLowerManagedNativeConstructorField",
    "XL6_IR_RUNTIME_XVALUE_CLASS_NEW",
    "XL6_IR_RUNTIME_XVALUE_CLASS_IDENTITY_BIND",
    "XL6_IR_RUNTIME_XVALUE_RAW_POINTER_BORROW",
    "XL6_IR_RUNTIME_XVALUE_OBJECT_SET_LITERAL_NEW",
    "xl6IrLowerEmitNativeDestroyIfNotNull",
    "xl6IrLowerNativeBindForCallOperand",
    "xl6IrLowerSpecializationCloneNativeHandleBinds",
    "XL6_IR_INSTR_NATIVE_HANDLE_BIND", "XL6_IR_INSTR_NULL_TEST"
) ) {
    if ( ! $lower.Contains($required) ) {
        throw "XLIR lowering is missing native handle bind ownership consumption: $required"
    }
}
foreach ( $required in @(
    "case XL6_IR_INSTR_NATIVE_HANDLE_BIND", "v%u = (void*)v%u",
    "XLIR C emitter native handle bind ABI is unresolved"
) ) {
    if ( ! $cEmitter.Contains($required) ) {
        throw "XLIR C emitter is missing mechanical native handle bind emission: $required"
    }
}
foreach ( $required in @(
    "cgen_native_class_bind_point.xl", "cgen_native_class_bind_point_basic.xl",
    "Typed direct/managed-field TAKE ABI", "source clearing", "temporary owner",
    "managed-resource parameter borrow", "destroy-directed", "-Werror", "runtime failed"
) ) {
    if ( ! $nativeHandleBindGate.Contains($required) ) {
        throw "XLIR native handle bind quality gate is incomplete: $required"
    }
}
foreach ( $required in @(
    "cgen_typed_container_objects.xl", "class-field-borrow",
    "fieldDeclaration", "runtime-reference", "xvalue-class-new",
    "xvalue-class-identity-bind", "xvalue-raw-pointer-borrow",
    "class-field-init-new", "executionOrigin", "instantiations",
    "SequenceEqual", "strict-C/runtime gate failed",
    "standalone artifact runtime gate failed"
) ) {
    if ( ! $nativeClassObjectIdentityAbiGate.Contains($required) ) {
        throw "XLIR managed native class object identity quality gate is incomplete: $required"
    }
}
if ( ! $languageFrontendSuite.Contains(
         "tools\check_xlir_native_class_object_identity_abi.ps1") ) {
    throw "XLIR managed native class object identity gate is not wired into the language frontend suite"
}
if ( ! $runtimeMatrix.Contains(
         "field-only native-backed managed object identity ABI") -or
     ! $runtimeMatrix.Contains("cgen_typed_container_objects.xl") ) {
    throw "XLIR managed native class object identity fixture is not wired into runtime-matrix coverage"
}

# A managed native-backed class remains a semantic xvalue at an extern call.
# The CallTarget owns the exact physical field-borrow ABI, lowering materializes
# the raw-pointer projection, and Cgen consumes only that verified operand.
foreach ( $required in @(
    "XL6_IR_CALL_PARAMETER_PHYSICAL_NATIVE_CLASS_HANDLE_BORROW",
    "parameterPhysicalFieldDeclarations"
) ) {
    if ( ! $header.Contains($required) ) {
        throw "XLIR header is missing managed native extern receiver physical ABI: $required"
    }
}
foreach ( $required in @(
    "xl6IrCallTargetNativeClassPhysicalField", "native-class-handle-borrow",
    "XLIR native class call operand has no exact lowered managed-field borrow ABI"
) ) {
    if ( ! $source.Contains($required) ) {
        throw "XLIR verifier/dump is missing managed native extern receiver physical ABI: $required"
    }
}
foreach ( $required in @(
    "xl6IrLowerCallTargetNativeClassPhysicalField",
    "XLIR native class call operand has no exact managed-field physical ABI"
) ) {
    if ( ! $lower.Contains($required) ) {
        throw "XLIR lowering is missing managed native extern receiver projection: $required"
    }
}
foreach ( $required in @(
    "xl6IrCgenNativeClassPhysicalField",
    "has no exact managed native-class field-borrow ABI"
) ) {
    if ( ! $cEmitter.Contains($required) ) {
        throw "XLIR C emitter is missing mechanical managed native extern receiver consumption: $required"
    }
}
foreach ( $required in @(
    "xlir_managed_native_extern_receiver_abi.xl", "class-field-borrow",
    "parameterPhysicalAbis", "native-class-handle-borrow",
    "parameterPhysicalFieldDeclarations",
    "xvalue-object-get-literal-raw-pointer", "direct nominal-handle projector",
    "SequenceEqual", "strict-C/runtime gate failed",
    "strict-C/runtime gate failed"
) ) {
    if ( ! $managedNativeExternReceiverGate.Contains($required) ) {
        throw "XLIR managed native extern receiver quality gate is incomplete: $required"
    }
}
if ( ! $languageFrontendSuite.Contains(
         "tools\check_xlir_managed_native_extern_receiver_abi.ps1") ) {
    throw "XLIR managed native extern receiver gate is not wired into the language frontend suite"
}
if ( ! $runtimeMatrix.Contains(
         "managed native extern receiver physical field-borrow ABI") -or
     ! $runtimeMatrix.Contains("xlir_managed_native_extern_receiver_abi.xl") ) {
    throw "XLIR managed native extern receiver fixture is not wired into runtime-matrix coverage"
}

# Value/default initialization is resolved in Typed XLIR.  Lowering consumes
# the declaration contract; Cgen sees only CONST/NULL/runtime/store effects.
foreach ( $required in @(
    "XL6_IrInitializerAbi", "XL6_IrValueInitializer",
    "XL6_IR_INITIALIZER_EXPLICIT", "XL6_IR_INITIALIZER_SCALAR_ZERO",
    "XL6_IR_INITIALIZER_STRING_EMPTY", "XL6_IR_INITIALIZER_NULL_SENTINEL",
    "XL6_IR_INITIALIZER_XVALUE_NULL", "XL6_IR_INITIALIZER_XVALUE_CONTAINER_EMPTY",
    "XL6_IR_INITIALIZER_CLASS_DEFAULT", "XL6_IR_INITIALIZER_CLASS_NULL",
    "XL6_IR_INITIALIZER_TAGGED_NULL",
    "XL6_IR_RUNTIME_XVALUE_CLASS_NEW",
    "XL6_IR_RUNTIME_XVALUE_NATIVE_HANDLE_TAKE",
    "nativeDestroySymbol", "valueInitializers"
) ) {
    if ( ! $header.Contains($required) ) {
        throw "XLIR header is missing the canonical ValueInitializer ABI: $required"
    }
}
foreach ( $required in @(
    "xl6IrBuildValueInitializers", "xl6IrDefaultEmptyContainerRuntimeOp",
    "xl6IrValueDeclarationUsesInitializerAbi", "xl6IrValueInitializerGet",
    "XLIR value declaration has no unique canonical initializer ABI",
    "XLIR runtime class storage construction has no canonical ABI",
    "XLIR native handle dynamic transfer has no exact owning destructor ABI",
    '\"valueInitializers\": [', '\"runtimeOpName\":\"%s\"', '\"nativeDestroySymbol\":%u',
    "value-initializer i%lu", "xvalue-class-new",
    "xvalue-native-handle-take"
) ) {
    if ( ! $source.Contains($required) ) {
        throw "XLIR verifier/dump is missing the canonical ValueInitializer ABI: $required"
    }
}
foreach ( $required in @(
    "xl6IrLowerValueInitializer", "xl6IrLowerSpecializationCloneValueInitializers",
    "XL6_IR_INITIALIZER_SCALAR_ZERO", "XL6_IR_INITIALIZER_STRING_EMPTY",
    "XL6_IR_INITIALIZER_NULL_SENTINEL", "XL6_IR_INITIALIZER_XVALUE_NULL",
    "XL6_IR_INITIALIZER_XVALUE_CONTAINER_EMPTY",
    "XL6_IR_INITIALIZER_CLASS_DEFAULT", "XL6_IR_INITIALIZER_CLASS_NULL",
    "XL6_IR_INITIALIZER_TAGGED_NULL",
    "xl6IrLowerDefaultClassConstruct",
    "XLIR value declaration has no verified initializer ABI",
    "xl6IrLowerExplicitClassInitializeOwnFields",
    "xl6IrLowerBoxRuntimeXValueWithNativeDestroy",
    "xl6IrLowerClassFieldGet",
    "XL6_IR_RUNTIME_XVALUE_CLASS_NEW",
    "XL6_IR_RUNTIME_XVALUE_NATIVE_HANDLE_TAKE",
    "Field declaration initializers run before an explicit create body",
    "XLIR explicit class constructor has no verified own-field initializer ABI"
) ) {
    if ( ! $lower.Contains($required) ) {
        throw "XLIR lowering is missing canonical ValueInitializer consumption: $required"
    }
}
if ( $cEmitter.Contains("valueInitializers") ) {
    throw "XLIR C emitter must not consume Typed ValueInitializer metadata"
}
foreach ( $required in @(
    "xlir_value_initializer_defaults.xl", "xlir_value_initializer_class_default.xl",
    "xlir_value_initializer_class_null.xl",
    "cgen_native_class_ownership.xl",
    "cgen_native_class_null_destroy.xl", "xlir_value_initializer_containers.xl",
    "xlir_value_initializer_package_exports.xl", "module package exports",
    "scalar-zero", "string-empty", "xvalue-container-empty",
    "xvalue-int-map-new", "xvalue-object-new",
    "null-sentinel", "class-default construction", "explicit-constructor class-null semantics",
    "destroy-aware release",
    "-Werror", "runtime failed"
) ) {
    if ( ! $valueInitializerGate.Contains($required) ) {
        throw "XLIR ValueInitializer quality gate is incomplete: $required"
    }
}
foreach ( $required in @(
    "xlir_explicit_constructor_field_initializer.xl", "name=declared init=explicit",
    "name=untouched init=scalar-zero", "before the create body", "-Werror", "runtime failed"
) ) {
    if ( ! $explicitConstructorFieldGate.Contains($required) ) {
        throw "XLIR explicit-constructor field-initializer gate is incomplete: $required"
    }
}
foreach ( $required in @(
    "xlir_class_reference_field_initializer.xl",
    "cgen_nested_class_constructor.xl", "cgen_class_native_field.xl",
    "runtime=xvalue-null", "runtime=xvalue-class-new",
    'runtimeOpName -eq "xvalue-native-handle-take"',
    "nativeDestroySymbol -le 0",
    "xrtValueObjectLifo", "-Werror", "generated-C runtime failed"
) ) {
    if ( ! $classReferenceFieldGate.Contains($required) ) {
        throw "XLIR class reference-field/owned-destruction gate is incomplete: $required"
    }
}
foreach ( $required in @(
    "XL6_IR_RUNTIME_XVALUE_NATIVE_HANDLE_TAKE",
    "xlir_native_handle_take_s", "xrtValueObjectLifo",
    "XLIR C emitter native handle transfer has no destructor linkage ABI"
) ) {
    if ( ! $cEmitter.Contains($required) ) {
        throw "XLIR C emitter is missing mechanical class-field ownership emission: $required"
    }
}

# Persistence derives and field attributes become an explicit Typed XLIR
# schema. Lowering selects one exact RuntimeOp and Cgen projects only that
# verified schema; it must never rediscover json.stringify or field attrs.
foreach ( $required in @(
    "XL6_IR_INTRINSIC_PERSISTENCE_JSON_STRINGIFY",
    "XL6_IR_INTRINSIC_PERSISTENCE_JSON_PARSE",
    "XL6_IR_INTRINSIC_PERSISTENCE_JSON_WRITE",
    "XL6_IR_INTRINSIC_PERSISTENCE_JSON_READ",
    "XL6_IR_INTRINSIC_PERSISTENCE_XLON_STRINGIFY",
    "XL6_IR_INTRINSIC_PERSISTENCE_XLON_PARSE",
    "XL6_IR_INTRINSIC_PERSISTENCE_XLON_WRITE",
    "XL6_IR_INTRINSIC_PERSISTENCE_XLON_READ",
    "XL6_IR_RUNTIME_PERSISTENCE_JSON_STRINGIFY",
    "XL6_IR_RUNTIME_PERSISTENCE_JSON_PARSE",
    "XL6_IR_RUNTIME_PERSISTENCE_JSON_WRITE",
    "XL6_IR_RUNTIME_PERSISTENCE_JSON_READ",
    "XL6_IR_RUNTIME_PERSISTENCE_XLON_STRINGIFY",
    "XL6_IR_RUNTIME_PERSISTENCE_XLON_PARSE",
    "XL6_IR_RUNTIME_PERSISTENCE_XLON_WRITE",
    "XL6_IR_RUNTIME_PERSISTENCE_XLON_READ",
    "XL6_IR_FIELD_PERSIST_INCLUDED", "XL6_IR_FIELD_PERSIST_OMIT_EMPTY",
    "XL6_IR_PERSIST_FORMAT_NONE", "XL6_IR_PERSIST_FORMAT_JSON",
    "XL6_IR_PERSIST_FORMAT_XLON", "jsonPersistenceFlags",
    "xlonPersistenceFlags", "jsonPersistenceKeyLiteral",
    "xlonPersistenceKeyLiteral", "persistenceFormatMask"
) ) {
    if ( ! $header.Contains($required) ) {
        throw "XLIR header is missing canonical JSON persistence ABI: $required"
    }
}
foreach ( $required in @(
    "xl6IrBuildFieldPersistence", "xl6IrBuildClassPersistenceFormatsFrom",
    "xl6IrBuildPersistenceJsonLayoutSupported",
    "xl6IrBuildPersistenceXlonLayoutSupported",
    "xl6IrBuildPersistenceLayoutSupported",
    "xl6IrBuildPersistenceGenericTarget",
    "persistence-json-stringify", "persistence-json-parse",
    "persistence-json-write", "persistence-json-read",
    "persistence-xlon-stringify", "persistence-xlon-parse",
    "persistence-xlon-write", "persistence-xlon-read",
    "xl6IrPersistenceLegacyIntrinsicStructure",
    "XLIR legacy persistence has no exact source-order roles, schema and bool ABI",
    "XLIR persistence parse intrinsic has no canonical format, generic target and class schema ABI"
) ) {
    if ( ! $source.Contains($required) ) {
        throw "XLIR verifier/dump is missing canonical JSON persistence ABI: $required"
    }
}
foreach ( $required in @(
    "xl6IrLowerIntrinsicPersistenceLegacy",
    "xl6IrLowerPersistenceProject",
    "xl6IrLowerIntrinsicPersistenceParse",
    "xl6IrLowerPersistenceClassDefault",
    "XL6_IR_RUNTIME_PERSISTENCE_JSON_STRINGIFY",
    "XL6_IR_RUNTIME_PERSISTENCE_JSON_PARSE",
    "XL6_IR_RUNTIME_PERSISTENCE_JSON_WRITE",
    "XL6_IR_RUNTIME_PERSISTENCE_JSON_READ",
    "XL6_IR_RUNTIME_PERSISTENCE_XLON_STRINGIFY",
    "XL6_IR_RUNTIME_PERSISTENCE_XLON_PARSE",
    "XL6_IR_RUNTIME_PERSISTENCE_XLON_WRITE",
    "XL6_IR_RUNTIME_PERSISTENCE_XLON_READ",
    "XLIR persistence stringify intrinsic has no exact format, borrowed class, schema, pretty flag and owned string ABI",
    "XLIR persistence write intrinsic has no exact format, borrowed path/class, schema, pretty flag and boolean ABI",
    "XLIR persistence parse intrinsic has no exact format, borrowed text, default class and schema ABI"
) ) {
    if ( ! $lower.Contains($required) ) {
        throw "XLIR lowering is missing canonical JSON persistence consumption: $required"
    }
}
foreach ( $required in @(
    "xl6IrCgenPersistenceStringifyAbi", "xl6IrPersistenceInvocationInstructionAbi",
    "xl6IrCgenPersistenceParseAbi",
    "xl6IrCgenEmitPersistenceHelpers",
    "XL6_IR_RUNTIME_PERSISTENCE_JSON_STRINGIFY",
    "XL6_IR_RUNTIME_PERSISTENCE_JSON_PARSE",
    "XL6_IR_RUNTIME_PERSISTENCE_JSON_WRITE",
    "XL6_IR_RUNTIME_PERSISTENCE_JSON_READ",
    "XL6_IR_RUNTIME_PERSISTENCE_XLON_STRINGIFY",
    "XL6_IR_RUNTIME_PERSISTENCE_XLON_PARSE",
    "XL6_IR_RUNTIME_PERSISTENCE_XLON_WRITE",
    "XL6_IR_RUNTIME_PERSISTENCE_XLON_READ",
    "xl6IrCgenEmitPersistenceProjectPlans", "xl6IrCgenEmitNativeRestoreHelpers",
    "xl6IrVerifyPersistenceProjectPlans", "xl6PersistenceRestoreParsedTakeV1",
    "xl6IrCgenPersistenceProjectSymbol", "xlir_persist_%s_parse_t", "xlir_persist_%s_read_t",
    'stringifyName = "xrtJsonStringify"',
    'parseName = "xrtJsonParse"',
    'writeName = "xrtJsonStringifyFile"',
    'readName = "xrtJsonParseFile"',
    'stringifyName = "xrtXlonStringify"',
    'parseName = "xrtXlonParse"',
    'writeName = "xrtXlonStringifyFile"',
    'readName = "xrtXlonParseFile"',
    'char* text = %s(v%u, v%u, &size)',
    'bool projection_ok_%u',
    'XL6_PersistencePlanV1',
    'xlir_string_cstr(path)'
) ) {
    if ( ! $cEmitter.Contains($required) ) {
        throw "XLIR C emitter is missing mechanical JSON persistence emission: $required"
    }
}
foreach ( $forbidden in @(
    "__xl_std_json_stringify", "__xl_std_json_parse",
    "__xl_std_json_write", "__xl_std_json_read",
    "__xl_std_xlon_stringify", "__xl_std_xlon_parse",
    "__xl_std_xlon_write", "__xl_std_xlon_read",
    "json.stringify", "json.parse", "json.write", "json.read",
    "xlon.stringify", "xlon.parse", "xlon.write", "xlon.read",
    "XL6_Hir", "XL6_Ast",
    "callee->text"
) ) {
    if ( $cEmitter.Contains($forbidden) ) {
        throw "XLIR C emitter reconstructs JSON persistence semantics: $forbidden"
    }
}
foreach ( $required in @(
    "check_persistence_project_language.py", "--fixture abi", "PublicDirectory", "--out"
) ) {
    if ( ! $persistenceJsonGate.Contains($required) ) {
        throw "XLIR JSON persistence quality gate is incomplete: $required"
    }
}
if ( ! $languageFrontendSuite.Contains(
        "tools\check_xlir_persistence_json_abi.ps1") ) {
    throw "XLIR JSON persistence gate is not wired into the frontend suite"
}
if ( ! $runtimeMatrix.Contains("xlir_persistence_json_abi.xl") ) {
    throw "XLIR JSON persistence fixture is missing from the runtime matrix"
}
foreach ($required in @('PersistenceProfile','jsonPersistenceFlags','xlonPersistenceFlags','may-raise',
    'persistenceRestoreRoots','BoxedProjectionTransactions','xvalue-string-copy','xvalue-i64')) {
    if (!(Read-Utf8 'tools/xlir_persistence_abi_checks.py').Contains($required)) {
        throw "Delegated persistence ABI proof lost a typed ownership/schema check: $required"
    }
}

# Multiple semantic returns cross every callable boundary as one canonical
# tuple TypeId.  Lowered XLIR constructs/projects that product explicitly;
# closure signatures expose the same physical result through callableResultType.
foreach ( $required in @(
    "XL6_IR_C_ABI_PRODUCT", "XL6_IR_INSTR_PRODUCT_MAKE",
    "XL6_IR_INSTR_PRODUCT_GET", "XL6_IR_INSTR_PRODUCT_TAKE",
    "XL6_IR_INSTR_PRODUCT_DROP", "callableResultType", "elementIndex", "cName",
    "xl6IrTypeInternTuple"
) ) {
    if ( ! $header.Contains($required) ) {
        throw "XLIR header is missing canonical product-result ABI: $required"
    }
}
foreach ( $required in @(
    "xl6IrTypePoolFinalizeCallableResults",
    "XL6_IrTypeId xl6IrTypeInternTuple",
    "XLIR product construction has no exact structural ABI",
    "XLIR product projection has no exact element ABI",
    "xl6IrVerifyManagedProductConsumption",
    "XLIR managed product has an ambiguous aggregate ownership use",
    '\"callableResultType\":%u', '\"calleeType\":%u',
    "product-make", "product-get"
) ) {
    if ( ! $source.Contains($required) ) {
        throw "XLIR verifier/dump is missing canonical product-result ABI: $required"
    }
}
foreach ( $required in @(
    "XLIR multi-result callable has no verified product ABI",
    "xl6IrTypeInternTuple(module, resultTypes, returns->childCount)",
    "XL6_IR_INSTR_PRODUCT_MAKE", "XL6_IR_INSTR_PRODUCT_GET",
    "XL6_IR_INSTR_PRODUCT_TAKE", "xl6IrLowerProductBindings",
    "signature->callableResultType", "elementIndex"
) ) {
    if ( ! $lower.Contains($required) ) {
        throw "XLIR lowering is missing canonical product-result consumption: $required"
    }
}
foreach ( $required in @(
    "xl6IrCgenEmitProductTypes", "case XL6_IR_INSTR_PRODUCT_MAKE",
    "case XL6_IR_INSTR_PRODUCT_GET", "case XL6_IR_INSTR_PRODUCT_TAKE",
    "case XL6_IR_INSTR_PRODUCT_DROP", "signature->callableResultType"
) ) {
    if ( ! $cEmitter.Contains($required) ) {
        throw "XLIR C emitter is missing mechanical product-result emission: $required"
    }
}
foreach ( $required in @(
    "cgen_multi_return.xl", "cgen_class_method_multi_return.xl",
    "xlir_product_closure_result.xl", "xlir_product_generic_result.xl",
    "xlir_product_declaration_only.xl", "declaration-driven tuple interning",
    "xlir_managed_product_result.xl", "protected managed field ownership",
    "callableResultType", "product-make", "product-take", "product-drop",
    "product-get", "-Werror", "runtime failed"
) ) {
    if ( ! $productResultGate.Contains($required) ) {
        throw "XLIR product-result quality gate is incomplete: $required"
    }
}

# A named function value is a lowering-owned adapter CFG, not a storage LOAD
# or a Cgen-side function-pointer heuristic.  The closure target records its
# resolved source target while the synthetic function owns forwarded/default
# arguments, direct CALL and RETURN.
foreach ( $required in @(
    "XL6_IR_FUNCTION_NAMED_ADAPTER", "XL6_IrClosureTargetKind",
    "XL6_IR_CLOSURE_TARGET_NAMED_ADAPTER", "adaptedTargetSymbol",
    "XL6_IrRuntimeBridgeKind", "XL6_IR_RUNTIME_BRIDGE_PRINT",
    "XL6_IR_RUNTIME_BRIDGE_STRING_CONTAINS",
    "XL6_IR_RUNTIME_BRIDGE_STRING_SPLIT_LINES",
    "XL6_IR_RUNTIME_BRIDGE_STRING_JOIN", "xl6IrRuntimeStringBridgeErrorAbi",
    "XL6_IR_RUNTIME_BRIDGE_DYNAMIC_FIELDS",
    "XL6_IR_RUNTIME_BRIDGE_RESERVED_TABLE_REMOVE",
    "XL6_IR_RUNTIME_BRIDGE_STRING_INDEX_OF",
    "XL6_IR_RUNTIME_BRIDGE_JSON_PARSE",
    "XL6_IR_RUNTIME_BRIDGE_TEXT_VALUE",
    "XL6_IR_RUNTIME_BRIDGE_TEMPLATE", "XL6_IR_CALL_TARGET_EXTERN_C",
    "XL6_IR_RUNTIME_BRIDGE_TEXT_CODEC",
    "XL6_IR_RUNTIME_BRIDGE_CRYPTO_TEXT_HASH",
    "XL6_IR_RUNTIME_BRIDGE_PATH_JOIN", "XL6_IR_RUNTIME_BRIDGE_FILE",
    "XL6_IR_RUNTIME_BRIDGE_LAST_ERROR_TEXT",
    "XL6_IrRuntimeBridgeOperation",
    "XL6_IR_RUNTIME_BRIDGE_OPERATION_NET_TCP_FILE_TRANSFER",
    "XL6_IR_RUNTIME_BRIDGE_OPERATION_NET_TCP_RECV_OWNED_BYTES_ASYNC",
    "XL6_IR_RUNTIME_BRIDGE_OPERATION_NET_TLS_RECV_OWNED_BYTES_ASYNC",
    "XL6_IrCallErrorPhysicalAbi",
    "XL6_IR_CALL_ERROR_PHYSICAL_RESULT_CARRIER",
    "XL6_IR_CALL_ERROR_PHYSICAL_XRT_THREAD_ERROR",
    "XL6_IR_CALL_ERROR_PHYSICAL_NULL_RESULT", "errorPhysicalAbi",
    "runtimeBridge", "runtimeBridgeOperation"
) ) {
    if ( ! $header.Contains($required) ) {
        throw "XLIR header is missing named-function adapter ABI: $required"
    }
}
foreach ( $required in @(
    "XLIR named-function adapter has no complete signature and CFG ABI",
    "XLIR named-function adapter parameter does not match its signature ABI",
    "adaptedTargetSymbol", "named-adapter", "xl6IrRuntimeBridgeTargetMatches",
    "runtimeBridgeName", "runtimeBridgeOperationName",
    "xl6IrRuntimeBridgeOperationTargetMatches",
    "has no exact compiler-runtime bridge ABI"
) ) {
    if ( ! $source.Contains($required) ) {
        throw "XLIR verifier/dump is missing named-function adapter ABI: $required"
    }
}
foreach ( $required in @(
    "xl6IrLowerRegisterNamedAdapter", "xl6IrLowerBeginNamedAdapterFunction",
    "xl6IrLowerNamedAdapterBody", "XL6_IR_FUNCTION_NAMED_ADAPTER",
    "XLIR named function adapter CFG could not be materialized exactly",
    "xl6IrLowerResolveRuntimeBridge",
    "xl6IrRuntimeBridgeOperationForName",
    "XLIR string-contains runtime bridge has no exact private helper ABI",
    "target->errorAbi = xl6IrRuntimeStringBridgeErrorAbi(target->cName)",
    "xl6IrRuntimeStringBridgeTargetMatches(ctx->module, target)",
    "XLIR string runtime bridge has no exact parameter/result ABI",
    "XLIR xvalue table-remove runtime bridge has no exact private helper ABI",
    "XLIR string-index-of runtime bridge has no exact private helper ABI",
    "XLIR JSON value runtime bridge has no exact parameter/result ABI",
    "XLIR direct runtime bridge has no exact declared ABI",
    "XLIR DynamicFields runtime bridge has no exact private helper ABI",
    "XLIR template runtime bridge has no exact borrowed native handle ABI",
    "xl6IrRuntimeDirectBridgeTargetMatches(ctx->module, target)",
    "XLIR File runtime bridge has no exact component ABI",
    "XLIR last-error-text runtime bridge has no exact owned-string ABI",
    "XLIR call target has no exact physical error ABI",
    "XLIR text codec runtime bridge has no exact ABI",
    "XLIR crypto text-hash runtime bridge has no exact ABI",
    "XLIR path-join runtime bridge has no exact strings/rest/result ABI"
) ) {
    if ( ! $lower.Contains($required) ) {
        throw "XLIR lowering is missing named-function adapter CFG: $required"
    }
}
foreach ( $required in @(
    "XL6_IR_CLOSURE_TARGET_NAMED_ADAPTER", "XL6_IR_FUNCTION_NAMED_ADAPTER",
    "XLIR C emitter closure adapter role is unresolved",
    "target->runtimeBridge == XL6_IR_RUNTIME_BRIDGE_PRINT",
    "target->runtimeBridge == XL6_IR_RUNTIME_BRIDGE_TEXT_VALUE",
    "XL6_IR_RUNTIME_BRIDGE_DYNAMIC_FIELDS",
    "xl6DictValueRemoveV1",
    "XL6_IR_RUNTIME_BRIDGE_JSON_PARSE",
    "XL6_IR_RUNTIME_BRIDGE_TEMPLATE", "XL6_IR_CALL_TARGET_EXTERN_C",
    "XL6_IR_RUNTIME_BRIDGE_TEXT_CODEC",
    "XL6_IR_RUNTIME_BRIDGE_CRYPTO_TEXT_HASH",
    "XL6_IR_RUNTIME_BRIDGE_PATH_JOIN", "XL6_IR_RUNTIME_BRIDGE_FILE",
    "XL6_IR_RUNTIME_BRIDGE_LAST_ERROR_TEXT",
    "XL6_IR_CALL_ERROR_PHYSICAL_RESULT_CARRIER",
    "XL6_IR_CALL_ERROR_PHYSICAL_XRT_THREAD_ERROR",
    "needsPathJoinBridgeRuntime", "void* xl6_xrt_path_join",
    "needsLastErrorTextBridgeRuntime", "xlir_string xl6_xrt_last_error_text",
    'xl6ErrorMessageCopyV1(xrtGetError())'
) ) {
    if ( ! $cEmitter.Contains($required) ) {
        throw "XLIR C emitter is missing mechanical named-adapter consumption: $required"
    }
}
foreach ( $required in @(
    "cgen_function_adapter_dedupe", "cgen_named_function_argument",
    "cgen_function_multi_return", "cgen_c_callback_export",
    "cgen_function_value_error_propagation", "cgen_extern_function_value",
    "xlir_named_function_extern_adapter",
    "runtimeBridgeName", "may-raise", "error-test",
    "--lowered", "--format json", "-Werror", "runtime failed"
) ) {
    if ( ! $namedFunctionAdapterGate.Contains($required) ) {
        throw "XLIR named-function adapter quality gate is incomplete: $required"
    }
}

# Collection member spelling is resolved once at the Typed XLIR boundary.
# Lowering and Cgen consume only intrinsic/runtime identities plus the exact
# collection-family ABI.  Managed BORROW parameters are independently owned
# lexical bindings before they may be rebound.
foreach ( $required in @(
    "XL6_IR_INTRINSIC_SET_ADD", "XL6_IR_INTRINSIC_COLLECTION_CONTAINS",
    "XL6_IR_INTRINSIC_COLLECTION_REMOVE",
    "XL6_IR_RUNTIME_XVALUE_COLLECTION_REMOVE", "collectionAbi"
) ) {
    if ( ! $header.Contains($required) ) {
        throw "XLIR header is missing canonical collection-member ABI: $required"
    }
}
foreach ( $required in @(
    "xl6IrCollectionMemberCalleeKind",
    "XLIR collection member intrinsic has no canonical identity ABI",
    "xl6IrVerifyBorrowedParameterOwnerAbi",
    "XLIR managed borrowed parameter has no exact lexical-owner entry prologue ABI",
    "functionParameters", "collectionAbiName"
) ) {
    if ( ! $source.Contains($required) ) {
        throw "XLIR source is missing collection-member or parameter-owner verification: $required"
    }
}
foreach ( $required in @(
    "xl6IrLowerIntrinsicCollectionInsert",
    "xl6IrLowerIntrinsicCollectionContains",
    "xl6IrLowerIntrinsicCollectionRemove",
    "xl6IrLowerMaterializeBorrowedParameterOwners"
) ) {
    if ( ! $lower.Contains($required) ) {
        throw "XLIR lowering is missing canonical collection-member or parameter-owner ABI: $required"
    }
}
foreach ( $required in @(
    "XL6_IR_RUNTIME_XVALUE_COLLECTION_REMOVE", "xrtValueArrayRemove",
    "xrtValueIntMapRemove", "xrtValueSetRemove", "xrtValueObjectRemove"
) ) {
    if ( ! $cEmitter.Contains($required) ) {
        throw "XLIR Cgen is missing mechanical collection-member runtime emission: $required"
    }
}
foreach ( $required in @(
    "cgen_param_modes_container_matrix", "cgen_typed_container_param_mutation",
    "collectionAbiName", "functionParameters", "lexical-owner", "-Werror",
    "strict-C runtime failed"
) ) {
    if ( ! $collectionMemberGate.Contains($required) ) {
        throw "XLIR collection-member quality gate is incomplete: $required"
    }
}
if ( ! $languageFrontendSuite.Contains("tools\check_xlir_collection_member_intrinsics.ps1") ) {
    throw "XLIR collection-member quality gate is not wired into the language frontend suite"
}
# Collection algebra is resolved from canonical opcode plus exact collection
# TypeIds.  Lowering selects one closed runtime identity and Cgen renders only
# that identity; generic xvalue arithmetic cannot recover collection meaning.
foreach ( $required in @(
    "XL6_IR_RUNTIME_XVALUE_ARRAY_CONCAT",
    "XL6_IR_RUNTIME_XVALUE_LIST_MERGE_RIGHT",
    "XL6_IR_RUNTIME_XVALUE_DICT_MERGE_RIGHT",
    "XL6_IR_RUNTIME_XVALUE_SET_SYMMETRIC_DIFFERENCE",
    "xl6IrCollectionAlgebraRuntimeOp"
) ) {
    if ( ! $header.Contains($required) ) {
        throw "XLIR header is missing closed collection-algebra ABI: $required"
    }
}
foreach ( $required in @(
    "xl6IrCollectionAlgebraRuntimeOp",
    "xl6IrCollectionAlgebraRuntimeSignatureValid",
    "typed XLIR collection algebra has no exact opcode and TypeId ABI"
) ) {
    if ( ! $source.Contains($required) ) {
        throw "XLIR source is missing collection-algebra admission or verification: $required"
    }
}
foreach ( $required in @(
    "xl6IrLowerCollectionAlgebraExpression",
    "xl6IrLowerStorageCompoundAssign",
    "xl6IrLowerCompleteEvaluationBorrows"
) ) {
    if ( ! $lower.Contains($required) ) {
        throw "XLIR lowering is missing closed collection-algebra semantics: $required"
    }
}
$algebraStart = $lower.IndexOf('static XL6_IrValueId xl6IrLowerCollectionAlgebraExpression(')
$algebraEnd = $lower.IndexOf('static XL6_IrValueId xl6IrLowerManagedStringConcatValue(', $algebraStart)
if ($algebraStart -lt 0 -or $algebraEnd -le $algebraStart) { throw 'Collection algebra transaction lowering is missing' }
$algebraBody = $lower.Substring($algebraStart, $algebraEnd - $algebraStart)
foreach ($required in @('xl6IrLowerAcquireValueOwner', 'xl6IrLowerProtectEvaluationStorage', 'xl6IrLowerCompleteEvaluationBorrows')) {
    if (!$algebraBody.Contains($required)) { throw "Collection algebra must use shared evaluation ownership: $required" }
}
if ($algebraBody.Contains('XL6_IR_INSTR_RELEASE') -or
    !(Read-Utf8 'test/cases/xlir_evaluation_cleanup.xl').Contains('cell.value.remove(evaluationReplaceDict(cell))') -or
    !(Read-Utf8 'test/cases/xlir_evaluation_cleanup_runtime.c').Contains('xrtMemDebugFailTriggered')) {
    throw 'Collection operands require shared failure rollback plus receiver-replacement and allocation-fault coverage'
}
foreach ( $required in @(
    "xl6IrCgenEmitCollectionAlgebraInstruction",
    "xl6IrCgenRuntimeCollectionAlgebraAbi",
    "xrtValueArrayConcat", "xrtValueSetIsSubset"
) ) {
    if ( ! $cEmitter.Contains($required) ) {
        throw "XLIR Cgen is missing mechanical collection-algebra emission: $required"
    }
}
foreach ( $required in @(
    "cgen_container_ops.xl", "22 explicit array/list/dict/set operations",
    "exact TypeId signatures", "byte-for-byte deterministic mechanical C",
    "no generic/legacy fallback", "-Werror", "strict-C runtime failed"
) ) {
    if ( ! $collectionAlgebraGate.Contains($required) ) {
        throw "XLIR collection-algebra quality gate is incomplete: $required"
    }
}
if ( ! $languageFrontendSuite.Contains("tools\check_xlir_collection_algebra.ps1") ) {
    throw "XLIR collection-algebra quality gate is not wired into the language frontend suite"
}
foreach ( $required in @(
    "XL6_IrTypedCollectionOp", "XL6_IrDynamicCollectionOp",
    "XL6_IR_INTRINSIC_TYPED_COLLECTION_MEMBER",
    "XL6_IR_INTRINSIC_DYNAMIC_COLLECTION_MEMBER",
    "XL6_IR_RUNTIME_XVALUE_TYPED_COLLECTION_METHOD",
    "XL6_IR_RUNTIME_XVALUE_DYNAMIC_COLLECTION_METHOD"
) ) {
    if ( ! $header.Contains($required) ) {
        throw "XLIR header is missing canonical collection-method identity: $required"
    }
}
foreach ( $required in @(
    "xl6IrTypedCollectionSignatureCompatible",
    "xl6IrTypedCollectionRuntimeSignatureCompatible",
    "xl6IrCollectionElementType",
    "xl6IrCollectionElementCompatible",
    "xl6IrCollectionBoxedElementType",
    "xl6IrCollectionMemberRuntimeInstructionCompatible",
    "xl6IrTypedCollectionDynamicTakeResultSupported",
    "xl6IrTypedCollectionMemberOperation",
    "xl6IrDynamicCollectionMemberOperation"
) ) {
    if ( ! $source.Contains($required) ) {
        throw "XLIR source is missing canonical collection-method verification: $required"
    }
}
foreach ( $required in @(
    "xl6IrLowerIntrinsicTypedCollectionMember",
    "xl6IrLowerIntrinsicDynamicCollectionMember",
    "XL6_IR_RUNTIME_XVALUE_TYPED_COLLECTION_METHOD",
    "XL6_IR_RUNTIME_XVALUE_DYNAMIC_COLLECTION_METHOD"
) ) {
    if ( ! $lower.Contains($required) ) {
        throw "XLIR lowering is missing canonical collection-method ABI: $required"
    }
}
foreach ( $required in @(
    "xl6ArrayExtractValueV1",
    "xl6ArrayExtractTrivialV1",
    "xl6IrCgenEmitSharedCollectionRuntime",
    "xl6ArrayResizeDefaultV1",
    "xl6ArrayMethodIndexOfV1",
    "xl6ArrayMethodReverseV1",
    "xlir_xvalue_dynamic_collection_method"
) ) {
    if ( ! $cEmitter.Contains($required) ) {
        throw "XLIR Cgen is missing mechanical collection-method emission: $required"
    }
}
foreach ($required in @('collectionDefaultAbi','xl6IrValueDefaultAbiForType','xl6IrTypedCollectionErrorAbi')) {
    if (!$header.Contains($required) -or !$source.Contains($required) -or !$lower.Contains($required)) {
        throw "Array logical default/effect ABI is not shared by IR and lowering: $required"
    }
}
if ($cEmitter.Contains('xlir_xvalue_typed_collection_take_raw') -or
    $cEmitter.Contains('xlir_xvalue_typed_array_index_of') -or
    $cEmitter.Contains('xlir_xvalue_typed_array_reverse') -or
    $cEmitter.Contains('xlir_xvalue_typed_array_resize_default') -or
    $cEmitter.Contains('xlir_xvalue_typed_array_default')) {
    throw 'Cgen must not reconstruct Array defaults, extraction, find or mutation algorithms'
}
foreach ( $required in @(
    "xlir_typed_collection_methods", "typedCollectionOpName",
    "xvalue-typed-collection-method", "strict-C runtime failed"
) ) {
    if ( ! $collectionMemberGate.Contains($required) ) {
        throw "XLIR typed collection-method quality gate is incomplete: $required"
    }
}
$erasedMemberGate = Read-Utf8 "tools\check_xlir_erased_collection_members.ps1"
foreach ( $required in @(
    "xlir_erased_collection_members", "xlir_erased_generic_collection_members",
    "xlir_erased_collection_member_lifetime", "xlir_erased_collection_member_continuation",
    "runtimeTypeMap.fallback", "boxed erased item", "resultAbiName",
    "typedCollectionOpName", "Get-FileHash", "-Werror"
) ) {
    if ( !$erasedMemberGate.Contains($required) ) {
        throw "XLIR erased collection member gate lacks evidence: $required"
    }
}
if ( !$languageFrontendSuite.Contains("tools\check_xlir_erased_collection_members.ps1") -or
     [regex]::Matches($source, 'static int xl6IrTypedCollectionSignatureMatches\(').Count -ne 1 -or
     [regex]::Matches($cEmitter, 'static int xl6IrCgenEmitSharedCollectionRuntime\(').Count -ne 1 -or
     !(Read-Utf8 "test\cases\xlir_cgen.c").Contains("testErasedCollectionAbi") ) {
    throw "XLIR erased collection contract must have one signature table, one shared emitter, malformed-ABI tests and suite wiring"
}
foreach ( $required in @(
    "cgen_dynamic_container_methods", "12 explicit operations",
    "dynamicCollectionOpName", "xvalue-dynamic-collection-method",
    "consume", "owned", "-Werror"
) ) {
    if ( ! $dynamicCollectionMethodGate.Contains($required) ) {
        throw "XLIR dynamic collection-method quality gate is incomplete: $required"
    }
}
if ( ! $languageFrontendSuite.Contains("tools\check_xlir_dynamic_collection_methods.ps1") ) {
    throw "XLIR dynamic collection-method quality gate is not wired into the language frontend suite"
}

# Runtime callable conversion/call is explicit typed IR.  Cgen may consume only
# the verified signature and owned argument list, and uses public XRT callable
# APIs.  XL6_Function is permitted only as a separately verified extern-C
# physical parameter ABI; it is never the internal dynamic-call representation.
foreach ( $required in @(
    "XL6_IR_RUNTIME_XVALUE_CALLABLE_FROM_CLOSURE",
    "XL6_IR_RUNTIME_XVALUE_DYNAMIC_CALL",
    "XL6_IrDynamicCallArgumentAdmission",
    "XL6_IR_DYNAMIC_CALL_ARGUMENT_ADMISSION_VERIFIED",
    "XL6_IR_DYNAMIC_CALL_ARGUMENT_ADMISSION_UNSUPPORTED_BOX_ABI",
    "XL6_IrCallableErasureAbi",
    "XL6_IR_CALLABLE_ERASURE_OPEN",
    "XL6_IR_CALLABLE_ERASURE_ARRAY_VARIADIC",
    "callableErasureAbi",
    "calleeType fixes the complete closure signature",
    "consumes every remaining"
) ) {
    if ( ! $header.Contains($required) ) {
        throw "XLIR header is missing runtime callable ABI: $required"
    }
}
foreach ( $required in @(
    "xl6IrRuntimeCallableSignatureSupported",
    "xl6IrTypeIsErasedCallable",
    "xl6IrDynamicCallArgumentAdmissionName",
    "unsupported-box-abi",
    "xl6IrCallableErasureAbiName",
    "XLIR runtime callable construction has no exact closure signature and failure ABI",
    "XLIR runtime dynamic call argument has no consumed owned xvalue ABI",
    "XLIR runtime callable operation is not immediately followed by ERROR_TEST",
    "xvalue-callable-from-closure", "xvalue-dynamic-call"
) ) {
    if ( ! $source.Contains($required) ) {
        throw "XLIR verifier/dump is missing runtime callable ABI: $required"
    }
}
foreach ( $required in @(
    "xl6IrLowerIntrinsicXCall", "xl6IrLowerDynamicCall",
    "xl6IrLowerValueInitializerErrorPlan",
    "xl6IrLowerEmitRuntimeErrorEdge",
    "XL6_SEM_CALL_DYNAMIC_FUNCTION",
    "XL6_IR_DYNAMIC_CALL_ARGUMENT_ADMISSION_VERIFIED",
    "unsupported runtime-xvalue boxing ABI",
    "XL6_IR_RUNTIME_XVALUE_CALLABLE_FROM_CLOSURE",
    "XL6_IR_RUNTIME_XVALUE_DYNAMIC_CALL"
) ) {
    if ( ! $lower.Contains($required) ) {
        throw "XLIR lowering is missing runtime callable operations: $required"
    }
}
foreach ( $required in @(
    "xl6IrCgenRuntimeCallableFromClosureAbi",
    "xl6IrCgenRuntimeDynamicCallAbi",
    "xl6IrCgenEmitRuntimeCallableEntry",
    "xl6IrCallableBridgeGet",
    "xl6IrCgenEmitCallableBridgeCleanup",
    "xl6IrCgenEmitCallableResultCleanup",
    "XL6_IR_CALLABLE_BRIDGE_F32", "XL6_IR_CALLABLE_BRIDGE_TIME",
    "xl6ClosureRuntimeV1Get()->Box", "xrtValueInvoke",
    "xrtCallResultSetTake", "xlir_closure_to_xvalue_scalar",
    "xlir_xvalue_dynamic_call", "xrtErrorCause(callError)",
    "xrtSetErrorTake(projected)"
) ) {
    if ( ! $cEmitter.Contains($required) ) {
        throw "XLIR C emitter is missing mechanical runtime callable ABI: $required"
    }
}
foreach ( $required in @(
    "cgen_function_bare_call.xl", "callableErasureAbiName",
    "array-variadic", "xrtValueArrayAppend(", "deterministic", "-Werror"
) ) {
    if ( ! $openFunctionGate.Contains($required) ) {
        throw "XLIR open-function callable quality gate is incomplete: $required"
    }
}
if ( ! $languageFrontendSuite.Contains("tools\check_xlir_open_function_abi.ps1") ) {
    throw "XLIR open-function callable quality gate is not wired into the language frontend suite"
}
foreach ( $required in @(
    "implicitConversionErrorAbi", "xl6IrNodeRequiresThrowExitPlan",
    "xl6IrNodeRequiresAnyExitPlan", "xl6IrTypeCanBoxRuntimeXValue",
    "XL6_IR_LOOP_ITEM_CLOSURE_COPY"
) ) {
    if ( ! $header.Contains($required) ) {
        throw "XLIR callable-container public semantic contract is incomplete: $required"
    }
}
foreach ( $required in @(
    "xl6IrNormalizeExpressionCallKinds", "implicitConversionErrorAbi",
    "xl6IrTypeCanBoxRuntimeXValue", "XL6_IR_LOOP_ITEM_CLOSURE_COPY",
    'return "closure-copy"', "xl6IrNodeRequiresThrowExitPlan"
) ) {
    if ( ! $source.Contains($required) ) {
        throw "XLIR callable-container Typed/verifier contract is incomplete: $required"
    }
}
foreach ( $required in @(
    "implicitConversionErrorAbi", "xl6IrNodeRequiresThrowExitPlan",
    "xl6IrNodeRequiresAnyExitPlan", "xl6IrTypeCanBoxRuntimeXValue",
    "XL6_IR_LOOP_ITEM_CLOSURE_COPY",
    "XL6_IR_RUNTIME_XVALUE_CALLABLE_FROM_CLOSURE"
) ) {
    if ( ! $lower.Contains($required) ) {
        throw "XLIR callable-container lowering contract is incomplete: $required"
    }
}
foreach ( $required in @(
    "cgen_typed_container_functions.xl", "implicitConversionErrorAbiName",
    "xvalue-callable-from-closure", "xvalue-project-closure-copy",
    "closure-copy", "function-value", "targetSymbol",
    "SequenceEqual", "strict-C/runtime gate failed",
    "strict-C/runtime gate failed"
) ) {
    if ( ! $typedCallableContainerGate.Contains($required) ) {
        throw "XLIR callable-container quality gate is incomplete: $required"
    }
}
if ( ! $languageFrontendSuite.Contains("tools\check_xlir_typed_callable_container_abi.ps1") ) {
    throw "XLIR callable-container quality gate is not wired into the language frontend suite"
}
foreach ( $required in @(
    "XL6_IR_RUNTIME_XVALUE_PROJECT_NULLABLE_UNION_COPY",
    "XL6_IR_RUNTIME_XVALUE_PROJECT_NULLABLE_XVALUE_RETAIN",
    "XL6_IR_RUNTIME_XVALUE_PROJECT_XVALUE_RETAIN"
) ) {
    if ( ! $header.Contains($required) ) {
        throw "XLIR safe-index nominal projection header contract is incomplete: $required"
    }
}
if ( $header.Contains("XL6_IR_RUNTIME_XVALUE_PROJECT_NULLABLE_NATIVE_HANDLE_BORROW") ) {
    throw "XLIR must not collapse nullable nominal native presence into a pointer sentinel"
}
foreach ( $required in @(
    "xl6IrNullableCAbiForElement",
    "A pure-native class is a nominal value",
    "XLIR xvalue nullable nominal projection has no exact source"
) ) {
    if ( ! $source.Contains($required) ) {
        throw "XLIR safe-index nullable nominal Typed/verifier contract is incomplete: $required"
    }
}
foreach ( $required in @(
    "xl6IrLowerProjectBorrowedIndexResult",
    "XL6_IR_RUNTIME_XVALUE_PROJECT_NULLABLE_UNION_COPY",
    "XL6_IR_RUNTIME_XVALUE_PROJECT_NULLABLE_XVALUE_RETAIN",
    "allowing the lookup itself to"
) ) {
    if ( ! $lower.Contains($required) ) {
        throw "XLIR safe-index exact projection lowering contract is incomplete: $required"
    }
}
foreach ( $required in @(
    "xlir_xvalue_project_nullable_retain",
    "xlir_xvalue_type_id(source) != exactTypeId",
    "instruction->payloadType",
    "XL6_IR_RUNTIME_XVALUE_PROJECT_NULLABLE_XVALUE_RETAIN"
) ) {
    if ( ! $cEmitter.Contains($required) ) {
        throw "XLIR safe-index exact projection C emitter contract is incomplete: $required"
    }
}
foreach ( $required in @(
    "cgen_typed_container_safe_index_objects_functions.xl",
    "xvalue-project-nullable-xvalue-retain",
    "xvalue-project-nullable-union-copy",
    "xvalue-project-nullable-closure-copy",
    "nullable native class did not preserve presence",
    "payloadType", "typeValueId", "SequenceEqual",
    "strict-C/runtime gate failed"
) ) {
    if ( ! $typedSafeIndexNominalGate.Contains($required) ) {
        throw "XLIR typed safe-index nominal quality gate is incomplete: $required"
    }
}
if ( ! $languageFrontendSuite.Contains("tools\check_xlir_typed_container_safe_index_objects_functions_abi.ps1") ) {
    throw "XLIR typed safe-index nominal quality gate is not wired into the language frontend suite"
}
foreach ( $required in @(
    "XL6_IR_RUNTIME_XVALUE_PROJECT_XVALUE_RETAIN",
    "Non-null counterpart for an xvalue-represented nominal result"
) ) {
    if ( ! $header.Contains($required) ) {
        throw "XLIR non-null nominal xvalue projection header contract is incomplete: $required"
    }
}
foreach ( $required in @(
    "XLIR xvalue non-null nominal projection has no exact source",
    'return "xvalue-project-xvalue-retain"'
) ) {
    if ( ! $source.Contains($required) ) {
        throw "XLIR non-null nominal xvalue projection verifier contract is incomplete: $required"
    }
}
foreach ( $required in @(
    "xl6IrLowerProjectBorrowedXValueAsXValue",
    "XL6_IR_RUNTIME_XVALUE_PROJECT_XVALUE_RETAIN",
    "XLIR borrowed index result has no exact non-null projection ABI"
) ) {
    if ( ! $lower.Contains($required) ) {
        throw "XLIR non-null nominal xvalue projection lowering contract is incomplete: $required"
    }
}
foreach ( $required in @(
    "xlir_xvalue_project_retain",
    "source == NULL || xrtValueType(source) == 0",
    "XL6_IR_RUNTIME_XVALUE_PROJECT_XVALUE_RETAIN"
) ) {
    if ( ! $cEmitter.Contains($required) ) {
        throw "XLIR non-null nominal xvalue projection C emitter contract is incomplete: $required"
    }
}
foreach ( $required in @(
    "cgen_typed_container_index_assign_objects_functions.xl",
    "implicitConversionErrorAbiName", "unique THROW ExitPlan",
    "xvalue-callable-from-closure", "xvalue-project-xvalue-retain",
    "runtimeIdentityType", "SequenceEqual",
    "strict-C/runtime gate failed"
) ) {
    if ( ! $typedIndexAssignNominalGate.Contains($required) ) {
        throw "XLIR typed nominal index-assignment quality gate is incomplete: $required"
    }
}
if ( ! $languageFrontendSuite.Contains("tools\check_xlir_typed_container_index_assign_objects_functions_abi.ps1") ) {
    throw "XLIR typed nominal index-assignment quality gate is not wired into the language frontend suite"
}
foreach ( $required in @(
    "XL6_IR_RUNTIME_XVALUE_PROJECT_SCALAR_COPY",
    "copies one verified", "trivial scalar payload"
) ) {
    if ( ! $header.Contains($required) ) {
        throw "XLIR xvalue scalar projection header contract is incomplete: $required"
    }
}
foreach ( $required in @(
    "XLIR xvalue scalar projection has no exact non-null source",
    'return "xvalue-project-scalar-copy"'
) ) {
    if ( ! $source.Contains($required) ) {
        throw "XLIR xvalue scalar projection verifier contract is incomplete: $required"
    }
}
foreach ( $required in @(
    "xl6IrLowerProjectBorrowedXValueAsScalar",
    "XL6_IR_RUNTIME_XVALUE_PROJECT_SCALAR_COPY"
) ) {
    if ( ! $lower.Contains($required) ) {
        throw "XLIR xvalue scalar projection lowering contract is incomplete: $required"
    }
}
foreach ( $required in @(
    "XL6_IR_RUNTIME_XVALUE_PROJECT_SCALAR_COPY",
    "xrtValueGetBool", "xrtValueGetInt", "xrtValueGetUInt",
    "xrtValueGetFloat", "xrtValueGetTime"
) ) {
    if ( ! $cEmitter.Contains($required) ) {
        throw "XLIR xvalue scalar projection C emitter contract is incomplete: $required"
    }
}
foreach ( $required in @(
    "cgen_typed_container_index_assign.xl",
    "xvalue-index-get-xvalue-key", "xvalue-project-scalar-copy",
    "payloadType", "canonical borrowed carrier ABI", "SequenceEqual",
    "strict-C/runtime gate failed"
) ) {
    if ( ! $dynamicKeyScalarProjectionGate.Contains($required) ) {
        throw "XLIR dynamic-key scalar projection quality gate is incomplete: $required"
    }
}
if ( ! $languageFrontendSuite.Contains("tools\check_xlir_dynamic_key_scalar_projection.ps1") ) {
    throw "XLIR dynamic-key scalar projection quality gate is not wired into the language frontend suite"
}
foreach ( $required in @(
    "XL6_IR_RUNTIME_XVALUE_PROJECT_RAW_POINTER_BORROW",
    "pointer-sentinel static result", "Generic point parameters"
) ) {
    if ( ! $header.Contains($required) ) {
        throw "XLIR xvalue raw-pointer projection header contract is incomplete: $required"
    }
}
foreach ( $required in @(
    "XLIR xvalue raw-pointer projection has no exact source",
    'return "xvalue-project-raw-pointer-borrow"'
) ) {
    if ( ! $source.Contains($required) ) {
        throw "XLIR xvalue raw-pointer projection verifier contract is incomplete: $required"
    }
}
foreach ( $required in @(
    "xl6IrLowerProjectBorrowedXValueAsRawPointer",
    "XL6_IR_RUNTIME_XVALUE_PROJECT_RAW_POINTER_BORROW"
) ) {
    if ( ! $lower.Contains($required) ) {
        throw "XLIR xvalue raw-pointer projection lowering contract is incomplete: $required"
    }
}
foreach ( $required in @(
    "XL6_IR_RUNTIME_XVALUE_PROJECT_RAW_POINTER_BORROW",
    "xrtValueGetPointer", "needsXValueRawPointerProjectionRuntime"
) ) {
    if ( ! $cEmitter.Contains($required) ) {
        throw "XLIR xvalue raw-pointer projection C emitter contract is incomplete: $required"
    }
}
foreach ( $required in @(
    "cgen_typed_point.xl", "xvalue-array-i64-set-new",
    "xvalue-raw-pointer-borrow", "xvalue-project-raw-pointer-borrow",
    "payloadType", "canonical borrowed carrier ABI", "SequenceEqual",
    "strict-C/runtime gate failed"
) ) {
    if ( ! $typedPointProjectionGate.Contains($required) ) {
        throw "XLIR typed point projection quality gate is incomplete: $required"
    }
}
if ( ! $languageFrontendSuite.Contains("tools\check_xlir_typed_point_projection_abi.ps1") ) {
    throw "XLIR typed point projection quality gate is not wired into the language frontend suite"
}
foreach ( $required in @(
    "XL6_IR_RUNTIME_XVALUE_PROJECT_STRING_COPY",
    "independently owned managed-string copy",
    "payloadType"
) ) {
    if ( ! $header.Contains($required) ) {
        throw "XLIR xvalue string projection header contract is incomplete: $required"
    }
}
foreach ( $required in @(
    "XL6_IR_RUNTIME_XVALUE_PROJECT_STRING_COPY",
    "XLIR xvalue string projection has no exact non-null source, static payload and owned-copy ABI",
    'return "xvalue-project-string-copy"'
) ) {
    if ( ! $source.Contains($required) ) {
        throw "XLIR xvalue string projection verifier contract is incomplete: $required"
    }
}
foreach ( $required in @(
    "xl6IrLowerProjectBorrowedXValueAsString",
    "xl6IrLowerProjectBorrowedXValueAsExactType",
    "The exact string projection is already the owned result",
    "XLIR may-raise constructor has no result-carrier or propagation cleanup ABI",
    "XLIR call completion has no explicit outcome and propagation CFG",
    "XL6_IR_INSTR_ERROR_TEST", "XL6_IR_INSTR_MOVE"
) ) {
    if ( ! $lower.Contains($required) ) {
        throw "XLIR scalar bridge and may-raise constructor lowering contract is incomplete: $required"
    }
}
foreach ( $required in @(
    "XL6_IR_RUNTIME_XVALUE_PROJECT_STRING_COPY",
    "needsXValueStringProjectionRuntime",
    "xlir_xvalue_project_string_copy",
    "xrtValueGetString", "xrtStrDupN"
) ) {
    if ( ! $cEmitter.Contains($required) ) {
        throw "XLIR xvalue string projection C emitter contract is incomplete: $required"
    }
}
foreach ( $required in @(
    "hir_xvalue_scalar_bridge_matrix.xl",
    'runtimeOpName -eq "xvalue-project-string-copy"',
    'runtimeOpName -eq "xvalue-type-id"',
    'runtimeOpName -eq "integer-convert-checked"',
    'errorAbiName -ne "may-raise"',
    'kindName -eq "error-test"', 'kindName -eq "move"',
    "SequenceEqual", "strict-C/runtime gate failed",
    "strict-C/runtime gate failed"
) ) {
    if ( ! $xvalueScalarBridgeGate.Contains($required) ) {
        throw "XLIR xvalue scalar-bridge quality gate is incomplete: $required"
    }
}
if ( ! $languageFrontendSuite.Contains("tools\check_xlir_xvalue_scalar_bridge_abi.ps1") ) {
    throw "XLIR xvalue scalar-bridge quality gate is not wired into the language frontend suite"
}
if ( ! $runtimeMatrix.Contains("xvalue scalar bridge, flow projection, and may-raise constructor ABI") -or
     ! $runtimeMatrix.Contains("hir_xvalue_scalar_bridge_matrix.xl") ) {
    throw "XLIR xvalue scalar-bridge fixture is not wired into runtime-matrix coverage"
}
foreach ( $forbidden in @("xl6XrtCallableCreate", "xl6XrtValueInvoke", "xl6CallResult") ) {
    if ( $cEmitter.Contains($forbidden) ) {
        throw "XLIR C emitter must not use legacy callable compatibility ABI: $forbidden"
    }
}
foreach ( $required in @(
    "XL6_IrCallParameterPhysicalAbi",
    "XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT",
    "XL6_IR_CALL_PARAMETER_PHYSICAL_LANGUAGE_FUNCTION_VALUE",
    "XL6_IrCallParameterConstraintAbi",
    "XL6_IR_CALL_PARAMETER_CONSTRAINT_RAW_CALLBACK_DIRECT_THUNK",
    "parameterPhysicalAbis", "parameterConstraintAbis",
    "xl6IrLanguageFunctionValueSignatureSupported"
) ) {
    if ( ! $header.Contains($required) ) {
        throw "XLIR extern function-value physical ABI header is incomplete: $required"
    }
}
foreach ( $required in @(
    "XLIR extern function parameter cannot use the direct closure physical ABI",
    "XLIR extern function parameter has no verified XL6_Function physical ABI",
    "language-function-value", "raw-callback-direct-thunk",
    "parameterPhysicalAbis", "parameterConstraintAbis",
    "XLIR constrained raw callback operand does not terminate in a verified direct thunk or constrained caller parameter"
) ) {
    if ( ! $source.Contains($required) ) {
        throw "XLIR extern function-value physical ABI verifier is incomplete: $required"
    }
}
foreach ( $required in @(
    "XLIR c_callback extern parameter has no verified raw C callback signature ABI",
    "XLIR language function extern parameter has no verified XL6_Function physical ABI",
    "XL6_IR_CALL_PARAMETER_PHYSICAL_LANGUAGE_FUNCTION_VALUE",
    "XL6_IR_CALL_PARAMETER_CONSTRAINT_RAW_CALLBACK_DIRECT_THUNK",
    "target->parameterPhysicalAbis", "target->parameterConstraintAbis"
) ) {
    if ( ! $lower.Contains($required) ) {
        throw "XLIR extern function-value physical ABI lowering is incomplete: $required"
    }
}
foreach ( $required in @(
    "xl6IrCgenLanguageFunctionValueSignatureAbi",
    "xlir_language_function_invoke_t", "xlir_language_function_value_t",
    "xrtValueGetString", "xrtStrDupN", "xrtValueString",
    "has no verified XL6_Function adapter ABI"
) ) {
    if ( ! $cEmitter.Contains($required) ) {
        throw "XLIR extern function-value physical ABI C boundary is incomplete: $required"
    }
}
foreach ( $required in @(
    "xlir_foreign_function_value_abi.xl", "language-function-value",
    "raw-pointer-null-convert", "typedef struct XL6_Function",
    "xlir_language_function_invoke_t", "strict-C/runtime gate failed"
) ) {
    if ( ! $foreignFunctionValueGate.Contains($required) ) {
        throw "XLIR extern function-value physical ABI quality gate is incomplete: $required"
    }
}
if ( ! $languageFrontendSuite.Contains(
        "tools\check_xlir_foreign_function_value_abi.ps1") ) {
    throw "XLIR extern function-value physical ABI gate is not wired into the language frontend suite"
}
foreach ( $required in @(
    "cgen_extern_function_value", "xlir_xcall_scalar_effect",
    "xvalue-callable-from-closure", "xvalue-dynamic-call",
    "consumed argument", "projected callable error causes", "ERROR_TEST",
    "xl6ClosureRuntimeV1Get()->Box", "-Werror", "runtime failed"
) ) {
    if ( ! $xcallGate.Contains($required) ) {
        throw "XLIR xcall quality gate is incomplete: $required"
    }
}
foreach ( $required in @(
    "xlir_async_future_direct_call_catch",
    "xlir_closure_future_capture",
    "xlir_nullable_async_continuation",
    "hir_flow_value_ownership_matrix",
    "xvalue-int-map-new", "xvalue-object-get-literal", "protocol-call",
    "one direct callback per canonical suspension",
    "full supported XLIR async inventory", "non-analyzed machine",
    "orphaned or ambiguous suspension",
    "fell back to legacy continuation rendering",
    "byte-for-byte deterministic C",
    "catch recovery", "closure/future ownership",
    "tagged nullable/null construction", "protocol dispatch",
    "-Werror", "runtime semantics"
) ) {
    if ( ! $asyncMachineAdmissionGate.Contains($required) ) {
        throw "XLIR async machine admission quality gate is incomplete: $required"
    }
}
foreach ( $required in @(
    "XLIR verified async machine has no admitted direct C program ABI",
    "An analyzed machine is the canonical executable async IR",
    "unsupported instruction/frame ABI must be added to verified XLIR",
    "xl6IrCgenEmitClosureMakeInstruction",
    "xl6IrCgenEmitClosureCallInstruction",
    "xl6IrCgenNullInstructionAbi", "xl6IrCgenEmitNullInstruction",
    "xl6IrCgenProtocolCallAbi", "xl6IrCgenEmitDispatchCallInstruction",
    "xl6IrCgenEmitProtocolDispatcherPrototypes",
    "XL6_IR_RUNTIME_XVALUE_INT_MAP_SET_I64_NEW",
    "XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL",
    "instruction->runtimeOp == XL6_IR_RUNTIME_STRING_COPY",
    "XL6_IR_ASYNC_FRAME_DESTROY_RELEASE_CLOSURE",
    "XL6_IR_ASYNC_FRAME_DESTROY_DESTROY_VALUE_ITERATOR",
    "XL6_IR_ASYNC_RESULT_COPY_TAGGED"
) ) {
    if ( ! $cEmitter.Contains($required) ) {
        throw "XLIR direct async machine Cgen is not fail-closed or lacks a verified ABI: $required"
    }
}
foreach ( $required in @(
    "protocol_declaration.xl", "declaration-only protocols",
    "protocolDispatchSlotCount", "protocol-call",
    "xlir_pdispatch_", "SequenceEqual",
    "byte-for-byte deterministic", "strict-C/runtime gate failed"
) ) {
    if ( ! $protocolDispatchModelGate.Contains($required) ) {
        throw "XLIR protocol declaration/dispatch quality gate is incomplete: $required"
    }
}
foreach ( $required in @(
    "xl6IrLowerOwnedStorageValue",
    "A declaration or assignment creates a storage owner",
    "XLIR value declaration initializer does not match its storage ABI",
    "XLIR managed storage value has no explicit owned transfer ABI"
) ) {
    if ( ! $lower.Contains($required) ) {
        throw "XLIR declaration ownership lowering is incomplete: $required"
    }
}
foreach ( $forbidden in @(
    "xl6IrCgenAsyncContinuation", "xl6IrCgenAsyncResumeOpForSlot",
    "xl6IrCgenEmitAsyncFutureChain", "xl6IrCgenEmitAsyncScalarCaptureChain",
    "xl6IrCgenEmitAsyncStringCaptureChain", "xl6IrCgenEmitAsyncXValueCaptureChain",
    "xlir_async_chain_frame_", "xlir_async_capture_chain_frame_",
    "xlir_async_string_capture_chain_", "xlir_async_xvalue_capture_chain_",
    "xlir_async_catch_default_"
) ) {
    if ( $cEmitter.Contains($forbidden) ) {
        throw "XLIR Cgen still contains a second async continuation renderer: $forbidden"
    }
}
if (!$header.Contains('XL6_IrAsyncCompletionAbi asyncCompletionAbi;') -or
    !$source.Contains('XLIR async target has no verified completion cleanup barrier') -or
    !$cEmitter.Contains('xl6ContinuationFrameFinishV1(frame, retired, &cancelled)') -or
    !(Read-Utf8 'test/suites/03_core_function_abi.bat').Contains('check_xlir_async_worker_completion.ps1') -or
    !(Read-Utf8 'test/cases/xlir_async_worker_completion_contract.c').Contains('ir.asyncMachineCount == 6') -or
    !(Read-Utf8 'test/cases/xlir_async_worker_completion_runtime.c').Contains('xrtMemDebugFailAfter(0)')) {
    throw 'No-await queued frames require verified pre-publication retirement/cancellation and real failure regression'
}
$readyLauncherIndex = $cEmitter.IndexOf(
    "target->asyncDispatch == XL6_IR_ASYNC_DISPATCH_XRT_READY_FUTURE")
$machineLauncherIndex = $cEmitter.IndexOf("if ( machine != NULL )", $readyLauncherIndex)
$missingMachineIndex = $cEmitter.IndexOf(
    "XLIR scheduled async target has no verified ready-queue machine", $machineLauncherIndex)
if ( $readyLauncherIndex -lt 0 -or $machineLauncherIndex -le $readyLauncherIndex -or
     $missingMachineIndex -le $machineLauncherIndex ) {
    throw "XLIR async launcher must execute an exact verified frame or reject, without a task-pool fallback"
}

foreach ( $required in @(
    "xl6IrLowerBlockTerminated(ctx) ) {",
    "Do not append", "unreachable statements"
) ) {
    if ( ! $lower.Contains($required) ) {
        throw "XLIR structured block lowering does not stop after a terminator: $required"
    }
}

# Callable emission is an exact Lowered XLIR graph closure.  Module/export/
# entry roots and direct, closure, dispatch and async edges are computed and
# independently reverified before Cgen sees the module.  Unused declarations
# remain inspectable IR facts but are not C artifacts.
foreach ( $required in @(
    "XL6_IrReachability", "XL6_IR_REACHABILITY_UNREACHABLE",
    "XL6_IR_REACHABILITY_ROOT", "XL6_IR_REACHABILITY_TRANSITIVE",
    "XL6_IrFunction", "XL6_IrCallTarget", "XL6_IrClosureTarget",
    "xl6IrRefreshReachability"
) ) {
    if ( ! $header.Contains($required) ) {
        throw "XLIR header is missing canonical callable reachability: $required"
    }
}
foreach ( $required in @(
    "xl6IrBuildReachabilityFacts", "xl6IrVerifyReachability",
    "XLIR function reachability is not the exact root closure",
    "XLIR call-target reachability is not the exact root closure",
    "XLIR closure-target reachability is not the exact root closure",
    '\"reachability\":\"%s\"', '\"closureTargets\": ['
) ) {
    if ( ! $source.Contains($required) ) {
        throw "XLIR verifier/dump is missing canonical callable reachability: $required"
    }
}
foreach ( $required in @(
    "xl6IrRefreshReachability", "canonical callable reachability"
) ) {
    if ( ! $lower.Contains($required) ) {
        throw "XLIR lowering is missing callable reachability closure: $required"
    }
}
foreach ( $required in @(
    "target->reachability != XL6_IR_REACHABILITY_UNREACHABLE",
    "function->reachability == XL6_IR_REACHABILITY_UNREACHABLE",
    "closure->reachability == XL6_IR_REACHABILITY_UNREACHABLE",
    "xl6IrCgenInstructionIsReachable"
) ) {
    if ( ! $cEmitter.Contains($required) ) {
        throw "XLIR C emitter does not consume canonical callable reachability: $required"
    }
}
if ( $cEmitter.Contains("xl6IrCgenTargetNeedsBody") ) {
    throw "XLIR C emitter still reconstructs callable reachability from instructions"
}
foreach ( $required in @(
    "xlir_callable_reachability.xl", "xlir_reachability_dead_external",
    "xl6TextCountV1", "virtual-call", "protocol-call",
    "xlir_async_start_", "-Werror", "generated-C runtime failed"
) ) {
    if ( ! $reachabilityGate.Contains($required) ) {
        throw "XLIR callable reachability quality gate is incomplete: $required"
    }
}

# Null is contextual typed XLIR, not a typeless CONST payload or a Cgen-side
# guess.  The type representation, construction/test instructions, verifier,
# lowering boundary, dumps, and emitter must evolve as one contract.
foreach ( $required in @(
    "XL6_IrNullAbi",
    "XL6_IR_NULL_ABI_NONE",
    "XL6_IR_NULL_ABI_POINTER_SENTINEL",
    "XL6_IR_NULL_ABI_XVALUE_SENTINEL",
    "XL6_IR_NULL_ABI_TAGGED_VALUE",
    "XL6_IR_INSTR_NULL",
    "XL6_IR_INSTR_NULL_TEST",
    "nullAbi",
    "xl6IrTypeFinalizeRepresentation"
) ) {
    if ( ! $header.Contains($required) ) {
        throw "XLIR header is missing contextual null ABI: $required"
    }
}
foreach ( $required in @(
    "xl6IrNullableAcceptsValue",
    "xl6IrNullableCAbiForElement",
    "XLIR null construction has no exact sentinel ABI",
    "XLIR null test has no exact sentinel operand ABI",
    '\"nullAbi\":%d',
    'ownership=%d null=%d'
) ) {
    if ( ! $source.Contains($required) ) {
        throw "XLIR verifier/dump is missing contextual null ABI: $required"
    }
}
foreach ( $required in @(
    "xl6IrLowerNodeAsType",
    'Source `null` deliberately has no standalone XLIR value',
    "XLIR tagged nullable value has no explicit product ABI",
    "XL6_IR_INSTR_NULL_TEST"
) ) {
    if ( ! $lower.Contains($required) ) {
        throw "XLIR lowering is missing contextual null ABI: $required"
    }
}
foreach ( $required in @(
    "XL6_IR_INSTR_NULL",
    "XL6_IR_INSTR_NULL_TEST",
    "xrtValueRetain(xrtValueNull())",
    "xrtValueType(v%u) == 0",
    "XLIR C emitter null ABI is unresolved"
) ) {
    if ( ! $cEmitter.Contains($required) ) {
        throw "XLIR C emitter is missing contextual null ABI: $required"
    }
}
foreach ( $required in @(
    "XL6_IR_INITIALIZER_TAGGED_NULL",
    "XL6_IR_RUNTIME_XVALUE_TAGGED_NULLABLE_BOX",
    "XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_TAGGED_NULLABLE"
) ) {
    if ( ! $header.Contains($required) ) {
        throw "XLIR header is missing tagged nullable class-field ABI: $required"
    }
}
foreach ( $required in @(
    "XLIR tagged nullable dynamic box has no exact scalar carrier ABI",
    "XLIR runtime tagged nullable class field access has no exact ABI",
    "xvalue-tagged-nullable-box",
    "xvalue-object-get-literal-tagged-nullable",
    "tagged-null"
) ) {
    if ( ! $source.Contains($required) ) {
        throw "XLIR verifier/dump is missing tagged nullable class-field ABI: $required"
    }
}
foreach ( $required in @(
    "xl6IrLowerRuntimeXValueMemberGetTaggedNullable",
    "XL6_IR_RUNTIME_XVALUE_TAGGED_NULLABLE_BOX",
    "XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_TAGGED_NULLABLE"
) ) {
    if ( ! $lower.Contains($required) ) {
        throw "XLIR lowering is missing tagged nullable class-field ABI: $required"
    }
}
foreach ( $required in @(
    "xl6IrCgenRuntimeTaggedNullableXValueAbi",
    "xl6IrCgenEmitTaggedNullableXValueRuntime",
    "xlir_xvalue_box_",
    "xlir_xvalue_object_get_literal_"
) ) {
    if ( ! $cEmitter.Contains($required) ) {
        throw "XLIR C emitter is missing mechanical tagged nullable class-field ABI: $required"
    }
}

# Cgen may recover an IR node solely to attach its already-materialized source
# span to a diagnostic.  It must never use AST tags or source fields to decide
# generated C semantics: all executable choices come from verified XLIR ABI.
foreach ( $forbiddenCgenSourceSemantic in @(
    "XL6_AST_",
    "->sourceKind",
    ".sourceKind"
) ) {
    if ( $cEmitter.Contains($forbiddenCgenSourceSemantic) ) {
        throw "XLIR C emitter still reads source semantics: $forbiddenCgenSourceSemantic"
    }
}

foreach ( $required in @(
    "XL6_IR_STAGE_TYPED",
    "XL6_IR_STAGE_LOWERED",
    "XL6_IrInstructionKind",
    "XL6_IrOpcode",
    "XL6_IrCAbi",
    "XL6_IrRuntimeOp",
    "XL6_IR_OPCODE_NULL_COALESCE",
    "XL6_IR_OPCODE_POW",
    "XL6_IR_OPCODE_APPROX",
    "XL6_IrOwnershipAbi",
    "XL6_IR_FUNCTION_MODULE_INIT",
    "XL6_IrErrorAbi",
    "XL6_IR_ERROR_ABI_MAY_RAISE",
    "XL6_IrStorage",
    "XL6_IrClassLayout",
    "XL6_IrClassField",
    "XL6_IrClassStaticField",
    "XL6_IrClassDispatchSlot",
    "XL6_IrClassDispatchImpl",
    "XL6_IrProtocolDispatchSlot",
    "XL6_IrProtocolDispatchImpl",
    "XL6_IR_INSTR_PROTOCOL_CALL",
    "runtimeTypeTag",
    "baseDeclaration",
    "inheritedFieldCount",
    "XL6_IrExitPlan",
    "XL6_IrCleanupAction",
    "XL6_IrTypePool",
    "XL6_IrNodeKind",
    "XL6_IrBlock",
    "XL6_IrFunction",
    "XL6_IrCallTarget",
    "XL6_IrGenericSpecialization",
    "XL6_IrParameterAbi",
    "XL6_IR_PARAMETER_ABI_BORROW",
    "XL6_IrExport",
    "XL6_IrExportReturnAbi",
    "XL6_IR_EXPORT_RETURN_OWNED",
    "XL6_IR_CALL_TARGET_EXTERN_C",
    "XL6_IR_CALL_TARGET_NATIVE_FACTORY",
    "XL6_IrModuleAbi",
    "XL6_IR_MODULE_ENTRY_XLANG_MAIN",
    "XL6_IR_C_ABI_RUNTIME_XVALUE_PTR",
    "XL6_IR_TYPE_PROTOCOL",
    "XL6_IR_C_ABI_NATIVE_HANDLE",
    "XL6_IR_C_ABI_RAW_POINTER",
    "XL6_IR_C_ABI_RUNTIME_ERROR_PTR",
    "XL6_IR_C_ABI_RUNTIME_FUTURE_PTR",
    "XL6_IR_C_ABI_CLOSURE_PTR",
    "XL6_IR_C_ABI_RUNTIME_CELL_PTR",
    "XL6_IR_C_ABI_U64",
    "XL6_IR_TYPE_CELL",
    "XL6_IrClosureTarget",
    "XL6_IrClosureCapture",
    "XL6_IrClosureCaptureAbi",
    "XL6_IR_CLOSURE_CAPTURE_COPY_STRING",
    "XL6_IR_CLOSURE_CAPTURE_RETAIN_CELL",
    "cellType",
    "storageSymbol",
    "XL6_IR_STORAGE_CAPTURE",
    "XL6_IR_INSTR_RETAIN",
    "XL6_IR_INSTR_CELL_NEW",
    "XL6_IR_INSTR_CELL_INIT",
    "XL6_IR_INSTR_CELL_LOAD",
    "XL6_IR_INSTR_CELL_STORE",
    "XL6_IR_INSTR_CELL_RETAIN",
    "XL6_IR_INSTR_CELL_RELEASE",
    "XL6_IR_CLEANUP_CELL_RELEASE",
    "XL6_IR_RUNTIME_XVALUE_I64",
    "XL6_IR_RUNTIME_XVALUE_STRING_COPY",
    "XL6_IR_RUNTIME_XVALUE_RETAIN",
    "XL6_IR_RUNTIME_XVALUE_COUNT",
    "XL6_IR_RUNTIME_XVALUE_CLASS_IS",
    "XL6_IR_RUNTIME_XVALUE_CLASS_AS",
    "XL6_IR_RUNTIME_XVALUE_ARRAY_NEW",
    "XL6_IR_RUNTIME_XVALUE_ARRAY_AT_I64",
    "XL6_IR_RUNTIME_XVALUE_ARRAY_AT_I64_INTEGER",
    "XL6_IR_RUNTIME_XVALUE_ARRAY_AT_I64_BOOL",
    "XL6_IR_RUNTIME_XVALUE_ARRAY_AT_I64_F64",
    "XL6_IR_RUNTIME_XVALUE_ARRAY_AT_I64_STRING_COPY",
    "XL6_IR_RUNTIME_XVALUE_OBJECT_NEW",
    "XL6_IR_RUNTIME_XVALUE_OBJECT_SET_LITERAL_NEW",
    "XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL",
    "XL6_IR_RUNTIME_XVALUE_OBJECT_GET_STRING",
    "XL6_IR_RUNTIME_XVALUE_OBJECT_SET_STRING_NEW",
    "XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_I64",
    "XL6_IR_RUNTIME_XVALUE_NATIVE_HANDLE_BORROW",
    "XL6_IR_RUNTIME_XVALUE_ARRAY_SET_I64_NEW",
    "XL6_IR_RUNTIME_XVALUE_INT_MAP_NEW",
    "XL6_IR_RUNTIME_XVALUE_INT_MAP_SET_I64_NEW",
    "XL6_IR_RUNTIME_XVALUE_SET_NEW",
    "XL6_IR_RUNTIME_XVALUE_SET_ADD_NEW",
    "XL6_IR_INSTR_TRAP_IF_FALSE",
    "XL6_IR_INSTR_ERROR_RAISE",
    "XL6_IR_INSTR_ERROR_TEST",
    "XL6_IR_INSTR_ERROR_TAKE",
    "XL6_IR_INSTR_ERROR_SET_TAKE",
    "XL6_IR_INSTR_ERROR_CLEAR",
    "XL6_IR_INSTR_AWAIT",
    "XL6_IR_INSTR_ASYNC_START",
    "XL6_IR_INSTR_CLOSURE_MAKE",
    "XL6_IR_INSTR_CLOSURE_CALL",
    "XL6_IR_FUNCTION_ASYNC_WORKER",
    "XL6_IR_CALL_TARGET_INTERNAL_ASYNC",
    "XL6_IrAsyncDispatchKind",
    "XL6_IrAsyncCaptureAbi",
    "XL6_IR_ASYNC_CAPTURE_COPY_VALUE",
    "XL6_IR_ASYNC_FRAME_TRANSFER_RETAIN_FUTURE",
    "XL6_IR_ASYNC_DISPATCH_FIXED_READY_QUEUE",
    "XL6_IrNode",
    "XL6_IrModule"
) ) {
    if ( ! $header.Contains($required) ) {
        throw "XLIR header is missing required canonical abstraction: $required"
    }
}

foreach ( $legacyAsyncField in @(
    "XL6_IR_ASYNC_DISPATCH_XRT_FUTURE_FORWARD",
    "XL6_IR_ASYNC_DISPATCH_XRT_FUTURE_THEN_SCALAR",
    "XL6_IR_ASYNC_CONTINUATION_BINARY_SCALAR",
    "XL6_IR_ASYNC_CONTINUATION_BINARY_STRING",
    "XL6_IR_ASYNC_CONTINUATION_BINARY_XVALUE",
    "XL6_IR_ASYNC_CONTINUATION_SCALAR_CAPTURE_CHAIN",
    "XL6_IR_ASYNC_CONTINUATION_STRING_CAPTURE_CHAIN",
    "XL6_IR_ASYNC_CONTINUATION_XVALUE_CAPTURE_CHAIN",
    "XL6_IR_ASYNC_CONTINUATION_BINARY_CARRIED_SCALAR",
    "XL6_IR_ASYNC_CONTINUATION_BINARY_CARRIED_STRING",
    "XL6_IrAsyncFrameSource captureSource;",
    "size_t captureParameter;",
    "char* captureImmediate;",
    "XL6_IrAsyncCaptureAbi captureAbi;",
    "asyncForwardParameter",
    "asyncResumeParameter",
    "asyncResumeOpcode",
    "asyncResumeAwaitOnLeft",
    "XL6_IR_FUNCTION_ASYNC_CFG",
    "XL6_IR_ASYNC_DISPATCH_XRT_FUTURE_CONTINUATION",
    "XL6_IrAsyncContinuation",
    "XL6_IrAsyncFrameSlot",
    "XL6_IrAsyncResumeOp",
    "XL6_IrAsyncInputSource",
    "XL6_IrAsyncFrameSource",
    "XL6_IR_ASYNC_CONTINUATION_",
    "XL6_IR_ASYNC_FRAME_SOURCE_",
    "XL6_IR_ASYNC_INPUT_",
    "asyncContinuation",
    "asyncContinuationCount",
    "asyncFrameSlotCount",
    "asyncResumeOpCount",
    "asyncContinuations",
    "asyncFrameSlots",
    "asyncResumeOps"
) ) {
    if ( $header.Contains($legacyAsyncField) -or $lower.Contains($legacyAsyncField) -or $cEmitter.Contains($legacyAsyncField) ) {
        throw "XLIR async continuation ABI still uses a legacy CallTarget shape field: $legacyAsyncField"
    }
}

# Suspension semantics are no longer selected from source expression shape.
# Lowering may select the no-suspension ready-Future fast path; every AWAIT is
# otherwise analyzed from the lowered CFG by the canonical machine builder.
foreach ( $required in @(
    "Suspension semantics are derived once from lowered CFG",
    "xl6IrLowerMarkAsyncReadyFuture",
    "xl6IrLowerSubtreeMayEscapeRaise"
) ) {
    if ( ! $lower.Contains($required) ) {
        throw "XLIR lowering does not preserve the planner-free async boundary: $required"
    }
}
foreach ( $forbiddenCall in @(
    "!xl6IrLowerPlanAsyncPureReturn",
    "!xl6IrLowerMarkAsyncFutureExternCatchDefault"
) ) {
    if ( $lower.Contains($forbiddenCall) ) {
        throw "XLIR async registration still invokes a source-shape planner: $forbiddenCall"
    }
}
foreach ( $required in @(
    "XL6_IrAsyncMachine", "XL6_IrAsyncSuspension", "XL6_IrAsyncMachineFrameSlot",
    "XL6_IrAsyncLiveSlot",
    "XL6_IrAsyncSpillSource", "XL6_IrAsyncFrameTransferAbi", "XL6_IrAsyncFrameResidence",
    "XL6_IR_ASYNC_FRAME_TRANSFER_NONE", "XL6_IR_ASYNC_FRAME_TRANSFER_MOVE_OWNED",
    "XL6_IR_ASYNC_FRAME_RESIDENCE_LAUNCH_PARAMETER",
    "XL6_IR_ASYNC_FRAME_RESIDENCE_FRAME_STORAGE", "XL6_IR_ASYNC_FRAME_RESIDENCE_SSA_SPILL",
    "XL6_IrAsyncFrameActionKind", "XL6_IrAsyncFrameAction",
    "XL6_IR_ASYNC_FRAME_ACTION_INIT_PARAMETER", "XL6_IR_ASYNC_FRAME_ACTION_BIND_STORAGE",
    "XL6_IR_ASYNC_FRAME_ACTION_SPILL_VALUE", "XL6_IR_ASYNC_FRAME_ACTION_PROJECT_VALUE",
    "XL6_IR_ASYNC_FRAME_ACTION_DESTROY_IF_INITIALIZED",
    "XL6_IR_ASYNC_MACHINE_ANALYZED", "XL6_IR_ASYNC_MACHINE_BLOCKED_LIVE_ABI",
    "asyncMachine", "asyncMachines", "asyncMachineFrameSlots", "asyncSuspensions",
    "asyncLiveSlots", "asyncFrameActions",
    "XL6_IR_ASYNC_FRAME_DESTROY_RELEASE_CELL", "XL6_IR_ASYNC_FRAME_DESTROY_FREE_ERROR",
    "XL6_IR_ASYNC_FRAME_DESTROY_DESTROY_VALUE_ITERATOR"
) ) {
    if ( ! $header.Contains($required) ) {
        throw "XLIR header is missing generic async CFG machine/liveness ABI: $required"
    }
}
foreach ( $required in @(
    "xl6IrBuildAsyncMachines", "xl6IrBuildAsyncFunctionMachine",
    "xl6IrAsyncStorageEffect", "xl6IrVerifyAsyncMachines",
    "asyncMachineCount", "asyncMachineFrameSlotCount", "asyncSuspensionCount",
    "xl6IrAsyncFrameResidenceName", "initBitCount", "firstLiveState", "lastLiveState",
    "xl6IrAsyncFinalizeMachineActions", "xl6IrAsyncFrameActionKindName",
    "firstPrologueAction", "firstSpillAction", "firstResumeAction", "firstDestroyAction",
    "failureExitPlan", "cleanupTargetScopeDepth", "cleanupTargetRegion",
    "firstCleanupAction", "cleanupActionCount",
    "asyncLiveSlotCount", "async-machine", "machine-frame",
    "asyncMachineFrameSlots", "asyncSuspensions", "asyncLiveSlots"
) ) {
    if ( ! $source.Contains($required) -and ! $lower.Contains($required) ) {
        throw "XLIR implementation is missing generic async CFG machine/liveness analysis: $required"
    }
}
foreach ( $required in @(
    "xlir_async_cfg_liveness.xl", "block program", "unique machine frame layout",
    "per-state membership", "frame residence/ranges/init bits", "ordered frame actions",
    "lexical cleanup transitions",
    "exact transfer/destroy ABI", "direct branch machine Cgen",
    "deterministic C", "task-pool compatibility", "-Werror"
) ) {
    if ( ! $asyncCfgLivenessGate.Contains($required) ) {
        throw "XLIR async CFG liveness quality gate is incomplete: $required"
    }
}
foreach ( $required in @("async-machine", "machine-frame", "xl6IrFunctionKindName") ) {
    if ( ! $source.Contains($required) ) {
        throw "XLIR dump/verifier does not expose the canonical async machine role: $required"
    }
}
foreach ( $required in @(
    "XL6_IR_INSTR_STORAGE_ADDRESS", "XLIR async storage-address ABI is not admitted",
    "XLIR async machine storage address has no exact binding"
) ) {
    if ( ! $header.Contains($required) -and ! $cEmitter.Contains($required) ) {
        throw "XLIR async storage-address implementation is incomplete: $required"
    }
}
foreach ( $required in @(
    "xlir_async_storage_address", "asyncMachineCount", "'storage-address'", "-Werror"
) ) {
    if ( ! $channelOwnedGate.Contains($required) ) {
        throw "XLIR async owned receive quality gate is incomplete: $required"
    }
}
if (!$asyncStorageAddressGate.Contains('check_xlir_channel_resource_abi.ps1')) {
    throw 'The historical async storage-address entry must run the current owned receive gate'
}
if ( ! $languageFrontendSuite.Contains(
         "tools\check_xlir_async_storage_address.ps1") ) {
    throw "XLIR async storage-address quality gate is not wired into the language frontend suite"
}
foreach ( $legacySelector in @(
    "xl6IrLowerMarkAsyncFutureExternForward",
    "xl6IrLowerMarkAsyncFutureExternThenLocalParameter",
    "xl6IrLowerMarkAsyncFutureForward",
    "xl6IrLowerMarkAsyncFutureThenScalar",
    "xl6IrLowerMarkAsyncFutureThenImmediate",
    "xl6IrLowerMarkAsyncFutureThenLocalParameter",
    "xl6IrLowerMarkAsyncFutureThenStringParameters",
    "xl6IrLowerMarkAsyncFutureThenCaptureChain",
    "xl6IrLowerMarkAsyncFutureAwaitChain"
) ) {
    if ( ([regex]::Matches($lower, [regex]::Escape($legacySelector) + '\s*\(')).Count -ne 0 ) {
        throw "Legacy async source-shape selector was not deleted: $legacySelector"
    }
}
foreach ( $required in @(
    "xlir_async_generic_expression_planner.xl",
    "source-shape planners disconnected",
    "compatibility model removed",
    "canonical lowered await CFG",
    "--lowered", "--format json", "--format text",
    "SequenceEqual", "-Werror", "runtime gate failed",
    "Legacy async source-shape selector was not deleted"
) ) {
    if ( ! $asyncGenericPlannerGate.Contains($required) ) {
        throw "XLIR generic async expression planner quality gate is incomplete: $required"
    }
}
foreach ( $required in @(
    "xlir_async_future_two_awaits",
    "xlir_async_future_three_awaits",
    "xlir_async_future_three_awaits_mixed",
    "xlir_async_future_three_strings",
    "direct scalar/managed two/three-await machine CFG",
    "canonical single-layout activation handoff",
    "forward/reverse transactional resident transfer",
    "exact managed operand owners and anchored borrow-storage observations with success/failure cleanup",
    "Multi-await borrowed spill does not name its exact operand owner",
    "Multi-await failure lost its managed operand cleanup",
    "Multi-await managed consumer success/failure lost its owner cleanup",
    "legacy async-chain renderer forbidden",
    "SequenceEqual", "-Werror", "runtime terminal-state propagation"
) ) {
    if ( ! $asyncMultiAwaitMachineGate.Contains($required) ) {
        throw "XLIR multi-await machine quality gate is incomplete: $required"
    }
}

foreach ( $required in @(
    "xl6IrLowerModule",
    "xl6IrLowerClassLayoutGet",
    "xl6IrLowerDefaultClassInitialize",
    "xl6IrLowerClassInitializeRuntimeTag",
    "xl6IrLowerClassDispatchSlotByDeclarationSymbol",
    "xl6IrLowerClassInitializeInheritedDefaults",
    "XLIR explicit class constructor has no verified inherited default-field ABI",
    "xl6IrLowerDefaultClassConstructorCall",
    "xl6IrLowerIsInstanceClassMethod",
    "xl6IrLowerClassCallableNeedsReceiver",
    "Receiver presence is a declaration ABI fact",
    "XLIR class callable has no canonical borrowed receiver ABI",
    "XLIR class callable self binding conflicts with its canonical receiver ABI",
    "xl6IrLowerClassStaticFieldFromMember",
    "xl6IrLowerRegisterClassStaticFields",
    "xl6IrLowerInitializeClassStaticFields",
    "borrowed receiver in slot zero",
    "XLIR default class constructor requires a verified zero-argument layout ABI",
    "xl6IrLowerRejectUnsupportedDeclarations",
    "A protocol declaration is compile-time metadata",
    "Every actual PROTOCOL_CALL still",
    "XLIR virtual declaration has no canonical class dispatch slot",
    "XL6_IR_INSTR_VIRTUAL_CALL",
    "xl6IrLowerClassDispatchSlotByDeclarationSymbol",
    "XL6_IR_INSTR_CONST",
    "XL6_IR_INSTR_LOAD",
    "XL6_IR_INSTR_STORE",
    "XL6_IR_INSTR_CALL",
    "XL6_IR_INSTR_DISCARD",
    "XL6_IR_INSTR_BRANCH",
    "XL6_IR_INSTR_JUMP",
    "XL6_IR_INSTR_RETURN",
    "XL6_IR_INSTR_THROW",
    "XL6_IR_INSTR_ERROR_RAISE",
    "XL6_IR_INSTR_ERROR_TEST",
    "XL6_IR_INSTR_ERROR_TAKE",
    "XL6_IR_INSTR_ERROR_SET_TAKE",
    "XL6_IR_INSTR_RUNTIME_CALL",
    "XL6_IR_RUNTIME_STRING_DUP",
    "xl6IrLowerCanonicalCStringLiteral",
    "xl6IrLowerEmitRuntimeDataCall",
    "XL6_IR_RUNTIME_STRING_FROM_UTF8", "XL6_IR_RUNTIME_BYTES_FROM_DATA",
    "XLIR managed string literal has no decoded UTF-8 payload",
    "XLIR bytes literal has no explicit-length bytes ABI",
    "XLIR char literal has no validated Unicode-scalar payload",
    "xl6IrLowerBeginFunction",
    "xl6IrLowerIf",
    "xl6IrLowerWhile",
    "xl6IrLowerDoWhile",
    "xl6IrLowerRangeFor",
    "xl6IrLowerScalarMatch",
    "xl6IrLowerAssert",
    "xl6IrLowerTryCatch",
    "xl6IrLowerTryFinally",
    "xl6IrLowerTryCatchFinally",
    "xl6IrLowerPushDeferredReturn",
    "xl6IrLowerActiveDeferredReturn",
    "xl6IrLowerPushFinallyBarrier",
    "xl6IrLowerFindDeferredLoopExit",
    "xl6IrLowerEmitDeferredLoopExitFinalies",
    "xl6IrLowerSubtreeHasReturn",
    "xl6IrLowerTypeSupportsDeferredReturn",
    "xl6IrLowerEmitExceptionTransfer",
    "XLIR try/catch requires one canonical handler and no finally",
    "XLIR catch payload has no canonical owned xerror storage ABI",
    "xl6IrLowerPushCatchPayload",
    "xl6IrLowerTypeIsRuntimeError",
    "explicit trivial-value or error-observer discard ABI",
    "XLIR catch/finally payload has no canonical owned xerror storage ABI",
    "XLIR catch rethrow requires an owned error payload transfer ABI",
    "xl6IrLowerActiveCatchPayload",
    "XLIR try/finally requires a canonical empty catch list and non-empty finally block",
    "XLIR try/catch/finally requires one canonical handler and non-empty finally block",
    "XLIR return ABI through finally has no typed pending-exit storage contract",
    "XLIR return conversion does not match its exact result ABI",
    "XLIR return through catch/finally has no pending value storage ABI",
    "xl6IrLowerEmitNormalBlockCleanup",
    "XLIR normal scope exit has no canonical AFTER_SCOPE plan",
    "XLIR scope plan action has no exact lowered storage ABI",
    "XLIR assertion has no explicit exception exit ABI",
    "xl6IrLowerVerifyErrorAbis",
    "xl6IrLowerPrecomputeCallTargetErrorAbis",
    "xl6IrLowerSubtreeMayEscapeRaise",
    "XLIR internal call target has no typed body for error ABI",
    "XLIR internal call target has no function error ABI",
    "XLIR escaping throw lacks a precomputed function error ABI",
    "XLIR call completion has no explicit outcome and propagation CFG",
    "XLIR await requires an exact resolved future result ABI",
    "XLIR await requires a resolved future storage, call, index or conditional result",
    "XLIR await has no explicit success or failure CFG ABI",
    "XLIR async task ABI requires a result ownership ABI and explicit task-frame captures",
    "xl6IrParameterFrameTransfer",
    "XLIR async task frame has no explicit managed capture ABI",
    "xl6IrLowerRemoveUnmaterializedLocalCleanup",
    "xl6IrAsyncParameterCapture",
    "xl6IrFunctionParameterBindingOwnsStorage",
    "workerErrorAbi",
    "XLIR loop exit has no resolved CFG target",
    "XLIR call requires normalized positional value arguments",
    "xl6IrLowerResolvedCallReturnType",
    "xl6IrLowerMaterializeGenericSpecializations",
    "xl6IrLowerBuildGenericSpecialization",
    "xl6IrLowerSpecializationCloneNode",
    "target->parent = XL6_IR_NODE_INVALID",
    "child->parent = id",
    "xl6IrLowerAppendSpecializationChild",
    "module->nodes[declaration - 1].parent = parent",
    "specializationParent = enclosingClass == XL6_IR_NODE_INVALID",
    "xl6IrLowerSpecializationCloneExitPlans",
    "xl6IrLowerInferGenericCallTypeArgs",
    "xl6IrLowerUnifyGenericType",
    "requires resolved concrete type arguments before lowering",
    "xl6IrLowerAnchorResolvedCallBorrow",
    "A same-TypeId",
    "projections are distinct producers",
    "xl6IrLowerCallTargetNeedsMonomorphization",
    "XLIR generic call requires lowering-time monomorphization into a concrete instance ABI",
    # Parameter ownership is shared by call targets and protocol requirements;
    # keeping this public IR helper prevents two lowering rules from drifting.
    "xl6IrParameterAbiForNode",
    "xl6IrLowerRegisterModuleCallTargets",
    "xl6IrLowerCName",
    "xl6IrLowerRegisterModuleExports",
    "xl6IrLowerAutoExportOverloadCount",
    "xl6IrLowerExportTypeTag",
    "xvalue_%016llx",
    "exported->parameterTypes",
    "exported->parameterAbis",
    "exported->returnAbi",
    "XLIR export parameter has no exact public C ABI",
    "XLIR export has no canonical C identifier ABI",
    "XLIR extern call target has no canonical C identifier ABI",
    "xl6IrLowerBuildModuleAbi",
    "XLIR module entry has no canonical function ABI",
    "XLIR call result has no canonical target function ABI",
    "XL6_AST_EXPR_STMT",
    "XLIR expression statement requires explicit trivial-value or error-observer discard ABI",
    "xl6IrLowerTypeHasStorageReleaseAbi",
    "XLIR expression statement has no exact owned-rvalue storage release ABI",
    "XLIR scalar match requires literal or else patterns",
    "XLIR lowering requires explicit ownership instructions",
    "XLIR function value requires a lowered closure environment and invoke ABI",
    "xl6IrLowerFunctionValueCall",
    "A first-class callable uses the same typed value boundary",
    "xl6IrLowerClosureTargetGet",
    "xl6IrLowerCollectClosureCaptures",
    "ancestor capture is first materialized",
    "xl6IrLowerClosureCaptureStorage",
    "XLIR closure capture requires an implemented frame ownership ABI",
    "XLIR return value does not match its function ABI",
    "XLIR closure call has no exact signature and result ABI",
    "xl6IrLowerExitPlan",
    "xl6IrLowerEmitExitCleanup",
    "XLIR exit has no explicit cleanup plan",
    "XLIR cleanup action has no lowered storage ABI",
    "xl6IrLowerShortCircuit",
    "XLIR short-circuit expression has no boolean ABI",
    "xl6IrLowerConditional",
    "XLIR conditional expression has no canonical shape or result ABI",
    "xl6IrLowerConditionalAsRuntimeXValue",
    "XLIR dynamic conditional has no resolved result ABI",
    "opcode = node->opcode",
    "XLIR binary operator has no canonical opcode",
    "xl6IrLowerEmitRuntimeCall",
    "XL6_IR_RUNTIME_STRING_CONCAT",
    "XL6_IR_RUNTIME_STRING_EQUAL",
    "XL6_IR_RUNTIME_STRING_COPY",
    "XL6_IR_RUNTIME_XVALUE_NULL",
    "XL6_IR_RUNTIME_XVALUE_I64",
    "XL6_IR_RUNTIME_XVALUE_STRING_COPY",
    "XLIR dynamic literal requires an explicit runtime boxing ABI",
    "xl6IrLowerTypeIsRuntimeXValue",
    "xl6IrLowerArrayLiteralAsRuntimeXValue",
    "xl6IrLowerListLiteralAsRuntimeXValue",
    "xl6IrLowerSetLiteralAsRuntimeXValue",
    "xl6IrLowerRuntimeXValueIndexI64",
    "xl6IrLowerRuntimeXValueKeyRead",
    "xl6IrLowerObjectLiteralAsRuntimeXValue",
    "xl6IrLowerCollectionItemAsRuntimeXValue",
    "xl6IrLowerRuntimeXValueObjectGetLiteral",
    "xl6IrLowerElementAssign",
    "xl6IrLowerPrepareElementTargetPlan",
    "xl6IrLowerWriteElementTargetPlan",
    "xl6IrLowerRuntimeXValueMemberGetLiteral",
    "xl6IrLowerRuntimeXValueCount",
    "xl6IrLowerEmitElementTargetWrite",
    "xl6IrLowerNodeAsRuntimeXValue",
    "xl6IrLowerXValueContains",
    "xl6IrLowerTypeIsDynamicXValueBuiltin",
    "xl6IrLowerStringContains",
    "xl6IrLowerRangeContains",
    "XLIR typed collection literal has no normalized inner literal",
    "XLIR dynamic conversion has no resolved target ABI",
    "XLIR dynamic member read requires a resolved xvalue receiver",
    "XLIR class field assignment does not match its declared storage ABI",
    "XLIR dynamic object literal requires canonical string keys",
    "XLIR integer index read requires a resolved xvalue receiver and i64 key",
    "XLIR key read requires an xvalue receiver and exact string or erased key ABI",
    "XLIR index mutation requires an xvalue receiver and typed key",
    "XLIR index mutation key does not match its resolved write ABI",
    "XLIR xvalue membership has no exact collection, element/key or boolean ABI",
    "XLIR string membership has no exact haystack, needle or boolean ABI",
    "XLIR range membership has no exact descriptor, integer item or boolean ABI",
    "XLIR dynamic collection conversion has no resolved array ABI",
    "XLIR nested dynamic collection has no resolved runtime ABI",
    "XL6_IR_RUNTIME_XVALUE_ARRAY_APPEND_NEW",
    "xl6IrLowerIntrinsicCollectionInsert",
    "XLIR property assignment has no resolved accessor transaction",
    "xl6IrLowerReleaseElementTargetPlan",
    "XLIR index mutation has no resolved address and write ABI",
    "XLIR storage target requires an exact type and ownership ABI",
    "xl6IrLowerOwnedManagedStringValue",
    "XLIR managed string boundary requires an exact lowered type",
    "xl6IrLowerBorrowManagedStringOperand",
    "xl6IrLowerReleaseManagedStringTemporaries",
    "XLIR managed binary has no exact operand/result ABI",
    "XLIR lowering has no rule for this semantic node"
) ) {
    if ( ! $lower.Contains($required) ) {
        throw "XLIR lowering implementation is missing explicit semantic contract: $required"
    }
}

foreach ( $required in @(
    "XL6_IR_RUNTIME_XVALUE_INDEX_GET_XVALUE_KEY",
    "XL6_IR_RUNTIME_XVALUE_INDEX_SET_XVALUE_KEY_NEW",
    "Borrows stable erased collection/key storage",
    "item on every path"
) ) {
    if ( ! $header.Contains($required) ) {
        throw "XLIR public model is missing dynamic-key index ownership ABI: $required"
    }
}
foreach ( $required in @(
    "XL6_IrIndexWriteAbi", "indexWriteAbi", "indexWriteErrorAbi",
    "XL6_IR_INDEX_WRITE_XVALUE_ARRAY_I64_SET_NEW",
    "XL6_IR_INDEX_WRITE_XVALUE_LIST_I64_SET_NEW",
    "XL6_IR_INDEX_WRITE_XVALUE_OBJECT_LITERAL_SET_NEW",
    "XL6_IR_INDEX_WRITE_XVALUE_OBJECT_STRING_SET_NEW",
    "XL6_IR_INDEX_WRITE_XVALUE_KEY_DISPATCH_SET_NEW",
    "xl6IrRefreshIndexWriteAbis"
) ) {
    if ( ! $header.Contains($required) ) {
        throw "XLIR public model is missing canonical IndexWriteAbi: $required"
    }
}
foreach ( $required in @(
    "xl6IrClassifyIndexWriteAbi", "xl6IrClassifyIndexWriteErrorAbi",
    "xl6IrBuildIndexWriteAbis",
    "xl6IrIndexWriteAbiName", "xvalue-array-i64-set-new",
    "xvalue-list-i64-set-new", "xvalue-object-literal-set-new",
    "xvalue-object-string-set-new", "xvalue-key-dispatch-set-new",
    "typed XLIR index mutation has no exact canonical write and failure ABI"
) ) {
    if ( ! $source.Contains($required) ) {
        throw "XLIR verifier/dump is missing canonical IndexWriteAbi: $required"
    }
}
foreach ( $required in @(
    "switch ( lhs->indexWriteAbi )",
    "XL6_IR_INDEX_WRITE_XVALUE_LIST_I64_SET_NEW",
    "XL6_IR_RUNTIME_XVALUE_INT_MAP_SET_I64_NEW",
    "xl6IrLowerEmitCheckedIndexWrite", "XL6_IR_INSTR_ERROR_RAISE",
    "container index assignment failed", "RangeError",
    "XLIR index mutation has no resolved address and write ABI"
) ) {
    if ( ! $lower.Contains($required) ) {
        throw "XLIR lowering is missing canonical IndexWriteAbi dispatch: $required"
    }
}
if ( $cEmitter.Contains("XL6_IR_INDEX_WRITE_") ) {
    throw "XLIR C emitter must not consume Typed IndexWriteAbi"
}
foreach ( $required in @(
    "xlir_index_write_abi.xl", "xlir_index_write_generic_abi.xl",
    "first-class verified IndexWriteAbi",
    "exact array/list/object/dynamic consuming RuntimeOps",
    "uint64 boxing", "null-aware list reuse", "recoverable failure ABI",
    "generic specialization refresh", "one RangeError CFG edge per write",
    "mechanical C", "-Werror"
) ) {
    if ( ! $indexWriteAbiGate.Contains($required) ) {
        throw "XLIR IndexWriteAbi quality gate is incomplete: $required"
    }
}
foreach ( $required in @(
    "XL6_IR_RUNTIME_I64_RANGE_CONTAINS",
    "item/start/stop/step",
    "verified RANGE intrinsic"
) ) {
    if ( ! $header.Contains($required) ) {
        throw "XLIR public model is missing canonical range membership ABI: $required"
    }
}
foreach ( $required in @(
    "XL6_IR_RUNTIME_I64_RANGE_CONTAINS",
    "xlir_i64_range_contains",
    "needsI64RangeContainsRuntime",
    "magnitude = (uint64_t)0 - (uint64_t)step"
) ) {
    if ( ! $cEmitter.Contains($required) ) {
        throw "XLIR C emitter is missing mechanical overflow-safe range membership emission: $required"
    }
}
foreach ( $required in @(
    "XL6_IrLoopAbi", "XL6_IR_LOOP_ABI_RANGE_I64_VALUE",
    "XL6_IR_LOOP_ABI_RANGE_I64_INDEX_VALUE",
    "XL6_IR_LOOP_ABI_XVALUE_ARRAY_I64_VALUE",
    "XL6_IR_LOOP_ABI_XVALUE_ARRAY_I64_INDEX_VALUE", "loopErrorAbi",
    "XL6_IR_LOOP_ABI_XVALUE_ITERATOR_VALUE",
    "XL6_IR_LOOP_ABI_XVALUE_ITERATOR_KEY",
    "XL6_IR_LOOP_ABI_XVALUE_ITERATOR_KEY_VALUE",
    "XL6_IR_LOOP_ABI_STRING_UTF8_VALUE",
    "XL6_IR_LOOP_ABI_STRING_UTF8_INDEX_VALUE",
    "XL6_IR_LOOP_ABI_BYTES_VALUE",
    "XL6_IR_LOOP_ABI_BYTES_INDEX_VALUE",
    "XL6_IR_LOOP_ABI_PROTOCOL_ITERATOR_VALUE",
    "XL6_IR_LOOP_ABI_PROTOCOL_ITERATOR_INDEX_VALUE",
    "XL6_IrLoopKeyAbi", "XL6_IrLoopItemAbi", "loopKeyAbi", "loopItemAbi",
    "XL6_IR_LOOP_KEY_DYNAMIC_XVALUE",
    "XL6_IR_LOOP_ITEM_DYNAMIC_LOGICAL_XVALUE",
    "XL6_IR_LOOP_ITEM_CLOSURE_COPY",
    "XL6_IR_LOOP_ITEM_DISCARD",
    "XL6_IR_LOOP_ITEM_CHAR", "XL6_IR_LOOP_ITEM_U8",
    "XL6_IrLoopTargetLifetimeAbi", "loopKeyLifetimeAbi", "loopItemLifetimeAbi",
    "XL6_IR_LOOP_TARGET_LIFETIME_OWNED_ITERATION_RELEASE",
    "XL6_IrLoopResourceAbi",
    "XL6_IR_LOOP_RESOURCE_XVALUE_SHALLOW_SNAPSHOT_RELEASE",
    "XL6_IR_LOOP_RESOURCE_XVALUE_OWNED_ITERATOR_RELEASE",
    "XL6_IR_LOOP_RESOURCE_XVALUE_ITERATOR_DESTROY",
    "XL6_IR_LOOP_RESOURCE_STRING_SNAPSHOT_RELEASE",
    "XL6_IR_LOOP_RESOURCE_BYTES_SNAPSHOT_RELEASE",
    "XL6_IR_LOOP_RESOURCE_PROTOCOL_ITERATOR_RELEASE",
    "XL6_IR_LOOP_ITEM_PROTOCOL_PRODUCT_VALUE",
    "XL6_IR_LOOP_ITEM_PROTOCOL_PRODUCT_TAKE",
    "loopProtocolIterTarget", "loopProtocolNextTarget",
    "loopProtocolIteratorType", "loopProtocolNextResultType",
    "loopProtocolItemType",
    "XL6_IR_RUNTIME_STRING_UTF8_AT", "XL6_IR_RUNTIME_STRING_SCALAR_AT",
    "XL6_IR_RUNTIME_BYTES_LENGTH", "XL6_IR_RUNTIME_BYTES_AT",
    "XL6_IR_TYPE_VALUE_ITERATOR",
    "XL6_IR_C_ABI_RUNTIME_VALUE_ITERATOR_PTR",
    "loopResourceAbi", "loopResourceType"
) ) {
    if ( ! $header.Contains($required) ) {
        throw "XLIR public model is missing canonical range-for loop ABI: $required"
    }
}
foreach ( $required in @(
    "xl6IrClassifyLoopAbi", "xl6IrRefreshLoopAbis", "xl6IrLoopAbiName",
    "range-i64-value", "range-i64-index-value", "loopErrorAbiName",
    "xvalue-array-i64-value", "xvalue-array-i64-index-value",
    "xvalue-iterator-value", "xvalue-iterator-key", "xvalue-iterator-key-value",
    "string-utf8-value", "string-utf8-index-value",
    "bytes-value", "bytes-index-value",
    "protocol-iterator-value", "protocol-iterator-index-value",
    "protocol-product-value", "protocol-product-take",
    "protocol-iterator-release",
    "xl6IrResolveLoopProtocol", "xl6IrClassDeclarationByConcreteType",
    "integer-i64", "string-copy", "dynamic-xvalue", "dynamic-logical-xvalue",
    "closure-copy",
    "owned-iteration-release", "xvalue-iterator-destroy",
    "string-snapshot-release", "bytes-snapshot-release",
    "string-scalar-at", "bytes-length", "bytes-at", "char", "u8",
    "xl6IrLoopTargetLifetimeAbiName",
    "XL6_IR_LOOP_RESOURCE_XVALUE_SHALLOW_SNAPSHOT_RELEASE",
    "xl6IrLoopResourceAbiName", "loopResourceType",
    "XL6_IR_TYPE_GENERIC_INSTANCE",
    "intrinsic->operandCount == 3", "stringType.semanticTypeAbiId"
) ) {
    if ( ! $source.Contains($required) ) {
        throw "XLIR verifier/dump is missing canonical range-for loop ABI: $required"
    }
}
foreach ( $required in @(
    "xl6IrLowerXValueIteratorFor", "XL6_IR_TYPE_VALUE_ITERATOR",
    "XL6_IR_RUNTIME_XVALUE_ITERATOR_CREATE",
    "XL6_IR_RUNTIME_XVALUE_ITERATOR_ADVANCE",
    "XL6_IR_RUNTIME_XVALUE_ITERATOR_ITEM_I64",
    "XL6_IR_RUNTIME_XVALUE_ITERATOR_KEY_I64",
    "XL6_IR_RUNTIME_XVALUE_ITERATOR_ITEM_U64",
    "XL6_IR_RUNTIME_XVALUE_ITERATOR_ITEM_BOOL",
    "XL6_IR_RUNTIME_XVALUE_ITERATOR_ITEM_F64",
    "XL6_IR_RUNTIME_XVALUE_ITERATOR_ITEM_STRING_COPY",
    "XL6_IR_RUNTIME_XVALUE_ITERATOR_KEY_STRING_COPY",
    "XL6_IR_RUNTIME_XVALUE_ITERATOR_CREATE_CHECKED",
    "XL6_IR_RUNTIME_XVALUE_ITERATOR_KEY_DYNAMIC_XVALUE",
    "XL6_IR_RUNTIME_XVALUE_ITERATOR_DYNAMIC_LOGICAL_XVALUE",
    "XL6_IR_LOOP_RESOURCE_XVALUE_ITERATOR_DESTROY",
    "XL6_IR_LOWER_LOOP_RESOURCE_ITERATION_TARGET",
    "xl6IrLowerPushIterationTargetResource",
    "conditionBlock", "iteratorStateSymbol"
) ) {
    if ( ! $lower.Contains($required) ) {
        throw "XLIR lowering is missing canonical xvalue iterator ABI: $required"
    }
}
foreach ( $required in @(
    "xl6IrLowerPrimitiveSequenceFor", "XL6_IR_RUNTIME_STRING_COPY",
    "XL6_IR_RUNTIME_STRING_UTF8_COUNT", "XL6_IR_RUNTIME_STRING_SCALAR_AT",
    "XL6_IR_RUNTIME_BYTES_RETAIN", "XL6_IR_RUNTIME_BYTES_LENGTH",
    "XL6_IR_RUNTIME_BYTES_AT",
    "XL6_IR_LOOP_RESOURCE_STRING_SNAPSHOT_RELEASE",
    "XL6_IR_LOOP_RESOURCE_BYTES_SNAPSHOT_RELEASE",
    "xl6IrLowerPushLoopResource", "xl6IrLowerPushIterationTargetResource",
    "conditionBlock", "snapshotSymbol", "positionSymbol"
) ) {
    if ( ! $lower.Contains($required) ) {
        throw "XLIR lowering is missing canonical UTF-8 string iterator ABI: $required"
    }
}
foreach ( $required in @(
    "xl6IrLowerProtocolIteratorFor", "xl6IrLowerCompleteReceiverCall",
    "xl6IrLowerNewHiddenLocalStorage",
    "XL6_IR_LOOP_ITEM_PROTOCOL_PRODUCT_TAKE",
    "XL6_IR_LOOP_RESOURCE_PROTOCOL_ITERATOR_RELEASE",
    "XL6_IR_INSTR_PRODUCT_GET", "XL6_IR_INSTR_PRODUCT_TAKE"
) ) {
    if ( ! $lower.Contains($required) ) {
        throw "XLIR lowering is missing canonical class protocol iterator ABI: $required"
    }
}
foreach ( $required in @(
    "XL6_IR_RUNTIME_STRING_SCALAR_AT", "xlir_string_scalar_at",
    "XL6_IR_RUNTIME_BYTES_AT", "xlir_bytes_at",
    "XL6_IR_RUNTIME_BYTES_LENGTH", "xlir_bytes_length"
) ) {
    if ( ! $cEmitter.Contains($required) ) {
        throw "XLIR C emitter is missing mechanical string/bytes scalar projection: $required"
    }
}
foreach ( $required in @(
    "xl6IrLowerRangeFor", "node->loopAbi", "indexTarget->symbol",
    "range step cannot be 0", "RangeError", "XL6_IR_INSTR_ERROR_RAISE",
    "xl6IrLowerEmitExceptionTransfer",
    "XLIR for loop has no admitted canonical iterator ABI"
) ) {
    if ( ! $lower.Contains($required) ) {
        throw "XLIR lowering is missing canonical range-for CFG/error ABI: $required"
    }
}
foreach ( $required in @(
    "xlir_list_i64_iterator_abi.xl", "xvalue-iterator-key-value",
    "integer-i64", "xvalue-iterator-destroy", "value-iterator",
    "xvalue-iterator-create", "xvalue-iterator-advance",
    "xvalue-iterator-item-i64", "xvalue-iterator-key-i64",
    "dedicated cursor TypeId", "orthogonal key/item projection",
    "stable named and owned-rvalue", "LoopResource destroy on return/throw/finally",
    "mechanical C", "-Werror"
) ) {
    if ( ! $xvalueIteratorAbiGate.Contains($required) ) {
        throw "XLIR xvalue iterator ABI quality gate is incomplete: $required"
    }
}
foreach ( $required in @(
    "xlir_set_i64_iterator_abi.xl", "cgen_xlir_set_i64_key_reject.xl",
    "xvalue-iterator-value", "xvalue-iterator-key-i64",
    "compiler-owned cursor TypeId", "shared xvalue cursor and C helpers",
    "exact value-only projection", "no invented key ABI",
    "stable named and owned-rvalue snapshots",
    "LoopResource destroy on return/throw/finally", "mechanical C", "-Werror"
) ) {
    if ( ! $setI64IteratorAbiGate.Contains($required) ) {
        throw "XLIR set<i64> iterator ABI quality gate is incomplete: $required"
    }
}
foreach ( $required in @(
    "xlir_xvalue_iterator_scalar_projection.xl",
    "xlir_set_u64_iterator_abi.xl",
    "xvalue-iterator-item-u64", "xvalue-iterator-item-bool",
    "xvalue-iterator-item-f64", "integer-i64",
    "list u64 plus list/set bool/f64", "native set<u64> admission is exact",
    "Cgen is mechanical", "-Werror"
) ) {
    if ( ! $iteratorScalarProjectionGate.Contains($required) ) {
        throw "XLIR xvalue iterator scalar projection quality gate is incomplete: $required"
    }
}
foreach ( $required in @(
    "xlir_xvalue_iterator_managed_projection.xl",
    "xvalue-iterator-item-string-copy", "owned-iteration-release",
    "per-iteration release on fallthrough/continue/break/return/throw/finally",
    "return ownership preserved through finally", "cursor teardown ordering",
    "mechanical C", "-Werror"
) ) {
    if ( ! $iteratorManagedProjectionGate.Contains($required) ) {
        throw "XLIR managed iterator projection quality gate is incomplete: $required"
    }
}
foreach ( $required in @(
    "xlir_xvalue_array_iterator_projection.xl",
    "xvalue-iterator-key-ordinal-i64",
    "xvalue-iterator-item-xvalue-retain",
    "ordinal/u64/bool/f64/string/xvalue projections",
    "snapshot stability", "managed cleanup across exits",
    "cursor/item frame ownership across await", "launch/resume completion",
    "deterministic mechanical C", "-Werror"
) ) {
    if ( ! $arrayIteratorProjectionGate.Contains($required) ) {
        throw "XLIR xvalue-array iterator projection quality gate is incomplete: $required"
    }
}
if ( ! $languageFrontendSuite.Contains(
        "tools\check_xlir_xvalue_array_iterator_projection.ps1") ) {
    throw "Language frontend suite does not execute the xvalue-array iterator projection gate"
}
foreach ( $required in @(
    "xlir_dict_iterator_projection.xl", "xlir_dict_key_only_iterator.xl",
    "xvalue-iterator-key", "xvalue-iterator-key-string-copy",
    "i64/u64/bool/f64/string item projections",
    "double-managed reverse cleanup on all exits/finally",
    "independent return ownership", "mechanical C", "-Werror"
) ) {
    if ( ! $dictIteratorProjectionGate.Contains($required) ) {
        throw "XLIR dict iterator projection quality gate is incomplete: $required"
    }
}
foreach ( $required in @(
    "xlir_erased_collection_iterator_abi.xl",
    "collectionAbiName", "runtimeTypeMap.fallback",
    "xvalue-iterator-item-xvalue-retain",
    "xvalue-iterator-key-ordinal-i64", "xvalue-iterator-key-i64",
    "xvalue-iterator-key-string-copy", "source-bound cursor",
    "ReadAllBytes", "-Werror"
) ) {
    if ( ! $erasedCollectionIteratorGate.Contains($required) ) {
        throw "XLIR statically shaped erased collection iterator quality gate is incomplete: $required"
    }
}
if ( ! $languageFrontendSuite.Contains(
        "tools\check_xlir_erased_collection_iterator_abi.ps1") ) {
    throw "Language frontend suite does not execute the erased collection iterator ABI gate"
}
foreach ( $required in @(
    "xlir_dynamic_var_iterator_abi.xl",
    "cgen_for_in_containers", "cgen_for_in_var_non_container",
    "xvalue-iterator-create-checked", "ERROR_TEST",
    "xvalue-iterator-key-dynamic-xvalue",
    "xvalue-iterator-dynamic-logical-xvalue",
    "canonical managed var TypeId", "source-bound cursor",
    "owned-rvalue checked-create release",
    "continue/break/return/finally", "TypeError",
    "ReadAllBytes", "-Werror"
) ) {
    if ( ! $dynamicVarIteratorGate.Contains($required) ) {
        throw "XLIR dynamic var iterator quality gate is incomplete: $required"
    }
}
if ( ! $languageFrontendSuite.Contains(
        "tools\check_xlir_dynamic_var_iterator_abi.ps1") ) {
    throw "Language frontend suite does not execute the dynamic var iterator ABI gate"
}
foreach ( $required in @(
    "xlir_string_iterator_abi.xl", "stdlib_string.xl",
    "string-utf8-value", "string-utf8-index-value",
    "string-snapshot-release", "string-utf8-count", "string-scalar-at",
    "trivial char projection", "checked status edges",
    "copy-vs-transfer acquisition semantics",
    "empty/continue/break/return/finally/throw cleanup",
    "emoji runtime semantics", "ReadAllBytes", "-Werror"
) ) {
    if ( ! $stringIteratorGate.Contains($required) ) {
        throw "XLIR UTF-8 string iterator quality gate is incomplete: $required"
    }
}
if ( ! $languageFrontendSuite.Contains(
        "tools\check_xlir_string_iterator_abi.ps1") ) {
    throw "Language frontend suite does not execute the UTF-8 string iterator ABI gate"
}
foreach ( $required in @(
    "char_bytes_literals.xl", "char_bytes_advanced.xl",
    "cAbi -ne 43", "cAbi -ne 44", "unicode-scalar",
    "string-scalar-set-copy", "bytes-set-copy", "bytes-index-value",
    "xvalue-char", "persistence-xlon-stringify",
    "future<char>", "generator<bytes>",
    "char_bytes_invalid_char_width.xl",
    "char_bytes_invalid_char_surrogate.xl",
    "char_bytes_invalid_bf_prefix.xl",
    "char_bytes_invalid_brf_prefix.xl",
    "formatted backticks must preserve ordinary escapes",
    "formatted triple strings must use common-indent framing",
    "SequenceEqual", "-Werror", "XL6_CGEN_LINKS"
) ) {
    if ( ! $charBytesAbiGate.Contains($required) ) {
        throw "XLIR char/bytes ABI quality gate is incomplete: $required"
    }
}
if ( ! $languageFrontendSuite.Contains(
        "tools\check_xlir_char_bytes_abi.ps1") ) {
    throw "Language frontend suite does not execute the char/bytes ABI gate"
}
foreach ( $required in @(
    "xlir_protocol_iterator_managed_item.xl",
    "extlib_native_iter_protocol_min.xl", "stdlib_buffer_file_runtime.xl",
    "protocol-iterator-value", "protocol-iterator-index-value",
    "protocol-product-take", "native-backed-class-managed-iterator-release",
    'PSObject.Properties.Name -contains "loopResourceDestroySymbol"',
    "continue/break/return/throw", "direct CALL targets",
    "ReadAllBytes", "-Werror"
) ) {
    if ( ! $protocolIteratorGate.Contains($required) ) {
        throw "XLIR canonical class protocol iterator quality gate is incomplete: $required"
    }
}
if ( ! $languageFrontendSuite.Contains(
        "tools\check_xlir_protocol_iterator_abi.ps1") ) {
    throw "Language frontend suite does not execute the class/native protocol iterator ABI gate"
}
foreach ( $required in @(
    "protocol_inherited_dispatch", "protocol_native_backed_dispatch",
    "implementationDeclaration", "implementationSymbol",
    "native-class-handle-borrow", "nominalIdentitySets",
    "byte-for-byte deterministic", "strict runtime",
    "strict runtime"
) ) {
    if ( ! $protocolWitnessAdapterGate.Contains($required) ) {
        throw "XLIR inherited/native protocol witness adapter gate is incomplete: $required"
    }
}
if ( ! $languageFrontendSuite.Contains(
        "tools\check_xlir_protocol_witness_adapter_abi.ps1") ) {
    throw "Language frontend suite does not execute the protocol witness adapter ABI gate"
}
foreach ( $required in @(
    "xlir_dynamic_typed_collection_projection.xl",
    "cgen_dynamic_to_typed_containers.xl",
    "xvalue-collection-checked-cast",
    "xvalue-collection-checked-rebind",
    "xvalue-type-rebind-owned", "xvalue-clone",
    "runtimeIdentityType", "xrtValueTypeIdRebind",
    "ReadAllBytes", "-Werror", "runtime contract"
) ) {
    if ( ! $collectionTypeRebindGate.Contains($required) ) {
        throw "XLIR checked collection TypeId rebind quality gate is incomplete: $required"
    }
}
if ( ! $languageFrontendSuite.Contains(
        "tools\check_xlir_collection_type_rebind.ps1") ) {
    throw "Language frontend suite does not execute the checked collection TypeId rebind gate"
}
foreach ( $required in @(
    "typed_nullable_container_matrix.xl",
    "tagged/string/closure/protocol layouts",
    "xvalue-int-map-at-i64-or-null",
    "xvalue-iterator-item-borrow",
    "ownership-only exact-TypeId retain",
    "explicit VALUE rebinds", "resident print ABI",
    "ReadAllBytes", "-Werror"
) ) {
    if ( ! $nullableContainerProjectionGate.Contains($required) ) {
        throw "XLIR nullable container projection quality gate is incomplete: $required"
    }
}
if ( ! $languageFrontendSuite.Contains(
        "tools\check_xlir_nullable_container_projection.ps1") ) {
    throw "Language frontend suite does not execute the nullable container projection gate"
}
foreach ( $required in @(
    "XL6_IR_C_ABI_RUNTIME_VALUE_ITERATOR_PTR",
    "XL6_IR_RUNTIME_XVALUE_ITERATOR_CREATE",
    "XL6_IR_RUNTIME_XVALUE_ITERATOR_ADVANCE",
    "XL6_IR_RUNTIME_XVALUE_ITERATOR_ITEM_I64",
    "XL6_IR_RUNTIME_XVALUE_ITERATOR_KEY_ORDINAL_I64",
    "XL6_IR_RUNTIME_XVALUE_ITERATOR_KEY_I64",
    "XL6_IR_RUNTIME_XVALUE_ITERATOR_ITEM_U64",
    "XL6_IR_RUNTIME_XVALUE_ITERATOR_ITEM_BOOL",
    "XL6_IR_RUNTIME_XVALUE_ITERATOR_ITEM_F64",
    "XL6_IR_RUNTIME_XVALUE_ITERATOR_ITEM_STRING_COPY",
    "XL6_IR_RUNTIME_XVALUE_ITERATOR_ITEM_XVALUE_RETAIN",
    "XL6_IR_RUNTIME_XVALUE_ITERATOR_KEY_STRING_COPY",
    "XL6_IR_RUNTIME_XVALUE_ITERATOR_CREATE_CHECKED",
    "XL6_IR_RUNTIME_XVALUE_ITERATOR_KEY_DYNAMIC_XVALUE",
    "XL6_IR_RUNTIME_XVALUE_ITERATOR_DYNAMIC_LOGICAL_XVALUE",
    "xlir_xvalue_iterator_create", "xlir_xvalue_iterator_advance",
    "xlir_xvalue_iterator_item_i64", "xlir_xvalue_iterator_key_ordinal_i64",
    "xlir_xvalue_iterator_key_i64",
    "xlir_xvalue_iterator_item_u64", "xlir_xvalue_iterator_item_bool",
    "xlir_xvalue_iterator_item_f64", "xlir_xvalue_iterator_item_string_copy",
    "xlir_xvalue_iterator_item_xvalue_retain",
    "xlir_xvalue_iterator_key_string_copy",
    "xlir_xvalue_iterator_create_checked",
    "xlir_xvalue_iterator_key_dynamic_xvalue",
    "xlir_xvalue_iterator_dynamic_logical_xvalue",
    "xlir_xvalue_iterator_destroy", "xrtValueIterAdvance"
) ) {
    if ( ! $cEmitter.Contains($required) ) {
        throw "XLIR C emitter is missing mechanical xvalue iterator emission: $required"
    }
}
foreach ( $required in @(
    "xl6IrLowerArrayI64For", "xl6IrLowerFor",
    "XL6_IR_RUNTIME_XVALUE_CLONE", "XL6_IR_RUNTIME_XVALUE_COUNT",
    "XL6_IR_RUNTIME_XVALUE_ARRAY_AT_I64_INTEGER",
    "snapshotSymbol", "conditionBlock", "incrementBlock", "exitBlock",
    "XL6_IR_INSTR_RELEASE",
    "xl6IrLowerPushLoopResource", "xl6IrLowerPopLoopResource",
    "xl6IrLowerEmitUnwindRoute",
    "xl6IrLowerEmitLoopCleanup",
    "xl6IrLowerEmitLoopResourcesForLoopExit",
    "xl6IrLowerEmitPendingExceptionRoute", "xl6IrTryExceptionPlanGet",
    "XLIR array loop has no admitted storage-backed iterator ABI"
) ) {
    if ( ! $lower.Contains($required) ) {
        throw "XLIR lowering is missing canonical storage-backed array<i64> loop ABI: $required"
    }
}
foreach ( $required in @(
    "xlir_range_for_abi.xl", "cgen_range_assert.xl", "loopAbiName",
    "range-i64-index-value", "loopErrorAbiName", "evaluate-once operands",
    "dynamic-zero RangeError CFG", "mechanical C", "-Werror"
) ) {
    if ( ! $rangeForAbiGate.Contains($required) ) {
        throw "XLIR range-for ABI quality gate is incomplete: $required"
    }
}
foreach ( $required in @(
    "xlir_array_i64_for_abi.xl",
    "cgen_xlir_array_i64_for_escape_reject.xl",
    "cgen_export_typed_container_abi.xl",
    "xvalue-array-i64-value", "xvalue-array-i64-index-value",
    "xvalue-shallow-snapshot-release", "xvalue-owned-iterator-release",
    "xvalue-clone", "xvalue-count", "xvalue-array-at-i64-integer",
    "exact named and owned-rvalue admission", "clone-or-transfer acquisition",
    "unique canonical exit release", "call/return admission",
    "local and outward exception routing", "return/throw/finally cleanup",
    "non-container excluded from array ABI",
    "runtime snapshot/transfer semantics", "mechanical C", "strict compile",
    "check_stdlib_collection_continuations.py", "--array-only"
) ) {
    if ( ! $arrayI64ForAbiGate.Contains($required) ) {
        throw "XLIR array<i64> for ABI quality gate is incomplete: $required"
    }
}
foreach ( $required in @("'-std=c11'", "'-Wall'", "'-Wextra'", "'-Werror'",
    "for level in [0,2]", "InputsUnchanged", "--array-only", "--multi-only", "--strings-only") ) {
    if ( ! $collectionContinuationGate.Contains($required) ) {
        throw "XLIR collection continuation strict-C child gate is incomplete: $required"
    }
}
foreach ( $required in @(
    "XL6_IR_RUNTIME_STRING_CONTAINS",
    "managed-string haystack",
    "separate from erased xvalue"
) ) {
    if ( ! $header.Contains($required) ) {
        throw "XLIR public model is missing managed-string membership ABI: $required"
    }
}
foreach ( $required in @(
    "XL6_IR_RUNTIME_STRING_CONTAINS",
    "xrtStrContains(v%u, v%u)",
    "needsStringContainsOperation"
) ) {
    if ( ! $cEmitter.Contains($required) ) {
        throw "XLIR C emitter is missing mechanical managed-string membership emission: $required"
    }
}
foreach ( $required in @(
    "XL6_IrMemberAbi", "XL6_IR_MEMBER_ABI_STRING_UTF8_COUNT",
    "XL6_IR_MEMBER_ABI_BYTES_LENGTH", "memberAbi",
    "XL6_IR_RUNTIME_STRING_UTF8_COUNT", "XL6_IR_RUNTIME_BYTES_LENGTH",
    "string.length", "bytes.length", "semantic ABI"
) ) {
    if ( ! $header.Contains($required) ) {
        throw "XLIR public model is missing canonical non-call member ABI: $required"
    }
}
foreach ( $required in @(
    "xl6IrClassifyMemberAbi", "xl6IrBuildMemberAbis",
    "typed XLIR member has no exact canonical member ABI",
    'memberAbiName\":\"%s', "string-utf8-count", "bytes-length",
    "XLIR runtime UTF-8 string count has no exact borrowed string or i64 result ABI"
) ) {
    if ( ! $source.Contains($required) ) {
        throw "XLIR verifier/dump is missing canonical non-call member ABI: $required"
    }
}
foreach ( $required in @(
    "xl6IrLowerBuiltinSequenceLength", "node->memberAbi",
    "XL6_IR_RUNTIME_STRING_UTF8_COUNT", "XL6_IR_RUNTIME_BYTES_LENGTH"
) ) {
    if ( ! $lower.Contains($required) ) {
        throw "XLIR lowering is missing canonical string length member ABI: $required"
    }
}
foreach ( $required in @(
    "XL6_IR_RUNTIME_STRING_UTF8_COUNT",
    "xrtUtf8Count(v%u)", "needsStringUtf8CountOperation",
    "XL6_IR_RUNTIME_BYTES_LENGTH", "xlir_bytes_length"
) ) {
    if ( ! $cEmitter.Contains($required) ) {
        throw "XLIR C emitter is missing mechanical UTF-8 string count emission: $required"
    }
}
if ( $cEmitter.Contains("memberAbi") ) {
    throw "XLIR C emitter must consume only the lowered string UTF-8 count operation"
}
foreach ( $required in @(
    "xlir_member_abi_string_length.xl", "memberAbiName",
    "string-utf8-count", "one-borrow/i64", "xrtUtf8Count",
    "Unicode runtime semantics", "-Werror"
) ) {
    if ( ! $memberAbiGate.Contains($required) ) {
        throw "XLIR member ABI quality gate is incomplete: $required"
    }
}
foreach ( $required in @(
    "XL6_IR_MEMBER_ABI_LENGTH_PROTOCOL_CALL", "targetSymbol",
    "zero-argument", "__len__", "class protocol"
) ) {
    if ( ! $header.Contains($required) ) {
        throw "XLIR public model is missing canonical length protocol member ABI: $required"
    }
}
foreach ( $required in @(
    "xl6IrLengthProtocolTarget", "length-protocol-call",
    "xl6IrClassDeclarationHasFieldNamedFrom"
) ) {
    if ( ! $source.Contains($required) ) {
        throw "XLIR verifier/dump is missing canonical length protocol member ABI: $required"
    }
}
foreach ( $required in @(
    "xl6IrLowerLengthProtocolCall",
    "XLIR length protocol member has no exact receiver/call ABI",
    "XL6_IR_INSTR_CALL"
) ) {
    if ( ! $lower.Contains($required) ) {
        throw "XLIR lowering is missing canonical length protocol member ABI: $required"
    }
}
foreach ( $required in @(
    "xlir_length_protocol_member.xl", "stdlib_buffer.xl",
    "length-protocol-call", "one exact direct CALL",
    "source-class/native-class", "-Werror"
) ) {
    if ( ! $lengthProtocolMemberGate.Contains($required) ) {
        throw "XLIR length protocol member quality gate is incomplete: $required"
    }
}
foreach ( $required in @(
    "XL6_IR_MEMBER_ABI_CLASS_FIELD_BOOL",
    "XL6_IR_MEMBER_ABI_CLASS_FIELD_I64",
    "XL6_IR_MEMBER_ABI_CLASS_FIELD_U64",
    "XL6_IR_MEMBER_ABI_CLASS_FIELD_F64",
    "XL6_IR_MEMBER_ABI_CLASS_FIELD_STRING_COPY",
    "XL6_IR_MEMBER_ABI_CLASS_FIELD_RAW_POINTER_BORROW",
    "XL6_IR_MEMBER_ABI_CLASS_FIELD_NATIVE_POINTER_BORROW",
    "XL6_IR_MEMBER_ABI_CLASS_FIELD_NATIVE_HANDLE_BORROW",
    "XL6_IR_RUNTIME_XVALUE_RAW_POINTER_BORROW"
) ) {
    if ( ! $header.Contains($required) ) {
        throw "XLIR public model is missing canonical class-field member ABI: $required"
    }
}
foreach ( $required in @(
    "xl6IrClassFieldForMemberAbi", "class-field-native-pointer-borrow",
    "class-field-native-handle-borrow",
    "XLIR runtime typed class field projection has no exact member, object or result ABI",
    "XLIR raw pointer dynamic bridge has no exact borrowed ABI"
) ) {
    if ( ! $source.Contains($required) ) {
        throw "XLIR verifier/dump is missing canonical class-field member ABI: $required"
    }
}
foreach ( $required in @(
    "xl6IrLowerClassFieldProject", "xl6IrRefreshMemberAbis", "node->memberAbi",
    "XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_NATIVE_POINTER_BORROW",
    "XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_NATIVE_HANDLE_BORROW",
    "XL6_IR_RUNTIME_XVALUE_RAW_POINTER_BORROW"
) ) {
    if ( ! $lower.Contains($required) ) {
        throw "XLIR lowering is missing canonical class-field member ABI: $required"
    }
}
foreach ( $required in @(
    "xlir_xvalue_object_get_literal_bool",
    "xlir_xvalue_object_get_literal_f64",
    "xlir_xvalue_object_get_literal_raw_pointer",
    "xlir_xvalue_object_get_literal_native_pointer",
    "xlir_xvalue_object_get_literal_native_handle"
) ) {
    if ( ! $cEmitter.Contains($required) ) {
        throw "XLIR C emitter is missing mechanical class-field projection: $required"
    }
}
foreach ( $required in @(
    "xlir_class_field_member_abi.xl", "class-field-raw-pointer-borrow",
    "class-field-xvalue-borrow", "xvalue-raw-pointer-borrow",
    "mechanical helper", "ownership runtime semantics", "-Werror"
) ) {
    if ( ! $classFieldMemberAbiGate.Contains($required) ) {
        throw "XLIR class-field member ABI quality gate is incomplete: $required"
    }
}
foreach ( $required in @(
    "XL6_IR_RUNTIME_XVALUE_CONTAINS",
    "Borrows one erased container",
    "neither operand is consumed"
) ) {
    if ( ! $header.Contains($required) ) {
        throw "XLIR public model is missing xvalue collection membership ABI: $required"
    }
}
foreach ( $required in @(
    "XL6_IR_RUNTIME_XVALUE_CONTAINS",
    "xlir_xvalue_contains",
    "if (valuesType == 11)",
    "if (valuesType == 12)",
    "if (valuesType == 4)",
    "valuesType != 9 && valuesType != 10",
    "xrtValueIterCreate",
    "xrtValueEqual"
) ) {
    if ( ! $cEmitter.Contains($required) ) {
        throw "XLIR C emitter is missing mechanical xvalue membership emission: $required"
    }
}
foreach ( $required in @(
    "XL6_IR_RUNTIME_XVALUE_INDEX_GET_XVALUE_KEY",
    "XL6_IR_RUNTIME_XVALUE_INDEX_SET_XVALUE_KEY_NEW",
    "xlir_xvalue_index_get_xvalue_key_or_null",
    "xl6ValueIndexSetTakeV1",
    "int collectionType = xrtValueType(collection); int keyType = xrtValueType(key);",
    "xrtValueRelease(item);"
) ) {
    if ( ! $cEmitter.Contains($required) ) {
        throw "XLIR C emitter is missing mechanical dynamic-key index emission: $required"
    }
}

foreach ( $required in @(
    "xlir_class_implicit_receiver.xl", "parameterAbis", "constructor/getter/setter/method",
    "canonical slot-zero borrow ABI", "-Werror", "generated-C runtime failed"
) ) {
    if ( ! $implicitClassReceiverGate.Contains($required) ) {
        throw "XLIR declaration-driven class receiver gate is incomplete: $required"
    }
}

foreach ( $required in @(
    "xl6IrBuildClassDispatchTables",
    "xl6IrBuildClassDispatchAppendSlot",
    "xl6IrBuildClassDispatchAppendImpl",
    "xl6IrTypeInternCell",
    "XLIR cell-backed storage must use explicit CELL operations",
    "XLIR cell allocation must return an independent empty cell owner",
    "XLIR cell initialization has no exact cell and element ABI",
    "XLIR cell store has no exact replacement ABI",
    "XLIR closure capture cell ABI is ambiguous"
) ) {
    if ( ! $source.Contains($required) ) {
        throw "XLIR verifier is missing the canonical mutable-capture cell ABI: $required"
    }
}

if ( $lower -match 'XL6_AstNode\s*\*' -or $lower -match 'XL6_HirModule\s*\*' ) {
    throw "XLIR lowering must not read AST or HIR objects"
}

foreach ( $required in @('Get-Command $Cc', "-Werror", "strict C compile failed", "XLIR Cgen batch found", "byte-for-byte deterministic", "cgen_generic_function", "cgen_generic_dedupe", "xlir_closure_capture", "xlir_closure_return", "xlir_closure_string_capture", "xlir_closure_xvalue_capture", "xlir_closure_future_capture", "xrtValueRetain(v", "xrtFutureRef(v", "materialized borrowed argument", "xlir_string_copy(v", "retain opcode=none", "kind=capture", "genericSpecializations") ) {
    if ( ! $determinism.Contains($required) ) {
        throw "XLIR quality gate is missing deterministic C compilation requirement: $required"
    }
}
foreach ( $required in @("'--backend', 'xlir'", "xlir_cgen_differential_scalar.xl", "xlir_time_conversion_abi.xl", "xlir_cgen_differential_managed_error.xl", "xlir_cgen_differential_xvalue_conditional.xl", "xlir_async_generic_expression_planner.xl", "xlir_async_future_scalar_mixed_capture_chain.xl", "xlir_xvalue_iterator_managed_projection.xl", "xlir_dict_iterator_projection_differential.xl", "xlir_dict_key_only_iterator.xl", "xlir_index_write_generic_abi.xl", "distinct time conversion ABI", "async generic expression planner ABI", "async scalar mixed-capture ABI", "managed list/set iterator target lifetime ABI", "dict key/item iterator projection ABI", "dict key-only iterator projection ABI", "canonical typed index write ABI", "xvalue conditional scalar ABI", "generated-C runtime failed") ) {
    if ( ! $runtimeMatrix.Contains($required) ) {
        throw "XLIR quality gate is missing runtime-matrix requirement: $required"
    }
}

foreach ( $required in @(
    "target->semanticTargetSymbolId = symbol->id",
    "node->semanticTargetSymbolId = sym->id",
    "mainNode->flags |= XL6_AST_FLAG_ENTRYPOINT"
) ) {
    if ( ! $sem.Contains($required) ) {
        throw "semantic analysis must record resolved symbol targets before XLIR lowering: $required"
    }
}
foreach ( $required in @(
    "exported->returnAbi", "exported->returnType",
    "exported->parameterCount", "exported->parameterTypes",
    "exported->parameterAbis[parameter] != XL6_IR_PARAMETER_ABI_BORROW"
) ) {
    if ( ! $cEmitter.Contains($required) ) {
        throw "XLIR C emitter is missing the verified public export ABI: $required"
    }
}
foreach ( $required in @(
    "XL6_IR_INSTR_ASYNC_START",
    "XL6_IR_CALL_TARGET_INTERNAL_ASYNC",
    "xl6IrCgenEmitAsyncLauncher",
    "xl6TaskSubmitPayloadV1",
    "xl6TaskRuntimeV1Get()->PoolCreate",
    "xlir_async_pool_submit",
    "xlir_async_pool_quiesce",
    "xl6TaskRuntimeV1Get()->PoolQuiesce",
    "xlir_async_task_",
    "xlir_async_frame_",
    "XL6_FUTURE_PAYLOAD_XVALUE",
    "XL6_FUTURE_PAYLOAD_STRING",
    "xl6IrCgenEmitActivationCapture",
    "xl6IrCgenEmitActivationInputRollback",
    "XL6_IR_ASYNC_FRAME_TRANSFER_TAKE_PARAMETER",
    "XL6_IR_ASYNC_CAPTURE_COPY_VALUE",
    "XL6_IR_ASYNC_FRAME_TRANSFER_RETAIN_FUTURE",
    "xl6IrCgenAsyncMachine",
    "xl6IrCgenEmitAsyncMachine",
    "xl6IrCgenEmitAsyncMachineInstruction",
    "XL6_IrAsyncMachineFrameSlot",
    "XL6_IrAsyncFrameAction",
    "target->asyncMachine",
    "machine->suspensionCount",
    "XL6_TaskResult",
    "xrtFutureRef",
    "xrtFutureThenOwned",
    "xrtFutureContinueOwned",
    "xrtPromiseRef",
    "xrtPromiseReject",
    "xrtPromiseCancel",
    "xrtPromiseResolveOwned",
    "xlir_async_machine_resume_",
    "frame->f%lu",
    "XLIR C emitter async worker error-result ABI is unresolved",
    "target->asyncCompletionAbi",
    "xlir_async_frame_retire_%u(frame, outcome)",
    "outcome.ok = worker.ok;",
    "xrtFutureError(future)",
    "xl6PayloadObserveOutcome",
    "XLIR verified async machine produced no C program",
    "xl6IrCgenEmitOwnedPlaceRelease",
    "xl6IrCgenEmitActivationCleanupBegin",
    "xl6IrCgenEmitActivationCleanupObserve",
    "XLIR C emitter async launch ABI is unresolved"
) ) {
    if ( ! $cEmitter.Contains($required) ) {
        throw "XLIR C emitter is missing the explicit async task ABI contract: $required"
    }
}
if ( $header -match '(?s)typedef struct XL6_IrAsyncFrameSlot \{.*?resumeOpcode.*?\} XL6_IrAsyncFrameSlot' -or
     $header -match '(?s)typedef struct XL6_IrAsyncFrameSlot \{.*?resumeAwaitOnLeft.*?\} XL6_IrAsyncFrameSlot' ) {
    throw "XLIR async frame layout still embeds resume expression semantics"
}
if ( ! $sem.Contains("case XL6_AST_ASSERT") -or
     ! $sem.Contains("node->semanticExitKind = XL6_SEM_EXIT_THROW") ) {
    throw "semantic analysis must assign assertions a resolved throw exit before XLIR lowering"
}

foreach ( $required in @("xl6IrCgenEmitC", "xl6IrCgenEmitHeader", "xl6IrCgenVerifyEmittable", "XL6_IR_STAGE_LOWERED", "XL6_IR_INSTR_BINARY", "XL6_IR_INSTR_VALUE", "XL6_IR_INSTR_DISCARD", "XL6_IR_INSTR_RUNTIME_CALL", "XL6_IR_INSTR_TRAP_IF_FALSE", "XL6_IR_INSTR_ERROR_RAISE", "XL6_IR_INSTR_ERROR_TAKE", "XL6_IR_INSTR_ERROR_SET_TAKE", "XL6_IR_INSTR_AWAIT", "XL6_IR_RUNTIME_STRING_DUP", "XL6_IR_RUNTIME_STRING_COPY", "XL6_IR_RUNTIME_STRING_CONCAT", "XL6_IR_RUNTIME_STRING_EQUAL", "XL6_IR_RUNTIME_XVALUE_I64", "XL6_IR_RUNTIME_XVALUE_STRING_COPY", "XL6_IR_RUNTIME_XVALUE_RETAIN", "XL6_IR_RUNTIME_XVALUE_COUNT", "XL6_IR_RUNTIME_XVALUE_ARRAY_NEW", "XL6_IR_RUNTIME_XVALUE_ARRAY_AT_I64", "XL6_IR_RUNTIME_XVALUE_ARRAY_AT_I64_INTEGER", "XL6_IR_RUNTIME_XVALUE_ARRAY_AT_I64_BOOL", "XL6_IR_RUNTIME_XVALUE_ARRAY_AT_I64_F64", "XL6_IR_RUNTIME_XVALUE_ARRAY_AT_I64_STRING_COPY", "XL6_IR_RUNTIME_XVALUE_ARRAY_APPEND_NEW", "XL6_IR_RUNTIME_XVALUE_ARRAY_SET_I64_NEW", "XL6_IR_RUNTIME_XVALUE_INT_MAP_NEW", "XL6_IR_RUNTIME_XVALUE_INT_MAP_SET_I64_NEW", "XL6_IR_RUNTIME_XVALUE_SET_NEW", "XL6_IR_RUNTIME_XVALUE_SET_ADD_NEW", "XL6_IR_RUNTIME_XVALUE_OBJECT_NEW", "XL6_IR_RUNTIME_XVALUE_OBJECT_SET_LITERAL_NEW", "XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL", "XL6_IR_RUNTIME_XVALUE_OBJECT_GET_STRING", "XL6_IR_RUNTIME_XVALUE_OBJECT_SET_STRING_NEW", "XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_I64", "xlir_xvalue_object_get_literal_i64", "XL6_IR_PARAMETER_ABI_BORROW", "xrtStrDup", "xrtStrConcat", "xrtStrEqual", "xrtStrView", "xrtFree", "xrtValueInt", "xrtValueString", "xrtValueRetain", "xrtValueRelease", "xrtValueCount", "xrtValueArray", "xrtValueArrayAt", "xl6ArrayValueAppendTakeV1", "xl6ArrayReadStringV1", "xl6ArrayValueSetTakeV1", "xrtValueIntMap", "xrtValueIntMapSetNew", "xrtValueSet", "xrtValueSetAddNew", "xrtValueObject", "xrtValueObjectGet", "xrtValueObjectSetNew", "xl6ArrayReadI64V1", "xl6ArrayReadBoolV1", "xl6ArrayReadF64V1", "xlir_xvalue_object_get_literal_or_null", "xlir_xvalue_object_get_string_or_null", "xrtFutureDestroy", "xrtFutureWait", "xrtFutureValue", "xrtGetError", "xl6IrCgenEmitAwaitPayload", "abort", "xl6IrCgenOperator", "item->cAbi", "instruction->opcode", "instruction->immediateText", "xl6IrCgenFindFunction", "xl6IrCallTargetGet", "xl6IrCgenEmitExternPrototype", "xl6IrCgenEmitModuleEntry", "xl6IrCgenEmitExport", "XL6_IR_CALL_TARGET_EXTERN_C", "XL6_IR_MODULE_ENTRY_XLANG_MAIN", "xlang_main", "typedef struct xvalue xvalue;", "typedef struct xfuture xfuture;", "xlir_result_void", "xlir_module_init", "xrtTakeError", "xrtSetErrorTake", "xrtErrorFree", "xl6IrCgenErrorStateInstructionAbi", "xl6IrCgenEmitErrorStateInstruction", "XLIR error-state ownership ABI is unresolved", "XLIR C emitter export linkage ABI is unresolved", "XLIR C header export linkage ABI is unresolved", "XLIR module initializer return ABI is not defined", "XLIR C emitter runtime exception ABI is not implemented", "xl6IrCgenErrorProjectionAbi", "XLIR C emitter ownership runtime ABI is not implemented", "XLIR C emitter await result ABI is not implemented", "XLIR lowered cleanup action has no materialized ownership instruction", "XLIR constant has no canonical immediate payload", "XLIR value copy has no deterministic definition or merge target", "XLIR discard has no deterministic non-owning observation ABI", "XLIR void function return carries a value", "XLIR C emitter may-raise call has no result carrier ABI", "XLIR C emitter call target or argument ABI is unresolved", "XLIR C emitter runtime trap ABI is unresolved", "XLIR C emitter has no deterministic binary operator rule", "XLIR C emitter has no rule for this lowered instruction") ) {
    if ( ! $cEmitter.Contains($required) -and
         ! ($required -eq "XLIR lowered cleanup action has no materialized ownership instruction" -and
            $source.Contains($required)) ) {
        throw "XLIR C emitter is missing deterministic backend contract: $required"
    }
}
if ( ! $cEmitter.Contains("xl6IrStorageGet(module, symbol)") ) {
    throw "XLIR C emitter must resolve specialized parameters from lowered storage ABI"
}
foreach ( $required in @(
    "xl6IrCgenClosureTargetGet",
    "xl6IrCgenCaptureGet",
    "xlir_closure_env_",
    "xlir_closure_layout_",
    "XL6_ClosureCaptureSlot",
    "XL6_ClosureEnvironmentV1",
    "XL6_IR_STORAGE_CAPTURE",
    "XLIR mutable closure capture requires an explicit cell ABI"
) ) {
    if ( ! $cEmitter.Contains($required) ) {
        throw "XLIR C emitter is missing the explicit closure environment ABI contract: $required"
    }
}
# Storage belongs to the resident ABI now, not a generated replica. Preserve
# the environment duty check at its actual definition and forbid duplication.
$closureHeader = Read-Utf8 'src/runtime/xl6_closure.h'
$closureRuntime = Read-Utf8 'src/runtime/xl6_closure.c'
foreach ($required in @('environment_free', 'environment_trace', 'XL6_CodeLease* lease', 'XL6_ClosureRuntimeV1')) {
    if (!$closureHeader.Contains($required)) { throw "Resident closure physical ABI missing: $required" }
}
foreach ($required in @('xl6CodeLeaseAcquireScoped', 'closure_enter', 'closure_leave', 'box_invoke', 'box_drop',
    'box->adapter', 'xl6CodeLeaseOwnership', 'xl6ClosureRuntimeV1Get')) {
    if (!$closureRuntime.Contains($required)) { throw "Resident closure lifetime contract missing: $required" }
}
if ($cEmitter.Contains('struct xlir_closure {') -or $cEmitter.Contains('static void xlir_closure_release(') -or
    $cEmitter.Contains('xlir_closure_env_free_') -or $cEmitter.Contains('xlir_closure_env_trace_') -or
    !$cEmitter.Contains('__xl_runtime/xl6_closure.h') -or !$cEmitter.Contains('xl6ClosureRuntimeV2Get()->Create') -or
    !$header.Contains('closureStorageAbi') -or !$source.Contains('closure storage/code ownership ABI is unresolved')) {
    throw 'Closure storage must be selected by verified IR and implemented once in the resident runtime'
}
foreach ($required in @('XL6_ClosureRuntimeV2', 'XL6_ClosureEnvironmentV1', 'XL6_ClosureCaptureSlot', 'CreateValue')) {
    if (!$closureHeader.Contains($required)) { throw "Closed closure layout ABI missing: $required" }
}
foreach ($required in @('environment_valid', 'environment_drop_body', 'closure_claim', 'closure_clear',
    'closure_finish', 'xl6ClosureOwnershipAdapterV1')) {
    if (!$closureRuntime.Contains($required)) { throw "Resident capture retirement contract missing: $required" }
}
if (!$header.Contains('XL6_IR_CLOSURE_STORAGE_RESIDENT_LAYOUT_V2') -or !$header.Contains('retirementAbi') -or
    !$source.Contains('XLIR module retirement ABI is unresolved') -or !$source.Contains('xl_script_module_retirement_v1') -or
    !$cEmitter.Contains('xl_script_module_retirement_v1(void)')) {
    throw 'Closed captures and checked module retirement must be explicit verified IR contracts'
}
if (!$header.Contains('XL6_IR_MODULE_RETIREMENT_PREPARED_GRAPH_V2') -or
    !$source.Contains('xl_script_module_retirement_v2') -or
    !$cEmitter.Contains('xl_script_module_retirement_v2(void)') -or
    !$cEmitter.Contains('xlir_module_prepare_resources') -or
    !$cEmitter.Contains('xl6TaskPoolModulePrepareV1(xlir_async_pool, teardown, wait)') -or
    !$cEmitter.Contains('xlir_module_prepare_resources(false, false)') -or
    !$cEmitter.Contains('xlir_module_prepare_resources(true, wait)') -or
    !$cEmitter.Contains('xlir_module_prepare_resources(true, true)')) {
    throw 'Module retirement preparation must be verified, shared with teardown, and precede graph admission'
}
if (!$header.Contains('XL6_IR_MODULE_EXECUTOR_OWNERSHIP_RESIDENT_WAIT_SLOTS_JOIN_V1') -or
    !$header.Contains('XL6_IR_MODULE_RETIREMENT_OWNED_SERVICES_GRAPH_V3') -or
    !$source.Contains('xl6IrModuleExecutorOwnershipRequirement(module)') -or
    !$source.Contains('XLIR module executor ownership ABI is unresolved') -or
    !$source.Contains('xl_script_module_retirement_v3') -or
    !$cEmitter.Contains('xl_script_module_retirement_v3(void)') -or
    !$cEmitter.Contains('visit(xl6ContinuationExecutorOwnership(xlir_continuation_executor), context)') -or
    !$cEmitter.Contains('if (teardown && !xlir_continuation_quiesce(wait))') -or
    !$cEmitter.Contains('xl6ContinuationExecutorDestroyCompleted(xlir_continuation_executor)') -or
    $cEmitter.Contains('if (!teardown && !xl6ContinuationExecutorIsIdle(xlir_continuation_executor))') -or
    $cEmitter.Contains('if (xlir_continuation_executor != NULL) return false;') -or
    !$api.Contains('((getV2 != NULL) + (getV3 != NULL) + (getV4 != NULL) <= 1)') -or
    !$api.Contains('preflight->graph == contract && preflight->preflight') -or
    !$api.Contains('module->retirementPreflight->preflight()')) {
    throw 'Owned executors require exact verified IR trace, exclusive V3 preflight and independent graph preparation; V2 drain semantics must remain distinct'
}
$retryableServices = Read-Utf8 'src/runtime/xl6_network_service.c'
$ownershipPlanHeader = Read-Utf8 'src/runtime/xl6_ownership_plan.h'
$ownershipPlanSource = Read-Utf8 'src/runtime/xl6_ownership_plan.c'
$moduleTrace = Read-Utf8 'src/cgen/xl6_ir_cgen_module_trace.c'
if (!$header.Contains('XL6_IR_MODULE_NETWORK_SERVICE_RESIDENT_RETRYABLE_V1') -or
    !$header.Contains('XL6_IR_MODULE_RETIREMENT_RETRYABLE_SERVICES_V4') -or
    !$source.Contains('xl6IrModuleNetworkServiceRequirement(module)') -or
    !$source.Contains('XLIR module network service ABI is unresolved') -or
    !$cEmitter.Contains('xl_script_module_retirement_v4(void)') -or
    !(Read-Utf8 'src/cgen/xl6_ir_cgen_function_shell.c').Contains('xlir_script_module_retire_step(void)') -or
    !$cEmitter.Contains('xl6NetworkServiceRuntimeV2Get()->Retire') -or
    !$moduleTrace.Contains('xl6NetworkServiceRuntimeV2Get()->Trace') -or
    $moduleTrace.IndexOf('xl6NetworkServiceRuntimeV2Get()->Trace') -gt $moduleTrace.IndexOf('xlir_module_initialized == 0') -or
    !$api.Contains('invalid module retirement V4 contract') -or
    !$api.Contains('xl6OwnershipPlanTryCreateV6') -or !$api.Contains('xl6OwnershipPlanFinishV2') -or
    !$api.Contains('retired == XL6_MODULE_RETIRE_BUSY') -or
    !$ownershipPlanHeader.Contains('XL6_OwnershipFinishV1') -or
    !$ownershipPlanSource.Contains('old->completion!=completion') -or
    !$ownershipPlanSource.Contains('status==XRT_OWNERSHIP_FINISH_READY)node->finished=true') -or
    !$retryableServices.Contains('xrtNetEngineTryDestroy') -or !$retryableServices.Contains('xrtNetResolverTryDestroy') -or
    !$retryableServices.Contains('if (result != XNET_RETIRE_READY) return translate(result);')) {
    throw 'Retryable services need verified IR, retained physical slots, pre-trace Finish admission and typed host completion; phase zero and BUSY do not permit unloading'
}
if (!$header.Contains('XL6_IR_MODULE_TASK_POOL_OWNERSHIP_RESIDENT_NATIVE_JOBS_JOIN_V1') -or
    !$source.Contains('xl6IrModuleTaskPoolOwnershipRequirement(module)') -or
    !$source.Contains('XLIR module task pool ownership ABI is unresolved') -or
    !$cEmitter.Contains('visit(xl6TaskPoolOwnership(xlir_async_pool), context)') -or
    $cEmitter.Contains('xl6TaskPoolTryQuiesceV1(xlir_async_pool)') -or
    !$cEmitter.Contains('xl6TaskPoolModulePrepareV1(xlir_async_pool, teardown, wait)') -or
    $cEmitter.Contains('if (!xl6TaskRuntimeV1Get()->PoolIsClosing(xlir_async_pool) ||') -or
    !(Read-Utf8 'src/runtime/xl6_ownership_domain_task_pool.c').Contains('xl6TaskPoolOwnershipAdapterV2(ref,policies->TaskPoolExit,policies->TaskPoolExitCount,preparation)') -or
    !(Read-Utf8 'src/runtime/xl6_ownership_domain_task_pool.c').Contains('xrtTaskPoolOwnershipAdapterV2(ref,policies->TaskPoolExit,policies->TaskPoolExitCount,preparation)')) {
    throw 'Task-pool ownership requires independently verified IR, actual wrapper/native edges, typed exit policies and non-draining V4 preflight'
}
foreach ( $required in @(
    "XL6_IR_INSTR_RETAIN",
    "xlir_closure_retain",
    "XLIR C emitter retain ABI is unresolved"
) ) {
    if ( ! $cEmitter.Contains($required) ) {
        throw "XLIR C emitter is missing the explicit closure shared-ownership ABI: $required"
    }
}

foreach ( $required in @(
    "XL6_IR_INSTR_ERROR_TEST",
    "xl6IrCgenErrorResultType",
    "xl6IrCgenValueDefinition",
    "xlir_result_i64",
    "xlir_result_u64",
    "xlir_result_string",
    "xlir_result_xvalue",
    "xrtErrorCreate",
    "xrtSetErrorTake",
    "xrtClearError",
    "XLIR C emitter may-raise call has no result carrier ABI",
    "XLIR C emitter export error-result adapter ABI is unresolved"
) ) {
    if ( ! $cEmitter.Contains($required) ) {
        throw "XLIR C emitter is missing the fixed error-result ABI contract: $required"
    }
}
foreach ( $required in @(
    "ERROR_RAISE consumes that owner",
    "xl6IrContinuationResidentValue",
    "xl6IrInstructionOperandOwnership"
) ) {
    if ( ! $header.Contains($required) -and ! $source.Contains($required) ) {
        throw "XLIR error-raise ownership contract is incomplete: $required"
    }
}
foreach ( $required in @(
    "XLIR throw payload has no owned error-raise transfer ABI",
    "XLIR assertion message has no owned error-raise transfer ABI",
    "xl6IrLowerOwnedManagedStringValue"
) ) {
    if ( ! $lower.Contains($required) ) {
        throw "XLIR lowering is missing owned error-raise transfer: $required"
    }
}
foreach ( $required in @(
    "instruction->kind != XL6_IR_INSTR_ERROR_RAISE",
    "xl6IrValueOwnershipGet",
    "xlir_string_drop(v%u);"
) ) {
    if ( ! $source.Contains($required) -and ! $cEmitter.Contains($required) ) {
        throw "XLIR verifier/C emitter is missing error-raise ownership consumption: $required"
    }
}
foreach ( $required in @(
    "XL6_IR_INSTR_AWAIT",
    "xl6IrCgenEmitAwaitPayload",
    "XL6_IR_AWAIT_OUTCOME_STATUS_DIAGNOSTIC_NULL_STATE_ERROR",
    "await_ok_%u",
    "xl6IrCgenBlockingAwaitAbi",
    "xlir_future_wait_checked",
    "xrtFutureWait",
    "xrtFutureValue"
) ) {
    if ( ! $cEmitter.Contains($required) ) {
        throw "XLIR C emitter is missing the scalar future await ABI contract: $required"
    }
}
if ( $cEmitter -match 'XL6_AstNode\s*\*' -or $cEmitter -match 'XL6_Hir' -or
$cEmitter -match 'xl6Cgen(?!Emit(?:BufferRuntime|FileRuntime|NetTcpComponent|NetBufferComponent|NetTcpStatsComponent|NetDatagramComponent|TlsRuntime|Http1Runtime|HttpDecodeRuntime|WebSocketProtocolRuntime|Base64TextCodecRuntime|HexBytesCodecRuntime|Base64BytesCodecRuntime|TextOutputRuntime|DirRuntime|ProcessRunRuntime|JsonValueRuntime|JsonStreamRuntime|SerializationCallbackRuntime|StructuredOptionsRuntime|NativeVoidAbiBegin|NativeVoidAbiEnd|XlonRuntime)\b)' -or
     $cEmitter -match '\bxl6Sem(?!(?:CreateV1|CreateDefaultV1|WaitV1|TryWaitV1|WaitTimeoutV1|PostV1|PostMultipleV1)\b)' ) {
    throw "XLIR C emitter must not consume AST/HIR/semantic analysis or call legacy Cgen"
}
$datagramComponent = Read-Utf8 'src/cgen/xl6_cgen_net_datagram.c'
if ($datagramComponent -match 'XL6_AstNode|XL6_Hir|XL6_CgenContext|xl6Sem|xl6CgenFindClass|xl6CgenNodeHasExternSymbol') {
    throw 'Shared datagram provider must remain independent of AST, semantics and legacy Cgen'
}
foreach ($required in @('xl6CgenEmitNetDatagramComponent', 'xlir_datagram_buffer_borrow',
    'bool xl6_xrt_udp_send_vec', 'void xl6_xrt_udp_destroy', 'int64_t xl6_xrt_udp_local_port')) {
    if (!$datagramComponent.Contains($required)) { throw "Datagram provider contract missing: $required" }
}
if ( $cEmitter -notmatch 'xl6CgenEmitFileRuntime\s*\(\s*out,\s*xl6IrCgenNeedsXrtDeclarations\(&needs\)\s*\)' -or
     $cEmitter -notmatch 'xl6CgenEmitBufferRuntime\s*\(\s*out,\s*needs\.needsFileBridgeRuntime,\s*xl6IrCgenNeedsXrtDeclarations\(&needs\)\s*\)' ) {
    throw "XLIR C emitter lost the canonical File/Buffer components"
}
$retiredComponentAbiFiles = @(
    'src/cgen/xl6_cgen_buffer.c',
    'src/cgen/xl6_cgen_file.c',
    'src/cgen/xl6_cgen_http1.c',
    'src/cgen/xl6_cgen_json_stream.c',
    'src/cgen/xl6_cgen_serialization_callback.c',
    'src/cgen/xl6_cgen_tls_stream.c',
    'src/cgen/xl6_cgen_websocket.c',
    'src/cgen/xl6_cgen_xlang_runtime.c',
    'src/cgen/xl6_cgen_xlon.c'
)
foreach ($path in $retiredComponentAbiFiles) {
    $componentText = Read-Utf8 $path
    if ($componentText -match '\bxlirAbi\b|xl6TlsLegacyFutureSource|XL6_CGEN_TLS_FUTURE_LEGACY') {
        throw "Retired backend ABI selection returned to the canonical runtime component: $path"
    }
}
$netTransportComponent = Read-Utf8 'src/cgen/xl6_cgen_net_transport.c'
if ($netTransportComponent -match 'XL6_CGEN_NET_(?:TCP|BUFFER)_COMPONENT_IMPLEMENTATION|XL6_CgenContext|xl6CgenEmitNetBufferBridge' -or
    !$netTransportComponent.Contains('void xl6CgenEmitNetTcpComponent(') -or
    !$netTransportComponent.Contains('void xl6CgenEmitNetBufferComponent(') -or
    !$netTransportComponent.Contains('void xl6CgenEmitNetTcpStatsComponent(')) {
    throw 'Network runtime provider must contain only canonical XLIR component emitters'
}
$stringNameDataStart = $cEmitter.IndexOf("if ( needs->needsTypeIdToStringRuntime )")
$stringNameDataEnd = $cEmitter.IndexOf(
    "if ( needs->needsStringContainsRuntime )", $stringNameDataStart)
if ( $stringNameDataStart -lt 0 -or
     $stringNameDataEnd -le $stringNameDataStart ) {
    throw "XLIR C emitter must bound TypePool display-name consumption to the closed string conversion ABI"
}
$cEmitterOutsideStringNameData = $cEmitter.Remove(
    $stringNameDataStart, $stringNameDataEnd - $stringNameDataStart)
if ( $cEmitterOutsideStringNameData -match '(?:type|result|signature)->name' ) {
    throw "XLIR C emitter must consume frontend-fixed C ABI categories, not type names"
}
foreach ($required in @('xl6_error_text.h', 'xl6ErrorCreateV1', 'xl6ErrorKindNameV1', 'xl6ErrorKindIsV1')) {
    if (!$cEmitter.Contains($required)) { throw "Error emission must select the explicit resident text ABI: $required" }
}
if ($cEmitter.Contains('static xerrkind xlir_error_xrt_kind') -or $cEmitter.Contains('static inline const char* xlir_error_parent_kind')) {
    throw 'Error classification belongs to the resident provider, not generated C'
}
if ( $cEmitter -match
     'strcmp\s*\(\s*(?:type|result|signature)->' ) {
    throw "XLIR C emitter may compare C ABI tokens only inside its explicit bounded bridges"
}

if ( $header -match 'XL6_AstNode\s*\*(?!\s*root\s*[,\)])' ) {
    throw "finalized XLIR data model must not expose an AST pointer"
}
if ( $header -match '#include\s+"src/ir/xl6_hir.h"' ) {
    throw "XLIR public model must not import HIR definitions"
}

foreach ( $required in @(
    "xl6IrBuildModuleFromAst",
    "xl6IrRefreshExitPlans",
    "xl6IrAppendExitCleanupActions",
    "xl6IrCleanupDeclarationIsLiveAtExit",
    "xl6IrCAbiFromFrontend",
    "xl6IrOwnershipAbiFromFrontend",
    "xl6IrIsCanonicalCStringLiteral",
    "XLIR runtime string duplicate has no canonical literal ABI",
    "XLIR runtime string copy has no exact operand ABI",
    "XLIR runtime string concatenation has no exact operand ABI",
    "XLIR runtime string comparison has no exact borrowed operands and boolean ABI",
    "XLIR runtime UTF-8 string count has no exact borrowed string or i64 result ABI",
    "XLIR runtime dynamic integer has no canonical ABI",
    "XLIR runtime dynamic retain must preserve its exact TypeId",
    "XLIR runtime dynamic array construction has no canonical ABI",
    "XLIR runtime dynamic array integer access has no canonical ABI",
    "XLIR runtime dynamic-key index read has no exact borrowed xvalue ABI",
    "XLIR runtime dynamic-key index write has no exact borrowed-key/consumed-item ABI",
    "XLIR runtime collection member has no exact operation, receiver, operand or result TypeId ABI",
    "XLIR runtime string membership has no exact borrowed haystack, needle or boolean ABI",
    "XLIR runtime range membership has no exact four-i64 and boolean ABI",
    "XLIR runtime object/dynamic-record literal set has no canonical ABI",
    "XLIR runtime object/dynamic-record literal access has no canonical ABI",
    "XLIR runtime object/dynamic-record string-key access has no canonical ABI",
    "XLIR runtime object/dynamic-record string-key set has no canonical ABI",
    "XLIR runtime dynamic array append has no canonical ABI",
    "XLIR runtime dynamic array integer set has no canonical ABI",
    "XLIR runtime trap has no boolean failure ABI",
    "XLIR error raise has no canonical error-state ABI",
    "XLIR error test has no canonical may-raise source ABI",
    "XLIR await has no exact future result ABI",
    "XLIR error take has no canonical owned error payload ABI",
    "XLIR error set-take has no canonical ownership-transfer ABI",
    "XLIR function has no canonical error ABI",
    "XLIR internal call target error ABI does not match its function",
    "kind != XL6_IR_TYPE_TYPE_PARAM || item->sourceTypeId == type->semanticTypeId",
    "xl6IrVerifyModule",
    "xl6IrCallTargetGet",
    "xl6IrExportGet",
    "xl6IrExportReturnAbiForType",
    "xl6IrIsCanonicalCIdentifier",
    "XLIR export parameter has no exact public-to-internal ABI",
    "XLIR call target has no complete canonical ABI",
    "XLIR export has no complete canonical C linkage ABI",
    "XLIR module entry has no exact process ABI",
    "xl6IrVerifyOperatorAbi",
    "XLIR async machine frame has no exact source/lifetime ABI",
    "typed XLIR value has no canonical type or storage type",
    "XLIR type pool is malformed",
    "declarations have no declared AST type",
    "XLIR load or store has no explicit storage ABI",
    "XLIR instruction accesses storage outside its function ABI",
    "XLIR control flow crosses function ABI",
    "XLIR instruction operand has no prior definition in its function ABI",
    "XLIR call argument type does not match its parameter ABI",
    "XLIR borrowed call argument has no explicit ownership ABI",
    "XLIR return value type does not match its function ABI",
    "XLIR non-void function has reachable unterminated block b%u",
    "XLIR block escapes its function instruction ABI",
    "XLIR instruction and block ownership do not match",
    "XLIR tree contains an orphan or cycle",
    "XLIR semantic exit has no unique explicit exit plan",
    "XLIR exit plan does not match its semantic exit ABI",
    "XLIR ownership instruction has no explicit cleanup storage ABI",
    "XLIR ownership instruction type does not match its cleanup storage ABI",
    "XLIR branch lacks explicit condition or targets",
    "XLIR throw has an invalid exception payload",
    "XLIR error raise has no canonical error-state ABI",
    "XLIR call result ABI is inconsistent with its canonical type",
    "XLIR generic-specialization arena is malformed",
    "XLIR generic specialization has no complete monomorphic callable ABI",
    "XLIR generic specialization has a non-concrete type argument",
    "XLIR generic specialization has a non-concrete return ABI",
    "XLIR generic specialization has a non-concrete parameter ABI",
    "XLIR generic specialization storage has a non-concrete ABI",
    "xl6IrTypeContainsTypeParam",
    "generic-instance source=",
    "genericSpecializations",
    "XLIR value copy has no deterministic definition or merge target",
    "XLIR instruction has no canonical operator opcode",
    "has non-canonical operands",
    "XLIR instruction has no canonical immediate payload",
    "xl6IrDumpModuleText",
    "xl6IrDumpModuleJson"
) ) {
    if ( ! $source.Contains($required) ) {
        throw "XLIR implementation is missing invariant: $required"
    }
}

if ( ! $compileUnit.Contains("xl6IrBuildModuleFromAst(&unit->ir") -or
     $compileUnit.Contains("xl6IrBuildModuleFromHir(&unit->ir") -or
     $compileUnit.Contains("xl6HirBuildModule(&unit->hir") -or
     ! $compileUnit.Contains('unit->irReady = ok') ) {
    throw "compile-unit path must construct XLIR directly from the semantic AST without eagerly building HIR"
}

if ( ! $api.Contains("xl6IrDumpModuleText(&out, &ctx->unit.ir)") -or
     ! $api.Contains("xl6IrDumpModuleJson(&out, &ctx->unit.ir)") ) {
    throw "xl ir command is not dumping canonical XLIR"
}
if ( ! $publicApi.Contains("xlcEmitLoweredIr") ) {
    throw "public compiler API does not expose verified lowered XLIR emission"
}
if ( ! $publicApi.Contains("xlcEmitXlirHeader") -or ! $api.Contains("xlcEmitXlirHeader") -or
     ! $shell.Contains("xlcShellEmitXlirHeader") -or ! $shell.Contains("xlcShellCgenDumpDir") -or
     ! $shell.Contains("xlcEmitLoweredIr(ctx, path, 0)") ) {
    throw "XLIR public C/header and dump products must stay on the verified IR emitter path"
}
if ( ! $api.Contains("xlcEmitLoweredIr") -or ! $api.Contains("xlEnsureUnitXlirLowered(ctx->runtime, &ctx->unit)") -or
     ! $shell.Contains('xl ir <file.xl> [--lowered]') -or ! $shell.Contains('xlcShellHasFlag(argc, argv, "--lowered")') ) {
    throw "xl ir must expose the verified lowered XLIR stage for deterministic backend diagnostics"
}
if ( ! $api.Contains("xlEnsureUnitXlirLowered") -or
     ! $api.Contains("xl6IrLowerModule(&unit->ir") ) {
    throw "XLIR C API is not lowering typed XLIR before deterministic C emission"
}
$xlirEmitStart = $api.IndexOf("static int xlEmitUnitCToFile")
if ( $xlirEmitStart -lt 0 ) {
    throw "XLIR C file-emission boundary is missing"
}
$xlirEmit = $api.Substring($xlirEmitStart)
$preflight = $xlirEmit.IndexOf("xl6IrCgenVerifyEmittable(&unit->ir")
$openOutput = $xlirEmit.IndexOf("xl6OutputOpenFile(&out, outputPath)")
if ( $preflight -lt 0 -or $openOutput -lt 0 -or $preflight -gt $openOutput ) {
    throw "XLIR C emission must verify the deterministic ABI before opening the output file"
}
foreach ( $required in @("XL6_IR_RUNTIME_ERROR_MESSAGE_COPY", "error-message-copy", "runtime error message copy has no exact borrowed error ABI") ) {
    if ( ! $header.Contains($required) -and ! $source.Contains($required) ) {
        throw "XLIR error-message IR contract is incomplete: $required"
    }
}
foreach ( $required in @("XL6_IR_RUNTIME_ERROR_CODE", "error-code", "runtime error code has no exact borrowed error scalar ABI") ) {
    if ( ! $header.Contains($required) -and ! $source.Contains($required) ) {
        throw "XLIR error-code IR contract is incomplete: $required"
    }
}
foreach ( $required in @("xl6IrLowerErrorMember", "XL6_IR_RUNTIME_ERROR_MESSAGE_COPY") ) {
    if ( ! $lower.Contains($required) ) { throw "XLIR error-message lowering contract is incomplete: $required" }
}
foreach ( $required in @("xl6IrLowerErrorMember", "XL6_IR_RUNTIME_ERROR_CODE") ) {
    if ( ! $lower.Contains($required) ) { throw "XLIR error-code lowering contract is incomplete: $required" }
}
foreach ( $required in @("XL6_IR_RUNTIME_ERROR_MESSAGE_COPY", "xl6ErrorMessageCopyV1") ) {
    if ( ! $cEmitter.Contains($required) ) { throw "XLIR error-message C ABI contract is incomplete: $required" }
}
foreach ( $required in @("XL6_IR_RUNTIME_ERROR_CODE", "xrtErrorCode") ) {
    if ( ! $cEmitter.Contains($required) ) { throw "XLIR error-code C ABI contract is incomplete: $required" }
}
foreach ( $required in @("XL6_IR_RUNTIME_ERROR_DOMAIN_COPY", "XL6_IR_RUNTIME_ERROR_OPERATION_COPY", "XL6_IR_RUNTIME_ERROR_DATA_COPY", "XL6_IR_RUNTIME_ERROR_SYSTEM_CODE") ) {
    if ( ! $header.Contains($required) -and ! $source.Contains($required) ) {
        throw "XLIR error-runtime-field IR contract is incomplete: $required"
    }
}
foreach ( $required in @("xl6IrLowerErrorMember", "XL6_IR_RUNTIME_ERROR_DOMAIN_COPY", "XL6_IR_RUNTIME_ERROR_SYSTEM_CODE") ) {
    if ( ! $lower.Contains($required) ) { throw "XLIR error-runtime-field lowering contract is incomplete: $required" }
}
foreach ( $required in @("XL6_IR_RUNTIME_ERROR_DOMAIN_COPY", "xl6ErrorDomainCopyV1", "xl6ErrorOperationCopyV1", "xl6ErrorDataCopyV1", "xrtErrorSystemCode") ) {
    if ( ! $cEmitter.Contains($required) ) { throw "XLIR error-runtime-field C ABI contract is incomplete: $required" }
}
foreach ( $required in @("XL6_IR_RUNTIME_ERROR_CAUSE_REF", "error-cause-ref", "runtime error cause has no exact borrowed-to-owned error ABI") ) {
    if ( ! $header.Contains($required) -and ! $source.Contains($required) ) {
        throw "XLIR error-cause IR contract is incomplete: $required"
    }
}
foreach ( $required in @("xl6IrLowerErrorMember", "XL6_IR_RUNTIME_ERROR_CAUSE_REF") ) {
    if ( ! $lower.Contains($required) ) { throw "XLIR error-cause lowering contract is incomplete: $required" }
}
if ( ! $lower.Contains("xl6IrLowerErrorMemberReceiver") ) {
    throw "XLIR error member lowering must accept a verified error value, not only a catch symbol"
}
foreach ( $required in @("XL6_IR_RUNTIME_ERROR_CAUSE_REF", "xrtErrorCause", "xrtErrorRef") ) {
    if ( ! $cEmitter.Contains($required) ) { throw "XLIR error-cause C ABI contract is incomplete: $required" }
}
foreach ( $required in @("XL6_IR_RUNTIME_ERROR_KIND_COPY", "error-kind-copy", "runtime error kind copy has no exact borrowed error ABI") ) {
    if ( ! $header.Contains($required) -and ! $source.Contains($required) ) {
        throw "XLIR error-kind IR contract is incomplete: $required"
    }
}
foreach ( $required in @("xl6IrLowerErrorMember", "XL6_IR_RUNTIME_ERROR_KIND_COPY") ) {
    if ( ! $lower.Contains($required) ) { throw "XLIR error-kind lowering contract is incomplete: $required" }
}
foreach ( $required in @("XL6_IR_RUNTIME_ERROR_KIND_COPY", "xl6ErrorKindCopyV1", "xlir_error_create", "xl6ErrorCreateV1") ) {
    if ( ! $cEmitter.Contains($required) ) { throw "XLIR error-kind C ABI contract is incomplete: $required" }
}
foreach ( $required in @("XL6_IR_RUNTIME_ERROR_KIND_IS", "error-kind-is", "runtime error kind test has no exact borrowed error predicate ABI") ) {
    if ( ! $header.Contains($required) -and ! $source.Contains($required) ) {
        throw "XLIR error-kind predicate IR contract is incomplete: $required"
    }
}
foreach ( $required in @("xl6IrLowerErrorKindIs", "XL6_IR_INSTR_BRANCH", "XL6_IR_RUNTIME_ERROR_KIND_IS") ) {
    if ( ! $lower.Contains($required) ) { throw "XLIR typed-catch lowering contract is incomplete: $required" }
}
foreach ( $required in @("xl6IrLowerTryCatchMulti", "dispatchStorage", "XLIR multi-catch must end with catch-all or canonical typed handlers") ) {
    if ( ! $lower.Contains($required) ) { throw "XLIR multi-catch lowering contract is incomplete: $required" }
}
foreach ( $required in @("typedCatch", "matchedHandlerBlock", "unmatchedHandlerBlock", "XLIR try/catch/finally handler has no canonical kind, binding or body ABI") ) {
    if ( ! $lower.Contains($required) ) { throw "XLIR typed catch/finally lowering contract is incomplete: $required" }
}
foreach ( $required in @("xl6IrLowerTryCatchMultiFinally", "XLIR multi-catch/finally requires canonical handlers and a non-empty finally block", "XLIR multi-catch/finally must end with catch-all or canonical typed handlers", "dispatchStorage", "normalFinallyBlock", "exceptionalFinallyBlock") ) {
    if ( ! $lower.Contains($required) ) { throw "XLIR multi-catch/finally lowering contract is incomplete: $required" }
}
foreach ( $required in @("returnFinallyBlock", "deferredReturnStorage", "XLIR return through multi-catch/finally has no pending value storage ABI") ) {
    if ( ! $lower.Contains($required) ) { throw "XLIR multi-catch/finally deferred-return contract is incomplete: $required" }
}
foreach ( $required in @("xl6IrLowerPushFinallyBarrier", "xl6IrLowerEmitDeferredLoopExitFinalies", "finallyBarrierId") ) {
    if ( ! $lower.Contains($required) ) { throw "XLIR multi-catch/finally loop-exit contract is incomplete: $required" }
}
foreach ( $required in @("xl6IrLowerEmitPendingExceptionRoute(ctx, id, node)", "xl6IrTryExceptionPlanGet(ctx->module, origin)", "xl6IrLowerEmitExceptionTransfer", "XL6_IR_INSTR_ERROR_SET_TAKE") ) {
    if ( ! $lower.Contains($required) ) { throw "XLIR typed-catch miss must consume the explicit post-try exception route" }
}
if (!$header.Contains("XL6_IR_EXIT_POINT_AFTER_TRY_EXCEPTION") -or
    !$source.Contains("xl6IrVerifyPendingExceptionRoutes") -or
    !$source.Contains("xl6IrVerifyOrderedUnwindRoute") -or
    !$source.Contains("xl6IrVerifyLoopOwnerFlow") -or
    !$source.Contains("module->instructions[failure->firstInstruction].parameterScope != i + 1") -or
    !(Read-Utf8 "test\suites\03_core_function_abi.bat").Contains("tools\check_xlir_pending_exception_route.ps1") -or
    !(Read-Utf8 "tools\check_xlir_pending_exception_route.ps1").Contains("xlir_pending_exception_route_contract.c")) {
    throw "Post-try and parameter-failure continuations need distinct verified identities and owner-route mutation coverage"
}
foreach ($obsolete in @("xl6IrLowerExceptionTargetWithDepth", "xl6IrLowerEmitLoopResourcesForReturn", "xl6IrLowerEmitLoopResourcesForException")) {
    if ($lower.Contains($obsolete)) { throw "Exception routing must not retain the old depth-only loop sweep: $obsolete" }
}
if (!$header.Contains("XL6_IrPendingException") -or !$header.Contains("pendingFailure") -or
    !$source.Contains("xl6IrVerifyPendingExceptionOwners") -or
    !$source.Contains("xl6IrVerifyStorageActivationFlow") -or
    !$lower.Contains("xl6IrLowerExceptionalFinallyBody") -or
    !$lower.Contains("xl6IrLowerPreserveCleanupException") -or
    !(Read-Utf8 "tools\check_xlir_pending_exception_route.ps1").Contains("xlir_pending_exception_owner_contract.c") -or
    !(Read-Utf8 "test\suites\03_core_function_abi.bat").Contains("xlir_pending_exception_control.xl")) {
    throw "Finally requires explicit verified pending-error capture, restoration cause, cancellation and continuation lifetime coverage"
}
foreach ($renderer in @('async_contract', 'generator', 'release')) {
    if (!(Read-Utf8 "src\cgen\xl6_ir_cgen_$renderer.c").Contains("xl6IrCgenFrameStorageSupported")) {
        throw "Continuation frame storage must share the physical destructor renderer: $renderer"
    }
}
foreach ( $required in @("XL6_IR_RUNTIME_ERROR_KIND_IS", "xlir_error_kind_is", "xl6ErrorKindIsV1", "xrtSetErrorTake") ) {
    if ( ! $cEmitter.Contains($required) ) { throw "XLIR typed-catch C ABI contract is incomplete: $required" }
}
foreach ( $required in @(
    "XL6_IR_RUNTIME_INTEGER_CONVERT_CHECKED",
    "XL6_IR_RUNTIME_INTEGER_CONVERT_WRAPPING",
    "XL6_IR_INTRINSIC_WRAPPING_INTEGER_CONVERT",
    "XL6_IrIntegerTarget",
    "XL6_IrIntegerSourceAbi",
    "XL6_IR_INTEGER_SOURCE_MANAGED_STRING",
    "SIGNED_64", "UNSIGNED_64",
    "XLIR checked integer conversion has no exact Typed source, target and failure ABI",
    "integerTarget"
) ) {
    if ( ! $header.Contains($required) -and ! $source.Contains($required) ) {
        throw "XLIR integer conversion IR contract is incomplete: $required"
    }
}
foreach ( $required in @(
    "xl6IrLowerIntrinsicCheckedIntegerConvert",
    "xl6IrLowerIntrinsicWrappingIntegerConvert",
    "xl6IrLowerEmitIntegerStorageNormalize",
    "xl6IrIntrinsicCallGet",
    "XL6_IR_RUNTIME_INTEGER_CONVERT_CHECKED",
    "XL6_IR_RUNTIME_INTEGER_CONVERT_WRAPPING",
    "builtin integer conversion has no resolved checked source and target ABI"
) ) {
    if ( ! $lower.Contains($required) ) { throw "XLIR integer conversion lowering contract is incomplete: $required" }
}
foreach ( $required in @(
    "XL6_IR_RUNTIME_INTEGER_CONVERT_CHECKED",
    "XL6_IR_RUNTIME_INTEGER_CONVERT_WRAPPING",
    "xl6IrCgenRuntimeIntegerConvertAbi",
    "xl6IntegerTextConvertV1", "xl6IntegerValueConvertV1",
    "xlir_f64_to_i64_checked", "xlir_f64_to_u64_checked",
    "xlir_integer_wrap_to_i64", "xrtGetError() == NULL"
) ) {
    if ( ! $cEmitter.Contains($required) ) { throw "XLIR integer conversion C ABI contract is incomplete: $required" }
}
foreach ( $required in @(
    "xlir_integer_conversion_source_abi.xl",
    "type_int_narrowing_wrap.xl",
    "integerSourceAbiName", "integerTargetName",
    "integer-convert-checked", "integer-convert-wrapping",
    "ERROR_TEST", "ReadAllBytes", "-Werror",
    "Static U64 conversion incorrectly re-entered the checked ABI"
) ) {
    if ( ! $integerConversionAbiGate.Contains($required) ) {
        throw "XLIR integer conversion runtime quality gate is incomplete: $required"
    }
}
if ( ! $languageFrontendSuite.Contains("tools\check_xlir_integer_conversion_abi.ps1") ) {
    throw "XLIR integer conversion quality gate is not wired into the language frontend suite"
}
foreach ( $required in @(
    "operatorExecutionType", "XL6_IrOperatorOperandAbi",
    "XL6_IR_OPERATOR_OPERAND_XVALUE_PROJECT_EXACT",
    "XL6_IR_SCALAR_CONVERSION_OPERATOR_I64_TO_U64",
    "xl6IrClassifyOperatorOperandConversionAbi"
) ) {
    if ( ! $header.Contains($required) -and ! $source.Contains($required) ) {
        throw "XLIR operator execution-type IR contract is incomplete: $required"
    }
}
foreach ( $required in @(
    "xl6IrLowerOperatorOperandAsExecutionType",
    "XL6_IR_SCALAR_CONVERSION_OPERATOR_I64_TO_U64",
    "operator operand has no explicit execution-domain conversion ABI"
) ) {
    if ( ! $lower.Contains($required) ) {
        throw "XLIR operator execution-type lowering contract is incomplete: $required"
    }
}
foreach ( $required in @(
    "type_int_minmax_bounds.xl", "extlib_imagehash.xl", "stdlib_json.xl",
    "runtime_var_type_storage_matrix.xl", "xvalue-project-exact",
    "xvalue-project-scalar-copy",
    "operator-i64-to-u64", "c_int", "c_bool", "integer-to-bool",
    "operatorExecutionType",
    "ReadAllBytes", "-Werror", "strict-C/runtime"
) ) {
    if ( ! $operatorExecutionTypeGate.Contains($required) ) {
        throw "XLIR operator execution-type quality gate is incomplete: $required"
    }
}
if ( ! $languageFrontendSuite.Contains(
         "tools\check_xlir_operator_execution_type.ps1") ) {
    throw "XLIR operator execution-type quality gate is not wired into the language frontend suite"
}
foreach ( $required in @(
    "XL6_IR_TYPE_WEAK", "implicitConversionRuntimeOp",
    "XL6_IR_RUNTIME_XVALUE_WEAK_CREATE",
    "XL6_IR_RUNTIME_XVALUE_WEAK_LOCK", "XL6_IR_INTRINSIC_WEAK_LOCK"
) ) {
    if ( ! $header.Contains($required) ) {
        throw "XLIR weak type/runtime IR contract is incomplete: $required"
    }
}
foreach ( $required in @(
    "xl6IrWeakCreateSignature", "xl6IrWeakLockSignature",
    "xl6IrClassifyImplicitConversionRuntimeOp",
    "xvalue-weak-create", "xvalue-weak-lock", "weak-lock"
) ) {
    if ( ! $source.Contains($required) ) {
        throw "XLIR weak Typed IR contract is incomplete: $required"
    }
}
foreach ( $required in @(
    "xl6IrLowerWeakCreate", "xl6IrLowerIntrinsicWeakLock",
    "implicitConversionRuntimeOp", "XL6_IR_RUNTIME_XVALUE_WEAK_CREATE",
    "XL6_IR_RUNTIME_XVALUE_WEAK_LOCK"
) ) {
    if ( ! $lower.Contains($required) ) {
        throw "XLIR weak lowering contract is incomplete: $required"
    }
}
foreach ( $required in @(
    "xrtValueWeakRef", "xrtValueWeakRefLock",
    "xlir_xvalue_weak_create", "xlir_xvalue_weak_lock"
) ) {
    if ( ! $cEmitter.Contains($required) ) {
        throw "XLIR weak C emitter contract is incomplete: $required"
    }
}
foreach ( $required in @(
    "protocol_weak_lifecycle.xl", "hir_nullable_types_check.xl",
    "weak_class_lifecycle.xl", "generatorCaptureAbis",
    "implicitConversionRuntimeOpName", "one exact THROW ExitPlan",
    "xvalue-weak-create", "weak-lock",
    "xrtValueWeakRef", "xrtValueWeakRefLock", "xrtValueGetRuntimeObject",
    "ReadAllBytes", "-Werror", "protocol weak lifecycle ok"
) ) {
    if ( ! $weakAbiGate.Contains($required) ) {
        throw "XLIR weak ABI quality gate is incomplete: $required"
    }
}
if ( ! $languageFrontendSuite.Contains("tools\check_xlir_weak_abi.ps1") ) {
    throw "XLIR weak ABI quality gate is not wired into the language frontend suite"
}
foreach ( $required in @(
    "xl6IrCgenClosureCallAbi", "xl6IrCgenGeneratorMachineProgramSupported",
    "XL6_IR_INSTR_CLOSURE_CALL", "closure-call carrier ABI"
) ) {
    if ( ! $cEmitter.Contains($required) ) {
        throw "XLIR generator closure-call emitter contract is incomplete: $required"
    }
}
foreach ( $required in @(
    "sem_generator_function_param.xl", "retain-closure",
    "release-closure", "closureCalls", "boolean call carrier",
    "-Werror", "runtime semantics"
) ) {
    if ( ! $generatorMachineGate.Contains($required) ) {
        throw "XLIR generator closure-call quality gate is incomplete: $required"
    }
}
if ( ! $languageFrontendSuite.Contains(
         "tools\check_xlir_generator_machine.ps1") ) {
    throw "XLIR generator machine quality gate is not wired into the language frontend suite"
}
foreach ( $required in @(
    "XL6_IrScalarConversionAbi",
    "XL6_IR_SCALAR_CONVERSION_BOOL_TO_INTEGER",
    "XL6_IR_SCALAR_CONVERSION_INTEGER_TO_BOOL",
    "XL6_IR_SCALAR_CONVERSION_INTEGER_TO_FLOAT",
    "XL6_IR_SCALAR_CONVERSION_FLOAT_TO_INTEGER",
    "XL6_IR_SCALAR_CONVERSION_TIME_TO_BOOL",
    "XL6_IR_SCALAR_CONVERSION_TIME_TO_INTEGER",
    "XL6_IR_SCALAR_CONVERSION_TIME_TO_FLOAT",
    "XL6_IR_SCALAR_CONVERSION_INTEGER_TO_TIME",
    "XL6_IR_C_ABI_TIME_I64",
    "scalarConversionAbi",
    "xl6IrClassifyScalarConversionAbi"
) ) {
    if ( ! $header.Contains($required) -and ! $source.Contains($required) ) {
        throw "XLIR scalar conversion IR contract is incomplete: $required"
    }
}
foreach ( $required in @(
    "xl6IrClassifyScalarConversionAbi",
    "scalarConversionAbi = abi",
    "XLIR scalar conversion has no exact source and result ABI"
) ) {
    if ( ! $lower.Contains($required) ) {
        throw "XLIR scalar conversion lowering contract is incomplete: $required"
    }
}
foreach ( $required in @(
    "instruction->scalarConversionAbi",
    "XL6_IR_SCALAR_CONVERSION_INVALID",
    "XL6_IR_SCALAR_CONVERSION_OPERATOR_I64_TO_U64",
    "XLIR C emitter scalar conversion ABI is unresolved"
) ) {
    if ( ! $cEmitter.Contains($required) ) {
        throw "XLIR scalar conversion Cgen contract is incomplete: $required"
    }
}
foreach ( $required in @(
    "xlir_scalar_conversion_abi.xl", "bool-to-bool", "bool-to-integer", "bool-to-float",
    "integer-to-bool", "integer-to-integer", "integer-to-float",
    "float-to-integer", "float-to-float", "scalarConversionAbiName",
    "ReadAllBytes", "-Werror"
) ) {
    if ( ! $scalarConversionAbiGate.Contains($required) ) {
        throw "XLIR scalar conversion runtime quality gate is incomplete: $required"
    }
}
if ( ! $languageFrontendSuite.Contains("tools\check_xlir_scalar_conversion_abi.ps1") ) {
    throw "XLIR scalar conversion quality gate is not wired into the language frontend suite"
}
foreach ( $required in @(
    "xlir_time_conversion_abi.xl", "TIME_I64", "time-to-bool",
    "time-to-integer", "time-to-float", "integer-to-time",
    "extern parameter/return ABI", "ReadAllBytes", "-Werror"
) ) {
    if ( ! $timeConversionAbiGate.Contains($required) ) {
        throw "XLIR time conversion runtime quality gate is incomplete: $required"
    }
}
if ( ! $languageFrontendSuite.Contains("tools\check_xlir_time_conversion_abi.ps1") ) {
    throw "XLIR time conversion quality gate is not wired into the language frontend suite"
}
foreach ( $required in @("XL6_IR_RUNTIME_BRIDGE_PRINT", "needsPrintRuntime", "extern bool xl6_xrt_print") ) {
    if ( ! $cEmitter.Contains($required) ) { throw "XLIR print runtime bridge contract is incomplete: $required" }
}
if ($cEmitter.Contains('static bool xlir_print_one(') -or
    $cEmitter.Contains('serialized = xrtXlonStringify(value, false, &size)')) {
    throw 'Cgen must not implement resident value display/printing algorithms'
}
foreach ($required in @('XL6_IR_RUNTIME_BRIDGE_TEXT_VALUE', 'needsTextAdapterDeclarations')) {
    if (!$cEmitter.Contains($required)) {throw "Cgen must consume resident text declarations: $required"}
}
if ($cEmitter.Contains("typedef struct xlir_string_list")) {
    throw "XLIR string split must consume the public XRT list ABI"
}
if (!$languageFrontendSuite.Contains("tools\check_xlir_string_bridges.ps1")) {
    throw "XLIR string bridge boundary gate is not wired into the language frontend suite"
}
foreach ($required in @('XL6_IrStringBridgeContract', 'target->errorAbi != contract->errorAbi',
    'xl6IrRuntimeStringBridgeErrorAbi', 'XL6_IR_RUNTIME_BRIDGE_STRING_JOIN',
    'XL6_IR_RUNTIME_BRIDGE_STRING_SPLIT_LINES')) {
    if (!$source.Contains($required)) { throw "XLIR string ABI registry is incomplete: $required" }
}
$nativeCollectionContracts = Read-Utf8 'src/abi/xl6_native_collection_contracts.inc'
foreach ($required in @('xl6TextJoinV1', 'xl6TextSplitV2', 'xl6TextLinesV2')) {
    if (!$nativeCollectionContracts.Contains($required)) {throw "Resident text exact ABI missing: $required"}
}
foreach ($entry in @(@('src/runtime/xl6_text_pieces.h', 'xrtStrSplitNext'),
        @('src/runtime/xl6_text_join.c', 'xrtStrJoinSized'))) {
    if (!(Read-Utf8 $entry[0]).Contains($entry[1])) {throw 'Resident text must reuse sized XRT algorithms'}
}
$stringJoinGate = Read-Utf8 'tools\check_xlir_string_join.ps1'
if (!$stringJoinGate.Contains('Cgen contains a text implementation') -or
    !$stringJoinGate.Contains('xl6_xrt_(?:string_|take_cstr)')) {
    throw 'Default string programs must reject generated/legacy library implementations'
}
foreach ($required in @('xlir_string_join_failure.xl', 'errorPhysicalAbiName', 'xrt-thread-error',
    'string join threads ok: 2000 rounds', 'xlir_string_join_bad_parameter',
    'xlir_string_join_bad_result', 'changedInputs', 'passed-scoped')) {
    if (!$stringJoinGate.Contains($required)) { throw "XLIR string effect and failure gate is incomplete: $required" }
}
if (!(Read-Utf8 'tools\run_xlir_parameterized_guards.ps1').Contains("'check_xlir_string_join.ps1'")) {
    throw 'XLIR source string-join gate is not registered with scenario workers'
}
foreach ( $required in @(
    "cgen_dynamic_to_typed_containers.xl", "--backend xlir",
    "xl6TextLinesV2", "-Werror",
    "xlir_dynamic_to_typed_containers_host.c"
) ) {
    if ( ! $languageFrontendSuite.Contains($required) ) { throw "XLIR dynamic collection bridge runtime gate is incomplete: $required" }
}
if ( $cEmitter.Contains("strcmp(target->cName") -or
     $cEmitter.Contains("xl6IrCgenIsPrintRuntimeTarget") -or
     $cEmitter.Contains("xl6IrRuntimeBridgeOperationForName") ) {
    throw "XLIR C emitter must consume verified runtimeBridge metadata instead of inferring helper semantics from C names"
}
foreach ( $required in @(
    "target->runtimeBridgeOperation",
    "XL6_IR_RUNTIME_BRIDGE_OPERATION_NET_TCP_FILE_TRANSFER",
    "XL6_IR_RUNTIME_BRIDGE_OPERATION_NET_TCP_RECV_OWNED_BYTES_ASYNC",
    "XL6_IR_RUNTIME_BRIDGE_OPERATION_NET_TLS_RECV_OWNED_BYTES_ASYNC"
) ) {
    if ( ! $cEmitter.Contains($required) ) {
        throw "XLIR network C emitter is missing verified runtime bridge operation metadata: $required"
    }
}
foreach ( $forbidden in @(
    "xl6_xrt_tcp_stream_send_file",
    "xl6_xrt_tcp_stream_recv_owned_async",
    "xl6_xrt_tls_stream_recv_owned_async"
) ) {
    if ( $cEmitter.Contains($forbidden) ) {
        throw "XLIR network C emitter must not infer bridge operation from symbol text: $forbidden"
    }
}
foreach ( $required in @("xl6IrLowerCanonicalIntegerLiteral", "xl6IrLowerCanonicalUnsignedIntegerLiteral", "xl6IrLowerCanonicalFloatLiteral", "XLIR literal has no canonical C runtime payload") ) {
    if ( ! $lower.Contains($required) ) { throw "XLIR scalar-literal lowering contract is incomplete: $required" }
}
foreach ( $required in @("candidate->scopeDepth != plan->targetScopeDepth", "candidate->region != plan->targetRegion", "xl6IrLowerEmitUnwindRoute(ctx, plan, transferredStorage)", "xl6IrLowerEmitExceptionTransfer") ) {
    if ( ! $lower.Contains($required) ) { throw "XLIR catch-edge cleanup contract is incomplete: $required" }
}
foreach ( $required in @("xl6IrLowerEmitIntConst", "constantType->cAbi == XL6_IR_C_ABI_U64", "xl6IrLowerCanonicalUnsignedIntegerLiteral(text)") ) {
    if ( ! $lower.Contains($required) ) { throw "XLIR synthetic integer-constant contract is incomplete: $required" }
}
foreach ( $required in @("XLIR throw payload requires a managed-string error-state ABI", "xl6IrLowerTypeIsManagedString") ) {
    if ( ! $lower.Contains($required) ) { throw "XLIR throw payload contract is incomplete: $required" }
}
foreach ( $required in @("xl6IrIsCanonicalIntegerLiteral", "xl6IrIsCanonicalUnsignedIntegerLiteral", "xl6IrIsCanonicalFloatLiteral", "XLIR scalar constant has no canonical C payload") ) {
    if ( ! $source.Contains($required) ) { throw "XLIR scalar-literal verification contract is incomplete: $required" }
}
foreach ( $required in @("XL6_IR_RUNTIME_U64_TO_STRING", "u64-to-string", "XL6_IR_INTEGER_TARGET_UNSIGNED_64") ) {
    if ( ! $header.Contains($required) -and ! $source.Contains($required) ) {
        throw "XLIR U64 IR contract is incomplete: $required"
    }
}
foreach ( $required in @("XL6_IR_RUNTIME_U64_TO_STRING", "xlir_u64_to_string", "xrtUIntFormatTo", "uint64_t", "xlir_result_u64") ) {
    if ( ! $cEmitter.Contains($required) ) { throw "XLIR U64 C ABI contract is incomplete: $required" }
}

# Language `type` values have the historical structural hash identity but a
# distinct physical ABI.  They must not collapse back into ordinary U64 merely
# because both are represented by uint64_t at the C boundary.
foreach ( $required in @(
    "XL6_IR_C_ABI_TYPE_ID", "uint64 typeValueId", "typeValueTarget",
    "XL6_IR_INTRINSIC_TYPE_OF", "XL6_IrTypeQueryAbi",
    "XL6_IR_TYPE_QUERY_STATIC_ID", "XL6_IR_TYPE_QUERY_DYNAMIC_XVALUE",
    "typeQueryAbi", "typeQueryTarget", "XL6_IR_ASYNC_RESULT_COPY_TYPE_ID",
    "XL6_IR_RUNTIME_XVALUE_TYPE_REBIND_OWNED", "runtimeIdentityType",
    "xl6IrTypeFinalizeTypeValueId", "char* displayName"
) ) {
    if ( ! $header.Contains($required) ) { throw "XLIR TypeId header contract is incomplete: $required" }
}
foreach ( $required in @(
    "xl6IrTypeValueHashIrType", "xl6IrIsCanonicalTypeIdLiteral",
    "xl6IrTypeFinalizeDisplayName", "xl6IrTypeDisplayNameMatches",
    "xl6IrGlobalExportInitializerNodeMatches", "type-of", "static-id",
    "xvalue-type-rebind-owned",
    'typeValueId\":\"0x%016llX', 'displayName\":\"%s',
    'typeQueryAbiName\":\"%s',
    "XLIR type-of intrinsic has no canonical query identity ABI",
    "XLIR type-of intrinsic has no exact static or dynamic type identity ABI",
    "XLIR runtime type rebind has no unique checked owner and exact target identity ABI"
) ) {
    if ( ! $source.Contains($required) ) { throw "XLIR TypeId identity/verifier contract is incomplete: $required" }
}
foreach ( $required in @(
    "xl6IrLowerGlobalExportInitializer", "XL6_IR_C_ABI_TYPE_ID",
    "XL6_IR_ASYNC_RESULT_COPY_TYPE_ID", "0x%016llXULL",
    "xl6IrLowerBindRuntimeTypeIdentity",
    "xl6IrLowerRebindRuntimeTypeIdentityOwned",
    "XL6_IR_RUNTIME_XVALUE_TYPE_BIND",
    "XL6_IR_RUNTIME_XVALUE_TYPE_REBIND_OWNED",
    "XL6_IR_RUNTIME_XVALUE_TYPE_ID"
) ) {
    if ( ! $lower.Contains($required) ) { throw "XLIR TypeId lowering contract is incomplete: $required" }
}
foreach ( $required in @(
    "xlir_result_type_id", "XL6_IR_ASYNC_RESULT_COPY_TYPE_ID",
    "XL6_CELL_TYPE_ID", "xl6CellRuntimeV1Get()->Load", "xl6CellRuntimeV1Get()->Store",
    "XL6_IR_ASYNC_RESULT_COPY_TYPE_ID",
    "xlir_xvalue_type_rebind_owned", "xrtValueTypeIdRebind",
    "type->displayName"
) ) {
    if ( ! $cEmitter.Contains($required) ) { throw "XLIR TypeId C ABI contract is incomplete: $required" }
}
if ( ! $cellRuntime.Contains('case XL6_CELL_U64: case XL6_CELL_TYPE_ID: return size==sizeof(uint64_t);') ) {
    throw 'Resident TypeId cells must validate the exact uint64_t payload size'
}
if ( ! $cellHeader.Contains('XL6_CELL_CHAR') -or
     ! $cellRuntime.Contains('case XL6_CELL_CHAR: return size==sizeof(uint32_t);') -or
     ! $cellTypeCallEmitter.Contains('case XL6_IR_C_ABI_CHAR_U32: return "XL6_CELL_CHAR";') ) {
    throw 'Resident char cells must preserve the exact uint32_t Unicode-scalar payload ABI'
}
if ( ! $cellTypeCallEmitter.Contains('case XL6_IR_C_ABI_MANAGED_BYTES:') -or
     ! $cellTypeCallEmitter.Contains('case XL6_IR_C_ABI_RUNTIME_XVALUE_PTR: return "XL6_CELL_XVALUE";') ) {
    throw 'Resident bytes cells must reuse the managed XValue payload ABI'
}
foreach ( $required in @(
    "--format json", "--lowered", "typeQueryAbiName", "copy-type-id",
    "xlir_result_type_id", "-Werror", "type_values.xl", "type_aliases.xl",
    "cgen_generic_class_type_values.xl",
    "xlir_type_value_dynamic.xl", "runtimeTypeMap",
    "xvalue-type-bind", "xvalue-type-id"
) ) {
    if ( ! $typeValueAbiGate.Contains($required) ) { throw "XLIR TypeId quality gate is incomplete: $required" }
}
if ( ! $languageFrontendSuite.Contains("tools\check_xlir_type_value_abi.ps1") ) {
    throw "XLIR TypeId quality gate is not wired into the language frontend suite"
}
foreach ( $required in @(
    "XL6_IrStringConversionAbi", "XL6_IR_STRING_CONVERSION_TYPE_ID_NAME",
    "XL6_IR_STRING_CONVERSION_CLASS_DERIVED",
    "XL6_IR_STRING_CONVERSION_CLASS_PROTOCOL_CALL",
    "XL6_IrClassStringAbi", "stringDeriveIncluded",
    "XL6_IR_RUNTIME_TYPE_ID_TO_STRING",
    "XL6_IR_RUNTIME_CLASS_DERIVED_TO_STRING"
) ) {
    if ( ! $header.Contains($required) ) {
        throw "XLIR closed string conversion header contract is incomplete: $required"
    }
}
foreach ( $required in @(
    "xl6IrBuildClassStringContract", "xl6IrBuildFieldDeriveIncluded",
    "xl6IrClassifyStringConversionAbi", "xl6IrVerifyClassStringAbi",
    "stringConversionAbiName", "stringAbiName",
    "type-id-to-string", "class-derived-to-string",
    "derived class string conversion has no exact layout"
) ) {
    if ( ! $source.Contains($required) ) {
        throw "XLIR closed string conversion typed/verifier contract is incomplete: $required"
    }
}
foreach ( $required in @(
    "xl6IrLowerIntrinsicStringConvert",
    "xl6IrLowerConvertValueToString",
    "XL6_IR_STRING_CONVERSION_F32_VALUE",
    "XL6_IR_STRING_CONVERSION_TYPE_ID_NAME",
    "XL6_IR_STRING_CONVERSION_CLASS_DERIVED",
    "XL6_IR_STRING_CONVERSION_CLASS_PROTOCOL_CALL",
    "XL6_IR_RUNTIME_TYPE_ID_TO_STRING",
    "XL6_IR_RUNTIME_CLASS_DERIVED_TO_STRING",
    "layout->stringTarget"
) ) {
    if ( ! $lower.Contains($required) ) {
        throw "XLIR closed string conversion lowering contract is incomplete: $required"
    }
}
foreach ( $required in @(
    "xl6IrCgenRuntimeTypeIdToStringAbi",
    "xl6IrCgenRuntimeClassDerivedStringAbi",
    "xlir_type_id_to_string", "xl6ClassValueTextV1", "xlir_class_string_schema_t",
    "stringDeriveIncluded", "xrtValueObjectGet"
) ) {
    if ( ! $cEmitter.Contains($required) ) {
        throw "XLIR closed string conversion C emitter contract is incomplete: $required"
    }
}
foreach ( $required in @(
    "xlir_string_conversion_abi.xl", "stringConversionAbiName",
    "runtime-derived", "internal-call", "type-id-name",
    "class-derived-to-string", "type-id-to-string",
    "stringDeriveIncluded", "SequenceEqual",
    "strict-C/runtime gate failed"
) ) {
    if ( ! $stringConversionAbiGate.Contains($required) ) {
        throw "XLIR closed string conversion quality gate is incomplete: $required"
    }
}
if ( ! $languageFrontendSuite.Contains(
         "tools\check_xlir_string_conversion_abi.ps1") ) {
    throw "XLIR closed string conversion quality gate is not wired into the language frontend suite"
}
if ( ! $runtimeMatrix.Contains(
         "TypeId, derived class and explicit class-protocol string conversion ABI") -or
     ! $runtimeMatrix.Contains("xlir_string_conversion_abi.xl") ) {
    throw "XLIR closed string conversion fixture is not wired into runtime-matrix coverage"
}
foreach ( $required in @(
    "XL6_IR_RUNTIME_CLASS_DERIVED_EQUAL",
    "XL6_IR_RUNTIME_XVALUE_CLASS_IDENTITY_BIND",
    "XL6_IrClassHashAbi", "XL6_IrClassEqualityAbi",
    "XL6_IR_CLASS_HASH_ABI_RUNTIME_REFERENCE",
    "XL6_IR_CLASS_EQUALITY_ABI_RUNTIME_REFERENCE",
    "compareDeriveIncluded", "hashAbi", "equalityAbi"
) ) {
    if ( ! $header.Contains($required) ) {
        throw "XLIR class value identity header contract is incomplete: $required"
    }
}
foreach ( $required in @(
    "xl6IrBuildClassIdentityContract",
    "xl6IrVerifyClassIdentityAbi",
    "XL6_IR_CLASS_HASH_ABI_RUNTIME_REFERENCE",
    "XL6_IR_CLASS_EQUALITY_ABI_RUNTIME_REFERENCE",
    "compareDeriveIncluded", "runtime-reference"
) ) {
    if ( ! $source.Contains($required) ) {
        throw "XLIR class value identity typed/verifier contract is incomplete: $required"
    }
}
foreach ( $required in @(
    "XL6_IR_RUNTIME_CLASS_DERIVED_EQUAL",
    "XL6_IR_RUNTIME_XVALUE_CLASS_IDENTITY_BIND",
    "XL6_IR_CLASS_HASH_ABI_RUNTIME_DERIVED",
    "XL6_IR_CLASS_EQUALITY_ABI_RUNTIME_DERIVED",
    "complete runtime hash/equality identity ABI"
) ) {
    if ( ! $lower.Contains($required) ) {
        throw "XLIR class value identity lowering contract is incomplete: $required"
    }
}
foreach ( $required in @(
    "xl6IrCgenRuntimeClassDerivedEqualAbi",
    "xl6IrCgenRuntimeClassIdentityBindAbi",
    "XL6_ClassIdentitySchemaV3", "xl6IrClassIdentityPlanGet",
    "xlir_class_identity_fields_t", "classIdentityFields",
    "XL6_IR_CLASS_IDENTITY_RESIDENT_SCHEMA_V3"
) ) {
    if ( ! $cEmitter.Contains($required) ) {
        throw "XLIR class value identity C emitter contract is incomplete: $required"
    }
}
foreach ( $required in @(
    "check_xlir_class_identity_plan.py", "PublicDirectory", "--public", "--out"
) ) {
    if ( ! $classValueIdentityAbiGate.Contains($required) ) {
        throw "XLIR class value identity quality gate is incomplete: $required"
    }
}
if ( ! $languageFrontendSuite.Contains(
         "tools\check_stdlib_identity_fields.ps1") ) {
    throw "XLIR current-source identity/fields batch is not wired into the language frontend suite"
}
foreach ($required in @('check_xlir_class_identity_plan.py','check_stdlib_dynamic_fields_resident.py',
    'check_xlir_class_identity_runtime.py','check_stdlib_object_fields_native.py','PublicDirectory','InputsUnchanged')) {
    if (!(Read-Utf8 'tools/check_stdlib_identity_fields.ps1').Contains($required)) {
        throw "Resident identity/fields batch lost an independent proof: $required"
    }
}
foreach ($required in @('cgen_derive_string_hash_equals','compareDeriveIncluded','hashAbiName','equalityAbiName',
    'xvalue-class-identity-bind','class-derived-equal','SourceInputsMatch','contract.repeat','.AOT.run')) {
    if (!(Read-Utf8 'tools/check_xlir_class_identity_plan.py').Contains($required)) {
        throw "Delegated source identity proof lost its typed/runtime check: $required"
    }
}
if (!(Read-Utf8 'src/runtime/xl6_value_object_fields.c').Contains('ops!=xl6DynamicFieldsHandlePolicyV1Get()->Ops') -or
    !(Read-Utf8 'src/cgen/xl6_ir_cgen_emit_async_support.c').Contains('classLayout->persistenceDynamicHandleField') -or
    (Read-Utf8 'src/cgen/xl6_ir_cgen_emit_xvalue_containers.c').Contains('static xrtdynamicfields* xlir_xvalue_dynamic_fields')) {
    throw 'Erased open fields must use authenticated resident storage and the verified layout path'
}
if ( ! $runtimeMatrix.Contains(
         "derived class structural hash/equality and typed-set identity ABI") -or
     ! $runtimeMatrix.Contains("cgen_derive_string_hash_equals.xl") ) {
    throw "XLIR class value identity fixture is not wired into runtime-matrix coverage"
}
if ( ! $cEmitter.Contains("runtimeOp <= XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_I64") ) {
    throw "XLIR xvalue runtime dependency range must exclude later non-xvalue intrinsics"
}

foreach ( $required in @(
    "XL6_IrClassCopyAbi", "XL6_IR_CLASS_COPY_ABI_RUNTIME_SHALLOW",
    "XL6_IR_CLASS_COPY_ABI_RUNTIME_DEEP", "XL6_IR_CLASS_COPY_ABI_INTERNAL_CALL",
    "XL6_IR_RUNTIME_XVALUE_CLONE", "XL6_IR_RUNTIME_XVALUE_DEEP_CLONE",
    "XL6_IrInitializerAbi", "XL6_IR_INITIALIZER_SCALAR_ZERO"
) ) {
    if ( ! $header.Contains($required) ) { throw "XLIR copy/clone header contract is incomplete: $required" }
}
foreach ( $required in @(
    "xl6IrBuildClassCopyContract", "xl6IrVerifyClassCopyAbi",
    "runtime dynamic clone has no exact borrowed-to-owned type ABI",
    "copyAbiName", "cloneAbiName", "initializerAbiName",
    "ownerDeclaration", "xl6IrCleanupEnclosingCallable",
    "action->ownerDeclaration != module->root"
) ) {
    if ( ! $source.Contains($required) ) { throw "XLIR copy/clone typed/verifier contract is incomplete: $required" }
}
foreach ( $required in @(
    "xl6IrLowerIntrinsicCopyClone", "XLIR copy operation has no exact xvalue source and result type ABI",
    "custom copy operation has no exact borrowed receiver and owned result call ABI",
    "XL6_IR_RUNTIME_XVALUE_CLONE", "XL6_IR_RUNTIME_XVALUE_DEEP_CLONE",
    "xl6IrLowerDefaultClassFieldInitializer",
    "xl6IrLowerMaterializeGenericClassCopyProtocols",
    "XLIR generic class copy protocol has no monomorphic callable ABI",
    "targetAction->ownerDeclaration = build->nodes[sourceAction.ownerDeclaration]",
    "XLIR default class field has no resolved initializer ABI"
) ) {
    if ( ! $lower.Contains($required) ) { throw "XLIR copy/clone lowering contract is incomplete: $required" }
}
foreach ( $required in @(
    "action->ownerDeclaration != function->declaration",
    "That state belongs to the closure environment",
    "xl6IrLowerEmitExitCleanup"
) ) {
    if ( ! $lower.Contains($required) ) { throw "XLIR callable-scoped cleanup lowering contract is incomplete: $required" }
}
foreach ( $required in @(
    "XL6_IR_RUNTIME_XVALUE_CLONE", "XL6_IR_RUNTIME_XVALUE_DEEP_CLONE",
    "xrtValueClone", "xrtValueDeepClone"
) ) {
    if ( ! $cEmitter.Contains($required) ) { throw "XLIR copy/clone C ABI contract is incomplete: $required" }
}
foreach ( $required in @(
    "xl6IrVerifyLoweredCleanupMaterialization",
    "module->functions[functionIndex].declaration == action->ownerDeclaration",
    "XLIR lowered cleanup action has no materialized ownership instruction"
) ) {
    if ( ! $source.Contains($required) ) { throw "XLIR cleanup materialization verifier contract is incomplete: $required" }
}

foreach ( $required in @(
    "XL6_IrGlobalExport", "XL6_IrGlobalMutability", "XL6_IR_GLOBAL_MUTABLE",
    "XL6_IR_GLOBAL_CONST", "globalExports", "globalExportCount",
    "initializerNode", "initializer"
) ) {
    if ( ! $header.Contains($required) ) { throw "XLIR GlobalExport header contract is incomplete: $required" }
}
foreach ( $required in @(
    "xl6IrLowerLinkedValueBinding", "xl6IrLowerValueDeclarationSymbol",
    "xl6IrLowerGlobalExportInitializer", "xl6IrLowerRegisterGlobalExport",
    "XLIR global export requires a canonical static scalar initializer ABI",
    "An explicit C data export is initialized by its public static"
) ) {
    if ( ! $lower.Contains($required) ) { throw "XLIR GlobalExport lowering contract is incomplete: $required" }
}
foreach ( $required in @(
    "xl6IrGlobalExportGet", "xl6IrGlobalExportInitializerMatches",
    "XLIR symbol lookup must nevertheless resolve the storage declaration",
    "typed XLIR name reference does not match its canonical binding storage ABI",
    "XLIR global export has no complete static scalar storage ABI",
    "XLIR const global export has a lowered write edge",
    "XLIR public global storage has no GlobalExport ABI",
    "global-export g", "globalExports"
) ) {
    if ( ! $source.Contains($required) ) { throw "XLIR GlobalExport verifier/dump contract is incomplete: $required" }
}
foreach ( $required in @(
    "xl6IrCgenPutStorageName", "xl6IrGlobalExportGet",
    "XLIR C emitter global export ABI is unresolved",
    "extern const ", "exported->initializer"
) ) {
    if ( ! $cEmitter.Contains($required) ) { throw "XLIR GlobalExport C boundary is incomplete: $required" }
}
foreach ( $required in @(
    "XL6_IrExternGlobal", "XL6_IrExternGlobalAccessAbi",
    "XL6_IR_EXTERN_GLOBAL_READ_WRITE", "XL6_IR_STORAGE_EXTERN_GLOBAL",
    "externGlobals", "externGlobalCount"
) ) {
    if ( ! $header.Contains($required) ) { throw "XLIR ExternGlobal header contract is incomplete: $required" }
}
foreach ( $required in @(
    "xl6IrExternGlobalTypeSupported", "xl6IrLowerRegisterExternGlobal",
    "XLIR extern global requires an exact trivial C storage ABI",
    "XLIR extern global has no canonical C identifier ABI"
) ) {
    if ( ! $lower.Contains($required) ) { throw "XLIR ExternGlobal lowering contract is incomplete: $required" }
}
foreach ( $required in @(
    "xl6IrExternGlobalGet", "xl6IrExternGlobalTypeSupported",
    "XLIR extern global has no complete imported trivial C storage ABI",
    "XLIR imported global storage has no ExternGlobal ABI",
    "extern-global x", "externGlobals"
) ) {
    if ( ! $source.Contains($required) ) { throw "XLIR ExternGlobal verifier/dump contract is incomplete: $required" }
}
foreach ( $required in @(
    "xl6IrExternGlobalGet", "XLIR C emitter extern global ABI is unresolved",
    "XLIR C emitter does not support extern global storage ABI",
    "extern %s %s;"
) ) {
    if ( ! $cEmitter.Contains($required) ) { throw "XLIR ExternGlobal C boundary is incomplete: $required" }
}

# task.spawn is a typed closure execution contract.  The builder owns source
# identity and canonical argument order; lowering owns the frame/result ABI;
# Cgen may only render the verified RuntimeOp.
foreach ( $required in @(
    "XL6_IR_RUNTIME_TASK_SPAWN_CLOSURE",
    "XL6_IR_INTRINSIC_TASK_SPAWN",
    "XL6_IR_ASYNC_RESULT_RETAINED_FUTURE",
    "XL6_IR_ASYNC_RESULT_RETAINED_GENERATOR"
) ) {
    if ( ! $header.Contains($required) ) {
        throw "XLIR task.spawn header contract is incomplete: $required"
    }
}
foreach ( $required in @(
    "xl6IrIsTaskSpawnCallee", "xl6IrTaskSpawnParameterAbiSupported",
    "task-spawn", "task-spawn-closure",
    "XLIR runtime task spawn has no exact closure, canonical argument frame and Future payload ABI"
) ) {
    if ( ! $source.Contains($required) ) {
        throw "XLIR task.spawn Typed/verifier contract is incomplete: $required"
    }
}
foreach ( $required in @(
    "xl6IrLowerIntrinsicTaskSpawn",
    "XL6_IR_RUNTIME_TASK_SPAWN_CLOSURE",
    "instruction->calleeType = callableBinding->type",
    "instruction->asyncResultAbi = intrinsic->payloadAbi"
) ) {
    if ( ! $lower.Contains($required) ) {
        throw "XLIR task.spawn lowering contract is incomplete: $required"
    }
}
foreach ( $required in @(
    "xl6IrCgenRuntimeTaskSpawnAbi", "xl6IrCgenEmitTaskSpawnHelper",
    "xlir_task_spawn_frame_", "xlir_task_spawn_task_",
    "xlir_task_spawn_start_", "xl6TaskSubmitPayloadV1",
    "XL6_FUTURE_PAYLOAD_FUTURE", "XL6_IR_ASYNC_RESULT_RETAINED_FUTURE",
    "XL6_FUTURE_PAYLOAD_GENERATOR", "XL6_IR_ASYNC_RESULT_RETAINED_GENERATOR",
    "xlir_generator_retain"
) ) {
    if ( ! $cEmitter.Contains($required) ) {
        throw "XLIR task.spawn C boundary is incomplete: $required"
    }
}
foreach ( $required in @(
    "xlir_task_nested_future_abi.xl", 'payloadAbiName -eq "retained-future"',
    'asyncResultAbiName -eq "retained-future"',
    "task.spawn Future argument frame retain", "independent await reference",
    "strict-C/runtime gate failed", "XLIR-only runtime contract"
) ) {
    if ( ! $taskNestedFutureGate.Contains($required) ) {
        throw "XLIR nested Future task quality gate is incomplete: $required"
    }
}
foreach ( $required in @(
    "xlir_task_spawn_closure_abi.xl", 'kindName -eq "task-spawn"',
    'runtimeOpName -eq "task-spawn-closure"',
    "byte-for-byte deterministic", "-Werror",
    "strict-C/runtime gate failed"
) ) {
    if ( ! $taskSpawnClosureGate.Contains($required) ) {
        throw "XLIR task.spawn quality gate is incomplete: $required"
    }
}
if ( ! $languageFrontendSuite.Contains(
        "tools\check_xlir_task_spawn_closure_abi.ps1") ) {
    throw "XLIR task.spawn quality gate is not wired into the language frontend suite"
}
if ( ! $runtimeMatrix.Contains("xlir_task_spawn_closure_abi.xl") ) {
    throw "XLIR task.spawn fixture is missing from the runtime matrix"
}

# Language-visible runtime resources carry a closed semantic identity that is
# distinct from their shared xvalue representation.  Thread source operations
# are resolved and verified before lowering; Cgen renders only those RuntimeOps
# and the exact TypeValueId/handle ABI supplied by XLIR.
foreach ( $required in @(
    "XL6_IrRuntimeResourceAbi", "XL6_IR_RUNTIME_RESOURCE_THREAD",
    "XL6_IR_RUNTIME_RESOURCE_OP_THREAD_CREATE",
    "XL6_IR_RUNTIME_RESOURCE_OP_THREAD_WAIT",
    "XL6_IR_RUNTIME_RESOURCE_OP_THREAD_WAIT_TIMEOUT",
    "XL6_IR_RUNTIME_RESOURCE_OP_THREAD_EXIT_CODE",
    "XL6_IR_RUNTIME_RESOURCE_OP_THREAD_STATE",
    "XL6_IR_RUNTIME_RESOURCE_OP_THREAD_STOP",
    "XL6_IR_RUNTIME_THREAD_CREATE", "XL6_IR_RUNTIME_THREAD_STOP"
) ) {
    if ( ! $header.Contains($required) ) {
        throw "XLIR thread resource header contract is incomplete: $required"
    }
}
foreach ( $required in @(
    "xl6IrRuntimeResourceAbiFromName", "xl6IrRuntimeResourceCalleeOp",
    "xl6IrRuntimeResourceSignatureCompatible", "runtimeResourceOp",
    "XL6_IR_C_ABI_RUNTIME_XVALUE_PTR", "thread-create",
    "XLIR runtime resource intrinsic has no exact resource, operand, result or ownership ABI"
) ) {
    if ( ! $source.Contains($required) ) {
        throw "XLIR thread resource Typed/verifier contract is incomplete: $required"
    }
}
foreach ( $required in @(
    "xl6IrLowerRuntimeResourceSignatureCompatible",
    "xl6IrLowerRuntimeResourceOp", "xl6IrLowerIntrinsicRuntimeResource",
    "XL6_IR_RUNTIME_THREAD_CREATE", "instruction->calleeType",
    "XLIR runtime resource operand has no exact lowered value ABI"
) ) {
    if ( ! $lower.Contains($required) ) {
        throw "XLIR thread resource lowering contract is incomplete: $required"
    }
}
foreach ( $required in @(
    "xl6IrCgenThreadTypeAbi", "xl6IrCgenThreadCallableAbi",
    "xl6IrCgenRuntimeThreadAbi", "xl6IrCgenEmitThreadRuntime",
    "needsThreadResourceRuntime", "xl6ThreadCreateV1", "xl6ThreadWaitV1",
    "xl6ThreadWaitTimeoutV1", "xl6ThreadExitCodeV1", "xl6ThreadStateV1", "xl6ThreadStopV1",
    "xlir_closure_retain (xl6ClosureRuntimeV1Get()->Retain)",
    "xlir_closure_release (xl6ClosureRuntimeV1Get()->Release)"
) ) {
    if ( ! $cEmitter.Contains($required) ) {
        throw "XLIR thread resource C boundary is incomplete: $required"
    }
}
# Thread owners still require atomic closure references; their implementation
# moved to the resident runtime, while generated owners call that exact ABI.
foreach ($required in @('xrtRefRetain(&closure->refs)', 'xrtRefRelease(&closure->refs)')) {
    if (!$closureRuntime.Contains($required) -or $cEmitter.Contains($required)) {
        throw "Atomic closure reference implementation must be resident and unique: $required"
    }
}
$threadRuntime = Read-Utf8 'src/runtime/xl6_thread.c'
foreach ($required in @('xl6ClosureRuntimeV1Get()->Retain', 'xl6ClosureRuntimeV1Get()->Release', 'xrtRefRetain', 'xrtRefRelease',
    'xrtOwnershipMutationBegin', 'workerActive')) {
    if (!$threadRuntime.Contains($required)) { throw "Thread closure ownership boundary missing: $required" }
}
foreach ( $required in @(
    "xlir_thread_resource_abi.xl", "runtimeResourceAbiName", "runtimeResourceOpName",
    "runtimeOpName", "retain-xvalue", "conditional", "repeat.read_bytes()", "-Werror"
) ) {
    if ( ! ($threadLanguageGate+$threadPublicGate).Contains($required) ) {
        throw "XLIR thread resource quality gate is incomplete: $required"
    }
}
if (!$threadResourceGate.Contains('check_xlir_runtime_operand_catalog.ps1')) {
    throw 'Thread resource acceptance must execute compiled current-source ownership and resident Thread tests'
}
if ( ! $languageFrontendSuite.Contains(
        "tools\check_xlir_thread_resource_abi.ps1") ) {
    throw "XLIR thread resource quality gate is not wired into the language frontend suite"
}
if ( ! $runtimeMatrix.Contains("xlir_thread_resource_abi.xl") ) {
    throw "XLIR thread resource fixture is missing from the runtime matrix"
}

# Channel returns a fresh owning (status, optional payload) product. Its
# closed schema and physical error ABI are verified before mechanical Cgen.
foreach ( $required in @(
    "XL6_IR_RUNTIME_RESOURCE_CHANNEL",
    "XL6_IrRuntimeResourcePayloadAbi",
    "XL6_IR_RUNTIME_RESOURCE_PAYLOAD_TRIVIAL_COPY",
    "XL6_IR_RUNTIME_RESOURCE_PAYLOAD_STRING_COPY",
    "XL6_IR_RUNTIME_RESOURCE_PAYLOAD_XVALUE_RETAIN",
    "XL6_IR_RUNTIME_CHANNEL_CREATE", "XL6_IR_RUNTIME_CHANNEL_SEND",
    "XL6_IrChannelReceivePlan", "XL6_IR_CALL_ERROR_PHYSICAL_STATUS_OUTPUT",
    "runtimeResourcePayloadAbi"
) ) {
    if ( ! $header.Contains($required) ) {
        throw "XLIR channel resource header contract is incomplete: $required"
    }
}
foreach ( $required in @(
    "xl6IrChannelReceivePlanBuild", "xl6IrChannelReceiveSignatureCompatible",
    "xl6IrChannelReceiveDependentSignatureCompatible", "xl6IrChannelReceiveIntrinsicCompatible",
    "xl6IrOptionalTryResultCompatible", "XL6_IR_CALL_ERROR_PHYSICAL_STATUS_OUTPUT"
) ) {
    if ( ! $channelReceivePlan.Contains($required) ) {
        throw "XLIR channel resource Typed/verifier contract is incomplete: $required"
    }
}
foreach ( $required in @(
    "xl6IrLowerRuntimeChannelPayloadType", "xl6IrChannelReceiveSpecResource",
    "xl6IrChannelReceiveIntrinsicCompatible", "XL6_IR_RUNTIME_CHANNEL_CREATE",
    "XL6_IR_RUNTIME_CHANNEL_SEND"
) ) {
    if ( ! $lower.Contains($required) ) {
        throw "XLIR channel resource lowering contract is incomplete: $required"
    }
}
foreach ( $required in @(
    "xl6IrChannelInstructionCompatible", "xl6IrChannelReceiveInstructionPlan",
    "xl6ChannelCreateTakeV1", "xl6ChannelReceiveV1", "XL6_ChannelStatusV1",
    "xl6ErrorScopeCaptureFailure", "xl6IrCgenEmitOwnedPlaceRelease"
) ) {
    if ( ! $channelNativeEmitter.Contains($required) ) {
        throw "XLIR channel resource C boundary is incomplete: $required"
    }
}
foreach ( $required in @(
    "xlir_channel_resource_abi", "xlir_channel_captured_output_abi",
    "runtimeResourceOpName", "runtimeOpName", "'storage-ref'", "'__tuple'",
    "repeat.read_bytes()", "-Werror", "NativeGraphPrefixSweeps", "-aot-run"
) ) {
    if ( ! $channelOwnedGate.Contains($required) ) {
        throw "XLIR channel resource quality gate is incomplete: $required"
    }
}
if (!$channelResourceGate.Contains('check_stdlib_channel_regressions.py')) {
    throw 'The Channel resource entry must execute the shared current-source owning result gate'
}
if ( ! $languageFrontendSuite.Contains(
        "tools\check_xlir_channel_resource_abi.ps1") ) {
    throw "XLIR channel resource quality gate is not wired into the language frontend suite"
}
if ( ! $runtimeMatrix.Contains("xlir_channel_resource_abi.xl") ) {
    throw "XLIR channel resource fixture is missing from the runtime matrix"
}

# Synchronization primitives are one exact runtime-resource family.  Typed
# XLIR fixes constructor/member effects and operands, lowering adds explicit
# constructor failure edges, and Cgen only selects a fixed helper/TypeValueId.
foreach ( $required in @(
    "XL6_IR_RUNTIME_RESOURCE_MUTEX", "XL6_IR_RUNTIME_RESOURCE_SEM",
    "XL6_IR_RUNTIME_RESOURCE_COND", "XL6_IR_RUNTIME_RESOURCE_RWLOCK",
    "XL6_IR_RUNTIME_RESOURCE_OP_MUTEX_CREATE",
    "XL6_IR_RUNTIME_RESOURCE_OP_SEM_CREATE_DEFAULT",
    "XL6_IR_RUNTIME_RESOURCE_OP_COND_WAIT_TIMEOUT",
    "XL6_IR_RUNTIME_RESOURCE_OP_RWLOCK_UPGRADE",
    "XL6_IR_INITIALIZER_RUNTIME_RESOURCE_DEFAULT", "runtimeResourceOp"
) ) {
    if ( ! $header.Contains($required) ) {
        throw "XLIR synchronization resource header contract is incomplete: $required"
    }
}
foreach ( $required in @(
    "xl6IrRuntimeResourceOpIsSync", "xl6IrRuntimeSyncOpIs",
    "xl6IrRuntimeSyncOpCreates", "xl6IrRuntimeSyncResourceAbi",
    "XLIR runtime synchronization operation has no exact resource identity ABI",
    "XLIR runtime synchronization create has no exact arguments, owned result or failure ABI",
    "runtime-resource-default", "sem-create-default"
) ) {
    if ( ! $source.Contains($required) ) {
        throw "XLIR synchronization Typed/verifier contract is incomplete: $required"
    }
}
foreach ( $required in @(
    "xl6IrLowerRuntimeResourceOpIsSync",
    "xl6IrLowerRuntimeResourceAbiForOp",
    "xl6IrLowerRuntimeResourceSignatureCompatible",
    "XL6_IR_RUNTIME_SEM_CREATE_DEFAULT",
    "XL6_IR_RUNTIME_COND_WAIT_TIMEOUT",
    "XL6_IR_RUNTIME_RWLOCK_UPGRADE"
) ) {
    if ( ! $lower.Contains($required) ) {
        throw "XLIR synchronization resource lowering contract is incomplete: $required"
    }
}
foreach ( $required in @(
    "xl6IrCgenSyncRuntimeOpIs", "xl6IrCgenSyncRuntimeOpCreates",
    "xl6IrCgenSyncResourceAbi", "xl6IrCgenRuntimeSyncAbi",
    "xl6IrCgenEmitSyncRuntime", "needsSyncResourceRuntime",
    "xlir_sync_handle_ops", "xrtRefRetain(&handle->refs)",
    "xrtRefRelease(&handle->refs)", "xrtValueHandleTake",
    "xrtValueTypeIdBind", "xrtValueGetHandle",
    "handle->type_id != typeId", "handle->kind != kind"
) ) {
    if ( ! $cEmitter.Contains($required) ) {
        throw "XLIR synchronization resource C boundary is incomplete: $required"
    }
}
foreach ( $required in @(
    "cgen_sync.xl", '"mutex", "sem", "cond", "rwlock"',
    "runtime-resource-default", "22 explicit error edges",
    "byte-for-byte deterministic", "-Werror",
    "strict-C/runtime gate failed"
) ) {
    if ( ! $syncResourceGate.Contains($required) ) {
        throw "XLIR synchronization resource quality gate is incomplete: $required"
    }
}
if ( ! $languageFrontendSuite.Contains(
        "tools\check_xlir_sync_resource_abi.ps1") ) {
    throw "XLIR synchronization resource quality gate is not wired into the language frontend suite"
}
if ( ! $runtimeMatrix.Contains("cgen_sync.xl") ) {
    throw "XLIR synchronization resource fixture is missing from the runtime matrix"
}

# Concurrent queues are closed runtime-resource operations. Typed XLIR fixes
# the queue family/payload/effect ABI, lowering produces storage addresses and
# explicit error edges, and Cgen only renders fixed queue helpers plus TypeIds.
foreach ( $required in @(
    "XL6_IR_RUNTIME_RESOURCE_SPSC_QUEUE",
    "XL6_IR_RUNTIME_RESOURCE_MPSC_QUEUE",
    "XL6_IR_RUNTIME_RESOURCE_MPMC_QUEUE",
    "XL6_IR_RUNTIME_RESOURCE_WAIT_QUEUE",
    "XL6_IR_RUNTIME_RESOURCE_OP_SPECIAL_QUEUE_CREATE",
    "XL6_IR_RUNTIME_RESOURCE_OP_SPECIAL_QUEUE_POP_TIMEOUT_INTO",
    "XL6_IR_RUNTIME_SPECIAL_QUEUE_CREATE",
    "XL6_IR_RUNTIME_SPECIAL_QUEUE_IS_DRAINED"
) ) {
    if ( ! $header.Contains($required) ) {
        throw "XLIR special queue header contract is incomplete: $required"
    }
}
foreach ( $required in @(
    "xl6IrRuntimeResourceOpIsSpecialQueue",
    "xl6IrRuntimeSpecialQueuePayloadType",
    "xl6IrSpecialQueueAbiForName",
    "XLIR runtime special queue operation has no exact resource and payload identity ABI",
    "XLIR runtime special queue receive has no exact storage address",
    "special-queue-pop-timeout-into"
) ) {
    if ( ! $source.Contains($required) ) {
        throw "XLIR special queue Typed/verifier contract is incomplete: $required"
    }
}
foreach ( $required in @(
    "xl6IrLowerRuntimeResourceOpIsSpecialQueue",
    "xl6IrLowerRuntimeSpecialQueuePayloadType",
    "XL6_IR_RUNTIME_SPECIAL_QUEUE_CREATE",
    "XL6_IR_RUNTIME_SPECIAL_QUEUE_POP_TIMEOUT_INTO",
    "XL6_IR_RUNTIME_SPECIAL_QUEUE_IS_DRAINED"
) ) {
    if ( ! $lower.Contains($required) ) {
        throw "XLIR special queue lowering contract is incomplete: $required"
    }
}
foreach ( $required in @(
    "xl6IrCgenSpecialQueueRuntimeOpIs",
    "xl6IrCgenSpecialQueueTypeAbi",
    "xl6IrCgenRuntimeSpecialQueueAbi",
    "xl6IrCgenEmitSpecialQueueRuntime",
    "xlir_special_queue_create", "xlir_special_queue_try_push",
    "xlir_special_queue_pop", "xlir_special_queue_is_drained",
    "xrtChannelDrain(handle->channel, xlir_channel_drain_item, handle)",
    "xrtChannelCapacity", "xrtChannelReset"
) ) {
    if ( ! $cEmitter.Contains($required) ) {
        throw "XLIR special queue C boundary is incomplete: $required"
    }
}
foreach ( $required in @(
    "stdlib_special_collections_concurrent.xl",
    'runtimeResourceOpName -like "special-queue-*"',
    'runtimeOpName -like "special-queue-*"',
    "retain-xvalue", "explicit error edges",
    "byte-for-byte deterministic", "-Werror",
    "strict-C/runtime gate failed"
) ) {
    if ( ! $specialQueueResourceGate.Contains($required) ) {
        throw "XLIR special queue quality gate is incomplete: $required"
    }
}
if ( ! $languageFrontendSuite.Contains(
        "tools\check_xlir_special_queue_resource_abi.ps1") ) {
    throw "XLIR special queue quality gate is not wired into the language frontend suite"
}
if ( ! $runtimeMatrix.Contains("stdlib_special_collections_concurrent.xl") ) {
    throw "XLIR special queue fixture is missing from the runtime matrix"
}

# Stack<T> is an owning generic runtime resource. Typed XLIR owns its payload,
# class-field default and effect contracts; lowering owns exact return values and
# partial-object cleanup; Cgen renders only verified RuntimeOps and TypeValueIds.
foreach ( $required in @(
    "XL6_IR_RUNTIME_RESOURCE_STACK",
    "XL6_IR_RUNTIME_RESOURCE_OP_STACK_CREATE",
    "XL6_IR_RUNTIME_RESOURCE_OP_STACK_PEEK_AT",
    "XL6_IR_RUNTIME_RESOURCE_OP_STACK_TRY_POP",
    "XL6_IR_RUNTIME_STACK_CREATE", "XL6_IR_RUNTIME_STACK_CAPACITY",
    "initializerRuntimeResourceOp", "initializerPayloadType",
    "initializerRuntimeResourcePayloadAbi", "initializerErrorAbi"
) ) {
    if ( ! $header.Contains($required) ) {
        throw "XLIR Stack resource header contract is incomplete: $required"
    }
}
foreach ( $required in @(
    "xl6IrRuntimeResourceOpIsStack", "xl6IrRuntimeStackPayloadType",
    "xl6IrDefaultRuntimeResourceOp", "xl6IrDefaultClassInitializerErrorAbi",
    "XLIR runtime Stack operation has no exact resource and payload identity ABI",
    "runtime-resource-default", "stack-create"
) ) {
    if ( ! $source.Contains($required) ) {
        throw "XLIR Stack Typed/verifier contract is incomplete: $required"
    }
}
foreach ( $required in @(
    "xl6IrLowerRuntimeStackPayloadType", "xl6IrLowerRuntimeResourceOpCreates",
    "XL6_IR_RUNTIME_STACK_CREATE", "XL6_IR_RUNTIME_STACK_PEEK_AT",
    "XL6_IR_RUNTIME_STACK_TRY_POP",
    "xl6IrLowerBeginClassConstruction", "xl6IrLowerCommitClassConstruction",
    "XL6_IR_RUNTIME_CLASS_FIELD_INIT_NEW", "XL6_IR_INSTR_ERROR_TEST"
) ) {
    if ( ! $lower.Contains($required) ) {
        throw "XLIR Stack lowering/partial-construction contract is incomplete: $required"
    }
}
foreach ( $required in @(
    "xl6IrCgenStackRuntimeOpIs", "xl6IrCgenStackTypeAbi",
    "xl6IrCgenRuntimeStackAbi", "xl6IrCgenEmitStackRuntime",
    "needsStackResourceRuntime", "xl6SequencePrimitiveCreateV1",
    "xl6SequenceReferenceCreateV1", "xl6SequenceNominalCreateV1",
    "xl6SequenceCScalarCreateV1", "xl6SequenceNullableCScalarCreateV1",
    "xl6SequenceNumericEnumCreateV1", "xl6SequenceNestedCreateTakeV2",
    "xl6StackTakeV1", "xl6StackTryTakeV2",
    "xl6StackCloneV1", "xl6StackSizeV1"
) ) {
    if ( ! $cEmitter.Contains($required) ) {
        throw "XLIR Stack mechanical C boundary is incomplete: $required"
    }
}
foreach ( $retired in @(
    "XL6_IR_RUNTIME_RESOURCE_OP_STACK_POP_INTO",
    "XL6_IR_RUNTIME_RESOURCE_OP_STACK_PEEK_INTO",
    "XL6_IR_RUNTIME_RESOURCE_OP_STACK_PEEK_AT_INTO",
    "XL6_IR_RUNTIME_STACK_POP_INTO", "XL6_IR_RUNTIME_STACK_PEEK_INTO",
    "XL6_IR_RUNTIME_STACK_PEEK_AT_INTO", "xl6StackPopOrPeekV1", "xl6StackPeekAtV1", "xl6StackTryTakeV1"
) ) {
    if ( $header.Contains($retired) -or $source.Contains($retired) -or
         $lower.Contains($retired) -or $cEmitter.Contains($retired) ) {
        throw "Retired Stack caller-writeback path returned: $retired"
    }
}
foreach ( $required in @(
    "xlir_stack_resource_abi.xl", "xlir_stack_class_default_abi.xl",
    'runtimeResourceOpName -like "stack-*"',
    'runtimeOpName -like "stack-*"', "storage-ref", "retain-xvalue",
    "partial-object cleanup", "byte-for-byte deterministic", "-Werror",
    "strict-C/runtime gate failed"
) ) {
    if ( ! $stackResourceGate.Contains($required) ) {
        throw "XLIR Stack resource quality gate is incomplete: $required"
    }
}
if ( ! $languageFrontendSuite.Contains(
        "tools\check_xlir_stack_resource_abi.ps1") ) {
    throw "XLIR Stack resource quality gate is not wired into the language frontend suite"
}
if ( ! $runtimeMatrix.Contains("xlir_stack_resource_abi.xl") ) {
    throw "XLIR Stack resource fixture is missing from the runtime matrix"
}

# OrderedMap<K,V> extends the resource contract with a second generic identity and
# an ordering ABI.  Typed XLIR owns both types, order, effects and storage refs;
# lowering normalizes optional merge syntax; Cgen renders only verified ops.
foreach ( $required in @(
    "XL6_IR_RUNTIME_RESOURCE_AVL_TREE",
    "XL6_IR_RUNTIME_RESOURCE_OP_AVL_TREE_CREATE",
    "XL6_IR_RUNTIME_RESOURCE_OP_AVL_TREE_UPPER_BOUND_INTO",
    "XL6_IR_RUNTIME_AVL_TREE_CREATE", "XL6_IR_RUNTIME_AVL_TREE_CLONE",
    "runtimeResourceKeyType", "runtimeResourceKeyPayloadAbi",
    "runtimeResourceKeyOrderAbi", "initializerRuntimeResourceKeyType",
    "initializerRuntimeResourceKeyOrderAbi"
) ) {
    if ( ! $header.Contains($required) ) {
        throw "XLIR OrderedMap resource header contract is incomplete: $required"
    }
}
foreach ( $required in @(
    "xl6IrRuntimeResourceOpIsAvlTree", "xl6IrRuntimeAvlKeyType",
    "xl6IrRuntimeAvlValueType", "xl6IrRuntimeAvlOpMayRaise",
    "XLIR runtime OrderedMap operation has no exact key, value, order, ownership or failure ABI",
    "runtimeResourceKeyOrderAbi", "avl-tree-merge"
) ) {
    if ( ! $source.Contains($required) ) {
        throw "XLIR OrderedMap Typed/verifier contract is incomplete: $required"
    }
}
foreach ( $required in @(
    "xl6IrLowerRuntimeResourceOpIsAvlTree", "xl6IrLowerRuntimeAvlKeyType",
    "xl6IrLowerRuntimeAvlValueType", "XL6_IR_RUNTIME_AVL_TREE_CREATE",
    "XL6_IR_RUNTIME_AVL_TREE_CLONE",
    "gives Cgen one exact merge ABI",
    "runtimeOperandCount = 3"
) ) {
    if ( ! $lower.Contains($required) ) {
        throw "XLIR OrderedMap lowering/normalization contract is incomplete: $required"
    }
}
foreach ( $required in @(
    "xl6IrCgenAvlRuntimeOpIs", "xl6IrCgenAvlRuntimeOpMayRaise",
    "xl6IrCgenAvlKeyOrderAbi", "xl6IrCgenNativeValueTypeExpr",
    "xl6IrCgenAvlTypeAbi", "xl6IrCgenRuntimeAvlAbi",
    "xl6IrCgenEmitAvlRuntime", "needsAvlTreeResourceRuntime",
    "xlir_avl_handle_ops", "xlir_avl_create", "xlir_avl_get_or_take",
    "xlir_avl_collect", "xrtTypedTreeCreate"
) ) {
    if ( ! $cEmitter.Contains($required) ) {
        throw "XLIR OrderedMap mechanical C boundary is incomplete: $required"
    }
}
foreach ( $required in @(
    "xlir_avl_resource_abi.xl", "xlir_avl_class_default_abi.xl",
    'runtimeResourceOpName -like "avl-tree-*"',
    'runtimeOpName -like "avl-tree-*"', "storage-ref", "retain-xvalue",
    "operandCount -ne 3", "partial-object cleanup",
    "byte-for-byte deterministic", "declaration-only C", "-Werror",
    "strict-C/runtime gate failed"
) ) {
    if ( ! $avlResourceGate.Contains($required) ) {
        throw "XLIR OrderedMap resource quality gate is incomplete: $required"
    }
}
if ( ! $languageFrontendSuite.Contains(
        "tools\check_xlir_avl_resource_abi.ps1") ) {
    throw "XLIR OrderedMap resource quality gate is not wired into the language frontend suite"
}
if ( ! $runtimeMatrix.Contains("xlir_avl_resource_abi.xl") ) {
    throw "XLIR OrderedMap resource fixture is missing from the runtime matrix"
}

# Future continuations are one closed, verified operation family.  Typed XLIR
# owns source/output payload types and callback signatures; lowering fixes the
# continuation mode and physical payload ABIs; Cgen only renders that RuntimeOp.
foreach ( $required in @(
    "XL6_IR_RUNTIME_FUTURE_CONTINUE_CLOSURE",
    "XL6_IR_INTRINSIC_FUTURE_THEN", "XL6_IR_INTRINSIC_FUTURE_CATCH",
    "XL6_IR_INTRINSIC_FUTURE_FINALLY", "XL6_IrFutureContinuationKind",
    "sourceAsyncResultAbi", "futureContinuationKind", "sourcePayloadType"
) ) {
    if ( ! $header.Contains($required) ) {
        throw "XLIR Future continuation header contract is incomplete: $required"
    }
}
foreach ( $required in @(
    "future-then", "future-catch", "future-finally",
    "XLIR Future continuation intrinsic has no exact receiver, closure, payload or failure ABI",
    "XLIR runtime Future continuation has no exact source, closure, payload or mode ABI",
    "sourcePayloadAbi", "xl6IrFutureContinuationKindName"
) ) {
    if ( ! $source.Contains($required) ) {
        throw "XLIR Future continuation Typed/verifier contract is incomplete: $required"
    }
}
foreach ( $required in @(
    "xl6IrLowerIntrinsicFutureContinuation",
    "XL6_IR_RUNTIME_FUTURE_CONTINUE_CLOSURE",
    "instruction->sourcePayloadType = intrinsic->sourcePayloadType",
    "instruction->sourceAsyncResultAbi = intrinsic->sourcePayloadAbi",
    "instruction->futureContinuationKind = continuationKind"
) ) {
    if ( ! $lower.Contains($required) ) {
        throw "XLIR Future continuation lowering contract is incomplete: $required"
    }
}
foreach ( $required in @(
    "xl6IrCgenRuntimeFutureContinuationAbi",
    "xl6IrCgenEmitFutureContinuationHelper", "xlir_future_continue_frame_",
    "xlir_future_continue_proc_", "xlir_future_continue_start_",
    "xrtFutureContinueOwned", "instruction->sourceAsyncResultAbi",
    "instruction->futureContinuationKind"
) ) {
    if ( ! $cEmitter.Contains($required) ) {
        throw "XLIR Future continuation C boundary is incomplete: $required"
    }
}
foreach ( $required in @(
    "stdlib_future_continuation.xl",
    'kindName -in @("future-then", "future-catch", "future-finally")',
    "sourcePayloadAbiName", 'runtimeOpName -eq "future-continue-closure"',
    "futureContinuationKindName", "Future<T> to Future<U>",
    "byte-for-byte deterministic", "task-boundary AsyncError cause",
    "strict-C/runtime gate failed"
) ) {
    if ( ! $futureContinuationGate.Contains($required) ) {
        throw "XLIR Future continuation quality gate is incomplete: $required"
    }
}
if ( ! $languageFrontendSuite.Contains(
        "tools\check_xlir_future_continuation_abi.ps1") ) {
    throw "XLIR Future continuation quality gate is not wired into the language frontend suite"
}
if ( ! $runtimeMatrix.Contains("stdlib_future_continuation.xl") ) {
    throw "XLIR Future continuation fixture is missing from the runtime matrix"
}

# User-class destruction is a verified lifecycle plan, not a Cgen spelling
# heuristic.  Typed XLIR owns exact destroy declarations and derived-to-base
# order; allocation prepares the code owner before publication, while the
# successful commit arms the plan. The finalizer receiver remains a pure borrow.
foreach ( $required in @(
    "XL6_IrCallTargetLifecycleAbi",
    "XL6_IR_CALL_TARGET_LIFECYCLE_CLASS_DESTROY_BORROW",
    "XL6_IR_RUNTIME_XVALUE_CLASS_CONSTRUCTION_COMMIT",
    "XL6_IrClassStorageAbi", "XL6_IR_CLASS_STORAGE_RESIDENT_GRAPH_V2",
    "XL6_IrClassDestroyStep", "firstDestroyStep", "destroyStepCount", "destroyFunction",
    "XL6_IR_FUNCTION_CLASS_FINI", "XL6_IR_INSTR_CLEANUP_FAIL",
    "classDestroySteps"
) ) {
    if ( ! $header.Contains($required) ) {
        throw "XLIR class lifecycle header contract is incomplete: $required"
    }
}
foreach ( $required in @(
    "xl6IrBuildClassDestroyPlans", "xl6IrClassDestroyTargetMatches", "xl6IrVerifyClassFinalizers",
    "xl6IrVerifyClassConstruction", "xl6IrClassPreparedAllocation", "classStorageAbi",
    "class-destroy-borrow", "xvalue-class-construction-commit",
    "classDestroySteps"
) ) {
    if ( ! $source.Contains($required) ) {
        throw "XLIR class lifecycle typed/verifier contract is incomplete: $required"
    }
}
foreach ( $required in @(
    "xl6IrLowerCommitClassLifetime", "xl6IrLowerClassFinalizers",
    "xl6IrLowerParameterBindingOwnsStorage",
    "XL6_IR_CALL_TARGET_LIFECYCLE_CLASS_DESTROY_BORROW",
    "XL6_IR_RUNTIME_XVALUE_PROJECT_XVALUE_RETAIN",
    "XL6_IR_RUNTIME_XVALUE_CLASS_CONSTRUCTION_COMMIT"
) ) {
    if ( ! $lower.Contains($required) ) {
        throw "XLIR class lifecycle lowering contract is incomplete: $required"
    }
}
foreach ( $required in @(
    "xl6IrCgenRuntimeClassConstructionCommitAbi",
    "xl6IrCgenEmitClassDestroyHelpers",
    "xl6ClassRuntimeV2Get()->Create", "xl6ClassRuntimeV2Get()->CommitTake", "xl6ClassRuntimeV2Get()->BindIdentity",
    "xlir_class_destroy_finalizer_t"
) ) {
    if ( ! $cEmitter.Contains($required) ) {
        throw "XLIR class lifecycle C boundary is incomplete: $required"
    }
}
$classRuntime = Read-Utf8 'src/runtime/xl6_code_finalizer.c'
foreach ($required in @('xrtValueObjectFinalizerPrepareOwned', 'xrtValueObjectConstructionCommit',
    'xrtValueObjectConstructionPrepare', 'xrtValueObjectLifetimeBindOwned',
    'class_lifetime_trace', 'class_lifetime_release', 'ownsLease',
    'xl6CodeLeaseAcquireScoped', 'xl6ClassRuntimeV1Get', 'xl6ClassRuntimeV2Get',
    'xrtValueObjectOwnershipBindV1', 'xrtValueObjectIdentityBindV1', 'class_ownership_policy',
    'class_create_checked', 'code_finalizer_checked', 'xl6ErrorScopeCaptureFailure')) {
    if (!$classRuntime.Contains($required)) { throw "Resident prepared class lifetime missing: $required" }
}
if ($classRuntime.Contains('class_no_finalizer')) { throw 'Copyable code capability must not impose a synthetic finalization identity' }
if (!$cEmitter.Contains('static bool xlir_class_destroy_finalizer_t') -or
    $cEmitter.Contains('static void xlir_class_destroy_finalizer_t')) {
    throw 'Verified V2 class destruction must return an explicit outcome, independent of diagnostic allocation'
}
if (!(Read-Utf8 'tools/run_xlir_ownership_batch.ps1').Contains('check_xlir_class_copy_storage.ps1') -or
    !(Read-Utf8 'tools/check_xlir_class_copy_storage.ps1').Contains('100 generated hash callbacks after Close')) {
    throw 'Copyable class capability needs original copy fixtures and real generated-code lifetime coverage'
}
foreach ($forbidden in @('xlir_class_commit_t', 'xrtValueObjectFinalizerBindTake(')) {
    if ($cEmitter.Contains($forbidden)) { throw "Cgen must not implement class publication: $forbidden" }
}
if (!(Read-Utf8 'tools/run_xlir_ownership_batch.ps1').Contains('check_xlir_class_storage.ps1') -or
    !(Read-Utf8 'tools/check_xlir_class_storage.ps1').Contains('24 rejected Typed/Lowered/C/header mutations')) {
    throw 'Prepared class storage must retain its additive IR/native/actual-TCC ownership gate'
}
foreach ( $required in @(
    "cgen_var_destroy_class_init.xl", "cgen_class_destroy.xl",
    "cgen_class_inherit_destroy.xl", "hasExplicitCreate",
    "class-destroy-borrow", "xvalue-class-construction-commit",
    "derived-to-base", "retained projection", "SequenceEqual",
    "strict-C/runtime gate failed"
) ) {
    if ( ! $classLifecycleGate.Contains($required) ) {
        throw "XLIR class lifecycle quality gate is incomplete: $required"
    }
}
if ( ! $languageFrontendSuite.Contains(
        "tools\check_xlir_class_lifecycle_abi.ps1") ) {
    throw "XLIR class lifecycle quality gate is not wired into the language frontend suite"
}

Write-Host "XLIR contract ok: owned typed IR, explicit lowering, verifier and CLI dump are wired."

# The shared frame contract now describes generator/iterator owners explicitly.
foreach ($required in @('XL6_IrContinuationFrameTraceAbi', 'XL6_IR_FRAME_TRACE_VALUE_ITERATOR',
    'XL6_IR_FRAME_TRACE_GENERATOR', 'traceAbi', 'itemTraceAbi')) {
    if (!$header.Contains($required) -or !$source.Contains($required)) { throw "Continuation frame graph IR contract missing: $required" }
}
if (!(Read-Utf8 'src/cgen/xl6_ir_cgen_async_machine.c').Contains('xl6ContinuationFrameRuntimeV3Get()->Bind') -or
    !(Read-Utf8 'src/runtime/xl6_continuation_future.c').Contains('xrtFutureWatchInitTraced') -or
    !(Read-Utf8 'src/runtime/xl6_continuation_queue.c').Contains('ownershipRefs')) {
    throw 'Continuation trace admission must preserve independent frame and transport leases'
}
$frameTrace = Read-Utf8 'src/cgen/xl6_ir_cgen_frame_trace.c'
$asyncOwnership = Read-Utf8 'src/runtime/xl6_continuation_frame.c'
$futureWait = Read-Utf8 'src/runtime/xl6_future_wait.c'
foreach ($required in @('xl6ContinuationFutureWaitV1','xl6CallbackFutureWaitV1','xrtPromiseOwnership(single.output)',
    'xrtFutureCombineWaitV1','xrtPromiseOwnership(multiple.Output)','multiple.Sources','multiple.Count',
    'XFUTURE_WAIT_ALL_TERMINAL','XFUTURE_WAIT_ANY_TERMINAL','XL6_FUTURE_WAIT_RESOURCE_FAILURE',
    'XFUTURE_PENDING','xl6FutureWaitCycleMemberV2')) {
    if (!$futureWait.Contains($required)) { throw "Shared resident wait dependency proof missing: $required" }
}
if (!$asyncOwnership.Contains('xl6FutureWaitCycleMemberV2') -or !$asyncOwnership.Contains('xl6ContinuationQueuePrepareOwns') -or
    !$asyncOwnership.Contains('proof==XL6_FUTURE_WAIT_CYCLE_MEMBER') -or
    $asyncOwnership.Contains('static bool wait_cycle_member') -or
    !(Read-Utf8 'src/runtime/xl6_future_callback.c').Contains('frame->claim!=token')) {
    throw 'Mixed producer waits require exact claimed runtime descriptors, not private continuation-only traversal'
}
if ($futureWait -match 'xrtFuture(?:Cancel|WatchRemove)\s*\(' -or
    !$futureWait.Contains('xrtSetErrorTake(prior)') -or
    !(Read-Utf8 'src/runtime/xl6_future_wait.h').Contains('XL6_FUTURE_WAIT_UNPROVEN')) {
    throw 'Borrowed wait proof must preserve diagnostics and leave actual interruption to the independently prepared producer'
}
foreach ($file in @('tools/check_xlir_future_wait_oracle.ps1','tools/check_xlir_aggregate_dependencies.ps1',
    'test/cases/xlir_future_wait_oracle.c','test/cases/xlir_module_aggregate_dependency.xl',
    'test/cases/xlir_module_aggregate_dependency_host.c','test/cases/xlir_module_aggregate_dependency_artifact.c')) {
    if (!(Test-Path -LiteralPath (Join-Path $Root $file) -PathType Leaf)) { throw "Multi-input proof requires independent algorithm and actual generated module witnesses: $file" }
}
if (!$header.Contains('XL6_IrAsyncOwnershipAbi')) { throw 'Async producer ownership must have a named IR ABI' }
foreach ($required in @('XL6_IR_ASYNC_OWNERSHIP_STABLE_PRODUCER_WATCH_V1','ownershipAbi')) {
    if (!$header.Contains($required) -or !$source.Contains($required)) { throw "Async producer ownership must be explicit verified IR: $required" }
}
foreach ($required in @('xrtPromiseProducerBindTakeV2','xrtFutureWatchInitOwnershipV1','producer->current=frame',
    'producer->output!=*frame_outer(frame)','producer_commit','detach_producer','producer_adapter')) {
    if (!$asyncOwnership.Contains($required)) { throw "Resident async owning-edge protocol missing: $required" }
}
foreach ($symbol in @('xl6ContinuationFrameRuntimeV3Get','xl6ContinuationFrameRuntimeV4Get','xl6ContinuationProducerPolicyV1Get','xl6ContinuationWatchPolicyV1Get')) {
    if (!(Read-Utf8 'src/api/xl_api_symbols.c').Contains('"'+$symbol+'", (const void*)'+$symbol) -or
        !(Read-Utf8 'test/cases/xlir_one_shot_tcc_host.c').Contains('BIND('+$symbol+')')) {
        throw "Async ownership runtime must be available in production and manual source TCC: $symbol"
    }
}
if (!(Read-Utf8 'src/cgen/xl6_ir_cgen_async_machine.c').Contains('xl6ContinuationFrameRuntimeV4Get()->Observe') -or
    (Read-Utf8 'src/cgen/xl6_ir_cgen_async_machine.c').Contains('frame->cancelObserved') -or
    !$asyncOwnership.Contains('xrtFutureWatchRemove') -or !$asyncOwnership.Contains('frame->cancelling=true')) {
    throw 'Cancellation must be a resident once-only observation and real Watch/queue handoff, not generated frame inference'
}
if (!(Read-Utf8 'src/api/xl_api_symbols.c').Contains('"xl6ActivationCaptureRuntimeV1Get", (const void*)xl6ActivationCaptureRuntimeV1Get') -or
    !(Read-Utf8 'test/cases/xlir_one_shot_tcc_host.c').Contains('BIND(xl6ActivationCaptureRuntimeV1Get)')) {
    throw 'Sparse activation runtime must be linked into both the production and manual source-TCC hosts'
}
if (!$frameTrace.Contains('xl6IrCgenCaptureKind(frame->destroyAbi)') -or !$frameTrace.Contains('if (!kind) return 0;') -or
    !$source.Contains('frame->traceAbi != xl6IrContinuationFrameTraceForDestroy') -or
    !(Read-Utf8 'src/runtime/xl6_capture.c').Contains('slot->kind > XL6_CAPTURE_GENERATOR')) {
    throw 'Unknown owning layouts must be rejected by verified IR, emitter and resident descriptor validation'
}
foreach ($emitter in @('src/cgen/xl6_ir_cgen_async_machine.c','src/cgen/xl6_ir_cgen_generator.c')) {
    $activationEmitter = Read-Utf8 $emitter
    $activationOwner = if ($emitter -eq 'src/cgen/xl6_ir_cgen_generator.c') { $generatorStorage } else { Read-Utf8 'src/runtime/xl6_continuation_frame.c' }
    if (!$activationEmitter.Contains('xl6IrCgenEmitActivationLayout') -or
        !$activationOwner.Contains('xl6ActivationCaptureRuntimeV1Get()->Trace') -or
        !$activationOwner.Contains('xl6ActivationCaptureRuntimeV1Get()->Cleanup') -or
        $activationEmitter.Contains('local < machine->destroyActionCount')) {
        throw 'Continuation families must serialize the exact layout and share resident whole-frame tracing/cleanup'
    }
}
foreach ($emitter in @('src/cgen/xl6_ir_cgen_instruction_emit.c','src/cgen/xl6_ir_cgen_async_instruction.c','src/cgen/xl6_ir_cgen_generator.c')) {
    $consumer = Read-Utf8 $emitter
    if (!$consumer.Contains('xl6IrCgenEmitGeneratorConsumerInstruction') -or !$consumer.Contains('xl6IrCgenEmitGeneratorNextTest')) {
        throw 'Generator consumers must share item ownership and independent next outcome emission'
    }
}
# Nullable language null and runtime failure use separate physical outcomes.
foreach ($required in @('XL6_IR_CALL_ERROR_PHYSICAL_STATUS_OUTPUT',
    'forwardErrorBoundaryAbi', 'reverseErrorBoundaryAbi')) {
    if (!$header.Contains($required) -or !$source.Contains($required)) { throw "Checked projection IR contract missing: $required" }
}
if (!(Read-Utf8 'src/cgen/xl6_ir_cgen_type_call.c').Contains('projection_ok_') -or
    !(Read-Utf8 'src/cgen/xl6_ir_cgen_callable_adapters.c').Contains('xl6ForeignCallBegin(&invocation)')) {
    throw 'Checked projection and reverse callable invocation must consume explicit isolated outcomes'
}

# Module globals and code capabilities belong to the same physical graph.
# Owned finalizers keep their lease through the separate context-release tail.
$finalizerRuntime = Read-Utf8 'src/runtime/xl6_code_finalizer.c'
$nativeWrapper = [regex]::Match((Read-Utf8 'src/api/xl_api.c'),
    '(?s)static int xlBuildCSourceWrapperText\(.*?\n\}').Value
if (!$nativeWrapper.Contains('#include <__xl_runtime/xruntime_decl.h>') -or $nativeWrapper.Contains('#include \"xrt.h\"')) {
    throw 'Native sidecars must start from the canonical declaration ABI, not preselected implementation guards'
}
foreach ($required in @('xrtValueObjectFinalizerBindOwned','code_finalizer_trace','code_finalizer_invoke',
    'code_finalizer_release','xl6CodeLeaseRetain(parent)','xl6CodeLeaseRelease(lease)')) {
    if (!$finalizerRuntime.Contains($required)) { throw "Resident finalizer contract missing: $required" }
}
if ($finalizerRuntime -match 'XL6_AstNode|XL6_Hir|xl6Cgen') { throw 'Resident finalization must not recover frontend semantics' }
if (!(Read-Utf8 'src/api/xl_api_symbols.c').Contains('"xl6CodeFinalizerBind"') -or
    !(Read-Utf8 'tools/check_xlir_code_finalizer.ps1').Contains('xlir_class_finalizer_cursor.xl')) {
    throw 'Resident finalizer API and real language regression must stay wired'
}
if (!$header.Contains('moduleTraceAbi') -or !$source.Contains('xl6IrModuleStorageTraceFor') -or
    !(Read-Utf8 'src/ir/xl6_ir_lower.c').Contains('xl6IrBindModuleStorageTrace')) {
    throw 'Module global traces must be lowered and independently verified in XLIR'
}
$moduleTrace = Read-Utf8 'src/cgen/xl6_ir_cgen_module_trace.c'
if (!$moduleTrace.Contains('moduleTraceAbi') -or !$moduleTrace.Contains('xl6IrCgenOwnershipView') -or
    !$moduleTrace.Contains('explicit opaque global storage') -or
    !$moduleTrace.Contains('xl6NetworkServiceRuntimeV2Get()->Trace') -or
    !(Read-Utf8 'src/api/xl_api.c').Contains('xrtNetEngineOwnershipAdapterV1') -or
    !(Read-Utf8 'src/api/xl_api.c').Contains('xrtNetResolverOwnershipAdapterV1')) {
    throw 'Module tracing must consume explicit physical slots and refuse unknown owners'
}
foreach ($file in @('src/runtime/xl6_code_lease.c','src/runtime/xl6_continuation_queue.c','src/cgen/xl6_ir_cgen_code_owner.c')) {
    if (!(Read-Utf8 $file).Contains('xl6CodeOwnerGraphOps')) { throw "Code-owner graph capability is not propagated: $file" }
}
if (!(Read-Utf8 'src/api/xl_api.c').Contains('module->ownershipTrace == NULL') -or
    !(Read-Utf8 'src/runtime/xl6_continuation_queue.c').Contains('successor->codeLease = predecessor->codeLease')) {
    throw 'Missing module descriptors must refuse inspection and successor ownership must transfer once'
}
$queueRuntime = Read-Utf8 'src/runtime/xl6_continuation_queue.c'
if ($queueRuntime -match 'codeContext|releaseCode|codeOwnershipOps' -or
    !$queueRuntime.Contains('xl6CodeLeaseAcquire(owner, &lease)') -or
    !$queueRuntime.Contains('visit(xl6CodeLeaseOwnership(ticket->codeLease), context)')) {
    throw 'A ticket must own the actual shared lease node; new roots must not inherit closed admission'
}
$runOne = [regex]::Match($queueRuntime, '(?s)static bool run_one\(.*?\n\}').Value
$enter = $runOne.IndexOf('xl6CodeScopeEnter')
$run = $runOne.IndexOf('run(data)')
$release = $runOne.IndexOf('release(data)')
$leave = $runOne.IndexOf('xl6CodeScopeLeave')
$publish = $runOne.IndexOf('publish_ticket')
if ($enter -lt 0 -or $run -le $enter -or $release -le $run -or $leave -le $release -or $publish -le $leave) {
    throw 'Continuation callbacks must return inside a resident scope before successor publication and final queue retirement'
}
$detach=$runOne.IndexOf('ticket->queue = NULL;')
$endDetach=$runOne.IndexOf('xrtOwnershipScopeEnd(&retirement)',[Math]::Max(0,$detach))
$beginPublish=$runOne.IndexOf('xrtOwnershipMutationBegin(&retirement)',[Math]::Max(0,$leave))
if ($detach -le $run -or $endDetach -le $detach -or $endDetach -ge $release -or
    $beginPublish -le $leave -or $beginPublish -ge $publish) {
    throw 'Async release callbacks must run between frame detach and credit publication admissions, retaining real code and queue roots'
}
