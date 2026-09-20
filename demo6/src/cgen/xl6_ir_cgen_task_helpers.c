static int xl6IrCgenEmitFuturePayloadBoxer(
    XL6_Output* out, const XL6_IrModule* module,
    const XL6_IrInstruction* instruction, XL6_DiagList* diags)
{
    if ( instruction == NULL ||
         !((instruction->runtimeOp == XL6_IR_RUNTIME_FUTURE_ALL_XVALUES &&
            xl6IrCgenRuntimeFutureAggregateAbi(module, instruction)) ||
           (instruction->runtimeOp ==
                XL6_IR_RUNTIME_FUTURE_ALL_XVALUES_ARRAY &&
            xl6IrCgenRuntimeFutureAggregateArrayAbi(module, instruction))) )
        return xl6IrCgenDiag(diags, module,
            instruction != NULL ? instruction->origin : XL6_IR_NODE_INVALID,
            "XLIR C emitter task.all payload boxer ABI is unresolved");
    xl6OutputPrintf(out,
        "static xvalue* xlir_future_payload_box_t%u(const void* payload)\n{\n"
        "    if (payload == NULL) return NULL;\n",
        (unsigned int)instruction->payloadType);
    switch ( instruction->asyncResultAbi ) {
    case XL6_IR_ASYNC_RESULT_COPY_BOOL:
        xl6OutputPuts(
            "    return xrtValueRetain(xrtValueBool(*(const bool*)payload));\n", out);
        break;
    case XL6_IR_ASYNC_RESULT_COPY_I64:
        xl6OutputPuts(
            "    return xrtValueInt(*(const int64_t*)payload);\n", out);
        break;
    case XL6_IR_ASYNC_RESULT_COPY_U64:
        xl6OutputPuts(
            "    return xrtValueUInt(*(const uint64_t*)payload);\n", out);
        break;
    case XL6_IR_ASYNC_RESULT_COPY_TYPE_ID:
        xl6OutputPuts(
            "    return xrtValueUInt(*(const uint64_t*)payload);\n", out);
        break;
    case XL6_IR_ASYNC_RESULT_COPY_F64:
        xl6OutputPuts(
            "    return xrtValueFloat(*(const double*)payload);\n", out);
        break;
    case XL6_IR_ASYNC_RESULT_OWNED_STRING:
        xl6OutputPuts(
            "    return *(char* const*)payload != NULL ? xrtValueString(xrtStrView(*(char* const*)payload)) : NULL;\n",
            out);
        break;
    case XL6_IR_ASYNC_RESULT_RETAINED_XVALUE:
        xl6OutputPuts(
            "    return *(xvalue* const*)payload != NULL ? xrtValueRetain(*(xvalue* const*)payload) : NULL;\n",
            out);
        break;
    case XL6_IR_ASYNC_RESULT_RETAINED_CLOSURE:
        xl6OutputPrintf(out,
            "    return *(xlir_closure* const*)payload != NULL ? xlir_closure_to_xvalue_scalar(*(xlir_closure* const*)payload, xlir_xcall_entry_t%u, 0x%016llXULL) : NULL;\n",
            (unsigned int)instruction->payloadType,
            (unsigned long long)xl6IrTypeGet(
                &module->types, instruction->payloadType)->typeValueId);
        break;
    case XL6_IR_ASYNC_RESULT_RETAINED_FUTURE:
        xl6OutputPuts(
            "    return *(xfuture* const*)payload != NULL ? xrtValueFuture(*(xfuture* const*)payload) : NULL;\n",
            out);
        break;
    default:
        return xl6IrCgenDiag(diags, module, instruction->origin,
            "XLIR C emitter task.all payload representation is unsupported");
    }
    xl6OutputPuts("}\n", out);
    return !xl6OutputHasFailed(out);
}

typedef struct XL6_IrCgenAsyncPayloadContract {
    const char* destroy;
    const char* residentKind;
} XL6_IrCgenAsyncPayloadContract;

static const XL6_IrCgenAsyncPayloadContract* xl6IrCgenAsyncPayloadContract(
    XL6_IrAsyncResultAbi abi)
{
    /* One verified ABI selects both destruction and the matching trace.
     * Contained code-bearing objects still require their own residency. */
    static const XL6_IrCgenAsyncPayloadContract value = {"xl6PayloadDropGet(XL6_FUTURE_PAYLOAD_VALUE)", "XL6_FUTURE_PAYLOAD_VALUE"};
    static const XL6_IrCgenAsyncPayloadContract string = {"xl6PayloadDropGet(XL6_FUTURE_PAYLOAD_STRING)", "XL6_FUTURE_PAYLOAD_STRING"};
    static const XL6_IrCgenAsyncPayloadContract xvalue = {"xl6PayloadDropGet(XL6_FUTURE_PAYLOAD_XVALUE)", "XL6_FUTURE_PAYLOAD_XVALUE"};
    static const XL6_IrCgenAsyncPayloadContract closure = {"xl6PayloadDropGet(XL6_FUTURE_PAYLOAD_CLOSURE)", "XL6_FUTURE_PAYLOAD_CLOSURE"};
    static const XL6_IrCgenAsyncPayloadContract error = {"xl6PayloadDropGet(XL6_FUTURE_PAYLOAD_ERROR)", "XL6_FUTURE_PAYLOAD_ERROR"};
    static const XL6_IrCgenAsyncPayloadContract future = {"xl6PayloadDropGet(XL6_FUTURE_PAYLOAD_FUTURE)", "XL6_FUTURE_PAYLOAD_FUTURE"};
    static const XL6_IrCgenAsyncPayloadContract generator = {"xl6PayloadDropGet(XL6_FUTURE_PAYLOAD_GENERATOR)", "XL6_FUTURE_PAYLOAD_GENERATOR"};
    switch ( abi ) {
    case XL6_IR_ASYNC_RESULT_OWNED_STRING:
        return &string;
    case XL6_IR_ASYNC_RESULT_RETAINED_XVALUE:
        return &xvalue;
    case XL6_IR_ASYNC_RESULT_RETAINED_CLOSURE:
        return &closure;
    case XL6_IR_ASYNC_RESULT_RETAINED_ERROR:
        return &error;
    case XL6_IR_ASYNC_RESULT_RETAINED_FUTURE:
        return &future;
    case XL6_IR_ASYNC_RESULT_RETAINED_GENERATOR:
        return &generator;
    case XL6_IR_ASYNC_RESULT_COPY_BOOL:
    case XL6_IR_ASYNC_RESULT_COPY_I64:
    case XL6_IR_ASYNC_RESULT_COPY_U64:
    case XL6_IR_ASYNC_RESULT_COPY_TYPE_ID:
    case XL6_IR_ASYNC_RESULT_COPY_F64:
    case XL6_IR_ASYNC_RESULT_COPY_TAGGED:
    case XL6_IR_ASYNC_RESULT_COPY_RAW_POINTER:
        return &value;
    default:
        return NULL;
    }
}

static const char* xl6IrCgenAsyncPayloadDestroy(XL6_IrAsyncResultAbi abi)
{
    const XL6_IrCgenAsyncPayloadContract* contract = xl6IrCgenAsyncPayloadContract(abi);
    return contract != NULL ? contract->destroy : NULL;
}

static int xl6IrCgenEmitAsyncPayloadPublish(XL6_Output* out, XL6_IrAsyncResultAbi abi,
    const char* promise, const char* value)
{
    const XL6_IrCgenAsyncPayloadContract* contract = xl6IrCgenAsyncPayloadContract(abi);
    if (contract == NULL) return 0;
    xl6OutputPrintf(out, "xl6PayloadResolveOwned(%s, %s, %s)", promise, value, contract->residentKind);
    return !xl6OutputHasFailed(out);
}

static const char* xl6IrCgenAsyncPayloadKind(XL6_IrAsyncResultAbi abi)
{
    /* Native void returns no owned slot; use the closed trivial policy so
     * submission remains certified and still rejects non-void mismatches. */
    if (abi == XL6_IR_ASYNC_RESULT_VOID) return "XL6_FUTURE_PAYLOAD_VALUE";
    const XL6_IrCgenAsyncPayloadContract* contract = xl6IrCgenAsyncPayloadContract(abi);
    return contract != NULL ? contract->residentKind : "XL6_FUTURE_PAYLOAD_INVALID";
}

/* Callback frames are emitted from the verified activation capture program.
 * Neither acquisition nor release is rediscovered from a C type here. */
static const XL6_IrCallbackBoundary* xl6IrCgenCallbackBoundary(
    const XL6_IrModule* module, const XL6_IrInstruction* instruction)
{
    if (!instruction || instruction->callbackBoundary == 0 ||
        instruction->callbackBoundary > module->callbackBoundaryCount) return NULL;
    return &module->callbackBoundaries[instruction->callbackBoundary - 1];
}
static const char* xl6IrCgenCallbackTransfer(XL6_IrAsyncFrameTransferAbi abi)
{
    switch (abi) {
    case XL6_IR_ASYNC_FRAME_TRANSFER_COPY_STRING: return "xrtStrDup";
    case XL6_IR_ASYNC_FRAME_TRANSFER_RETAIN_XVALUE: return "xrtValueRetain";
    case XL6_IR_ASYNC_FRAME_TRANSFER_RETAIN_FUTURE: return "xrtFutureRef";
    case XL6_IR_ASYNC_FRAME_TRANSFER_RETAIN_CLOSURE: return "xlir_closure_retain";
    case XL6_IR_ASYNC_FRAME_TRANSFER_RETAIN_ERROR: return "xrtErrorRef";
    default: return NULL;
    }
}
/* Callable result/bridge cleanup still consumes an explicit destruction ABI. */
static const char* xl6IrCgenCallbackDestroy(XL6_IrAsyncFrameDestroyAbi abi)
{
    switch (abi) {
    case XL6_IR_ASYNC_FRAME_DESTROY_FREE_STRING: return "xrtFree";
    case XL6_IR_ASYNC_FRAME_DESTROY_RELEASE_XVALUE: return "xrtValueRelease";
    case XL6_IR_ASYNC_FRAME_DESTROY_DESTROY_FUTURE: return "xrtFutureDestroy";
    case XL6_IR_ASYNC_FRAME_DESTROY_RELEASE_CLOSURE: return "xlir_closure_release";
    case XL6_IR_ASYNC_FRAME_DESTROY_FREE_ERROR: return "xrtErrorFree";
    default: return NULL;
    }
}
static void xl6IrCgenCallbackField(char* name, size_t size,
    const XL6_IrCallbackCapture* capture, int input)
{
    if (capture->role == XL6_IR_CALLBACK_CAPTURE_SOURCE) snprintf(name, size, "source");
    else if (capture->role == XL6_IR_CALLBACK_CAPTURE_CALLABLE) snprintf(name, size, "callable");
    else snprintf(name, size, "%c%lu", input ? 'a' : 'p', (unsigned long)(capture->operand - 1));
}
static int xl6IrCgenEmitCallbackFrame(XL6_Output* out, const XL6_IrModule* module,
    const XL6_IrCallbackBoundary* boundary, const char* prefix, size_t index)
{
    size_t i;
    if (!boundary || boundary->cleanupAbi != XL6_IR_CALLBACK_CLEANUP_TRANSACTIONAL) return 0;
    xl6OutputPrintf(out, "typedef struct %s_frame_%lu {\n", prefix, (unsigned long)index);
    for (i = 0; i < boundary->captureCount; ++i) {
        const XL6_IrCallbackCapture* c = &module->callbackCaptures[boundary->firstCapture + i];
        const char* type = xl6IrCgenType(module, c->type); char field[64];
        if (!type) return 0;
        xl6IrCgenCallbackField(field, sizeof(field), c, 0);
        xl6OutputPrintf(out, "    %s %s;\n", type, field);
    }
    xl6OutputPrintf(out, "} %s_frame_%lu;\nstatic const XL6_CaptureSlot %s_slots_%lu[] = {\n",
        prefix, (unsigned long)index, prefix, (unsigned long)index);
    for (i = 0; i < boundary->captureCount; ++i) {
        const XL6_IrCallbackCapture* c = &module->callbackCaptures[boundary->firstCapture + i];
        const char* kind = xl6IrCgenCaptureKind(c->destroyAbi); char field[64];
        if (!kind) return 0;
        xl6IrCgenCallbackField(field, sizeof(field), c, 0);
        xl6OutputPrintf(out, "    {%s, offsetof(%s_frame_%lu, %s), sizeof(((%s_frame_%lu*)0)->%s)},\n",
            kind, prefix, (unsigned long)index, field, prefix, (unsigned long)index, field);
    }
    xl6OutputPrintf(out,
        "};\nstatic const XL6_CaptureLayoutV1 %s_layout_%lu = {\n"
        "    sizeof(XL6_CaptureLayoutV1), sizeof(%s_frame_%lu), %lu, %s_slots_%lu\n};\n"
        "#define %s_frame_cleanup_%lu(frame) (%s()->Cleanup)(frame)\n",
        prefix, (unsigned long)index, prefix, (unsigned long)index,
        (unsigned long)boundary->captureCount, prefix, (unsigned long)index,
        prefix, (unsigned long)index,
        boundary->storageAbi == XL6_IR_CALLBACK_STORAGE_RESIDENT_LAYOUT_TASK_FRAME_V2
            ? "xl6TaskRuntimeV2Get" : "xl6FutureCallbackRuntimeV2Get");
    xl6OutputPrintf(out, "static void %s_frame_free_%lu(void* data, void* user)\n{ ", prefix, (unsigned long)index);
    if (boundary->storageAbi == XL6_IR_CALLBACK_STORAGE_RESIDENT_LAYOUT_TASK_FRAME_V2)
        xl6OutputPuts("(void)user; xlir_task_discard(data);",out);
    else if (boundary->storageAbi == XL6_IR_CALLBACK_STORAGE_RESIDENT_LAYOUT_FUTURE_FRAME_V2)
        xl6OutputPuts("(void)user; xlir_future_callback_discard(data);",out);
    else return 0;
    xl6OutputPuts(" }\n",out);
    return !xl6OutputHasFailed(out);
}
static int xl6IrCgenEmitCallbackCapture(XL6_Output* out, const XL6_IrModule* module,
    const XL6_IrCallbackBoundary* boundary, const char* prefix, size_t index)
{
    size_t i;
    if (boundary->storageAbi == XL6_IR_CALLBACK_STORAGE_RESIDENT_LAYOUT_TASK_FRAME_V2)
        xl6OutputPrintf(out,
            "    frame = (%s_frame_%lu*)(xl6TaskRuntimeV2Get()->Allocate)(xlir_task_owner, sizeof(*frame), %s_task_%lu, &%s_layout_%lu);\n",
            prefix, (unsigned long)index, prefix, (unsigned long)index, prefix, (unsigned long)index);
    else if (boundary->storageAbi == XL6_IR_CALLBACK_STORAGE_RESIDENT_LAYOUT_FUTURE_FRAME_V2)
        xl6OutputPrintf(out,
            "    frame = (%s_frame_%lu*)xlir_future_callback_allocate(sizeof(*frame), %s_proc_%lu, &%s_layout_%lu);\n",
            prefix, (unsigned long)index, prefix, (unsigned long)index, prefix, (unsigned long)index);
    else return 0;
    xl6OutputPrintf(out,
        "    if (frame == NULL) { if (!xrtGetError()) xrtSetErrorTake(xlir_error_create(\"MemoryError\", \"callback frame allocation failed\")); return NULL; }\n"
        "    *frame = (%s_frame_%lu){0};\n",
        prefix, (unsigned long)index);
    for (i = 0; i < boundary->captureCount; ++i) {
        const XL6_IrCallbackCapture* c = &module->callbackCaptures[boundary->firstCapture + i];
        char field[64], input[64]; const char* transfer;
        xl6IrCgenCallbackField(field, sizeof(field), c, 0);
        xl6IrCgenCallbackField(input, sizeof(input), c, 1);
        if (c->transferAbi == XL6_IR_ASYNC_FRAME_TRANSFER_COPY_VALUE) {
            xl6OutputPrintf(out, "    frame->%s = %s;\n", field, input);
            continue;
        }
        transfer = xl6IrCgenCallbackTransfer(c->transferAbi);
        if (!transfer) return 0;
        xl6OutputPrintf(out,
            "    frame->%s = %s != NULL ? %s(%s) : NULL;\n"
            "    if (%s != NULL && frame->%s == NULL) {\n"
            "        if (!xrtGetError()) xrtSetErrorTake(xlir_error_create(\"MemoryError\", \"callback capture failed\"));\n"
            "        %s_frame_free_%lu(frame, NULL); return NULL;\n"
            "    }\n",
            field, input, transfer, input, input, field, prefix, (unsigned long)index);
    }
    return !xl6OutputHasFailed(out);
}

/* Commit only after all pre-invocation failures (including result allocation).
 * Detached owners now belong to the callable, even when it raises or rebinds;
 * fallback frame destruction continues to own every uncommitted capture. */
static int xl6IrCgenEmitCallbackInvocationCommit(
    XL6_Output* out, const XL6_IrModule* module, const XL6_IrCallbackBoundary* boundary)
{
    size_t i;
    for (i = 0; i < boundary->captureCount; ++i) {
        const XL6_IrCallbackCapture* capture = &module->callbackCaptures[boundary->firstCapture + i];
        char field[64]; const char* type;
        if (capture->invokeAbi != XL6_IR_CALLBACK_INVOKE_TAKE) continue;
        type = xl6IrCgenType(module, capture->type);
        if (!type || capture->role != XL6_IR_CALLBACK_CAPTURE_ARGUMENT) return 0;
        xl6IrCgenCallbackField(field, sizeof(field), capture, 0);
        xl6OutputPrintf(out, "    %s invoked_%s = frame->%s; frame->%s = NULL;\n",
            type, field, field, field);
    }
    return !xl6OutputHasFailed(out);
}

static int xl6IrCgenEmitTaskSpawnHelper(
    XL6_Output* out, const XL6_IrModule* module,
    const XL6_IrInstruction* instruction, size_t instructionIndex,
    XL6_DiagList* diags)
{
    const XL6_IrCallbackBoundary* boundary = xl6IrCgenCallbackBoundary(module, instruction);
    const XL6_IrType* signature;
    const char* payloadType = NULL;
    const char* destroy = NULL;
    size_t argument;
    int isVoid = instruction->asyncResultAbi == XL6_IR_ASYNC_RESULT_VOID;
    if (!xl6IrCgenRuntimeTaskSpawnAbi(module, instruction) || !boundary ||
        boundary->transportAbi != XL6_IR_CALLBACK_TRANSPORT_TASK_POOL ||
        boundary->errorAbi != XL6_IR_CALLBACK_ERROR_ASYNC_CAUSE ||
        boundary->forwardAbi != XL6_IR_CALLBACK_FORWARD_NONE ||
        !(signature = xl6IrTypeGet(&module->types, instruction->calleeType)))
        return xl6IrCgenDiag(diags, module, instruction->origin, "XLIR task callback boundary is unresolved");
    if (!isVoid) {
        payloadType = xl6IrCgenType(module, instruction->payloadType);
        destroy = xl6IrCgenAsyncPayloadDestroy(instruction->asyncResultAbi);
        if (!payloadType || !destroy) return 0;
    }
    if (!xl6IrCgenEmitCallbackFrame(out, module, boundary, "xlir_task_spawn", instructionIndex)) return 0;
    xl6OutputPrintf(out,
        "static XL6_TaskOutcome xlir_task_spawn_task_%lu(xcancel* cancel, void* data, XL6_TaskResult* result)\n"
        "{\n"
        "    xlir_task_spawn_frame_%lu* frame = (xlir_task_spawn_frame_%lu*)data;\n"
        "    bool ok = false, cancelled = false;\n",
        (unsigned long)instructionIndex, (unsigned long)instructionIndex, (unsigned long)instructionIndex);
    if (!isVoid) xl6OutputPrintf(out, "    %s* value = NULL;\n", payloadType);
    xl6OutputPuts(
        "    if (frame == NULL || frame->callable == NULL || result == NULL) {\n"
        "        if (!xrtGetError()) xrtSetErrorTake(xlir_error_create(\"AsyncError\", \"task worker frame is invalid\"));\n"
        "        goto complete;\n"
        "    }\n", out);
    if (!isVoid) xl6OutputPrintf(out,
        "    value = (%s*)xrtMalloc(sizeof(*value));\n"
        "    if (value == NULL) {\n"
        "        if (!xrtGetError()) xrtSetErrorTake(xlir_error_create(\"MemoryError\", \"task result allocation failed\"));\n"
        "        goto complete;\n"
        "    }\n"
        "    *value = (%s){0};\n", payloadType, payloadType);
    if (!xl6IrCgenEmitCallbackInvocationCommit(out, module, boundary)) return 0;
    xl6OutputPuts("    ok = ((bool (*)(void*", out);
    if (!isVoid) xl6OutputPrintf(out, ", %s*", payloadType);
    for (argument = 0; argument < signature->paramCount; ++argument)
        xl6OutputPrintf(out, ", %s", xl6IrCgenType(module, signature->params[argument]));
    xl6OutputPuts("))frame->callable->invoke)(frame->callable->environment", out);
    if (!isVoid) xl6OutputPuts(", value", out);
    for (argument = 0; argument < signature->paramCount; ++argument) {
        const XL6_IrCallbackCapture* capture =
            &module->callbackCaptures[boundary->firstCapture + argument + 1];
        xl6OutputPrintf(out, ", %sp%lu",
            capture->invokeAbi == XL6_IR_CALLBACK_INVOKE_TAKE ? "invoked_" : "frame->",
            (unsigned long)argument);
    }
    xl6OutputPuts(
        ");\n"
        "    if (!ok && !xrtGetError()) xrtSetErrorTake(xlir_error_create(\"AsyncError\", \"task callable failed\"));\n"
        "    cancelled = ok && !xrtGetError() && xrtCancelRequested(cancel);\n"
        "complete:\n", out);
    xl6OutputPrintf(out,
        "    if (frame) xlir_task_spawn_frame_cleanup_%lu(frame);\n"
        "    if (!ok || xrtGetError() != NULL || cancelled) {\n",
        (unsigned long)instructionIndex);
    if (!isVoid) xl6OutputPrintf(out,
        "        xerror* primary = xrtTakeError();\n"
        "        if (value) %s(value, NULL);\n"
        "        xrtClearError(); xrtSetErrorTake(primary);\n", destroy);
    xl6OutputPuts(
        "        if (cancelled) { xrtClearError(); return XL6_TASK_CANCELLED; }\n"
        "        xlir_task_spawn_wrap_current_error(\"task callback cleanup failed\"); return XL6_TASK_FAILED;\n"
        "    }\n", out);
    if (isVoid) xl6OutputPuts("    result->Value = NULL; result->Destroy = NULL;\n", out);
    else xl6OutputPrintf(out, "    result->Value = value; result->Destroy = %s;\n", destroy);
    xl6OutputPuts("    result->DestroyData = NULL; return XL6_TASK_SUCCESS;\n}\n", out);
    xl6OutputPrintf(out, "static xfuture* xlir_task_spawn_start_%lu(xlir_closure* callable", (unsigned long)instructionIndex);
    for (argument = 0; argument < signature->paramCount; ++argument)
        xl6OutputPrintf(out, ", %s a%lu", xl6IrCgenType(module, signature->params[argument]), (unsigned long)argument);
    xl6OutputPrintf(out,
        ")\n{\n"
        "    xfuture* future; xlir_task_spawn_frame_%lu* frame;\n"
        "    if (callable == NULL) { if (!xrtGetError()) xrtSetErrorTake(xlir_error_create(\"TaskError\", \"task.spawn callable is null\")); return NULL; }\n",
        (unsigned long)instructionIndex);
    if (!xl6IrCgenEmitCallbackCapture(out, module, boundary, "xlir_task_spawn", instructionIndex)) return 0;
    xl6OutputPrintf(out,
        "    future = xlir_async_pool_submit(frame, NULL, %s);\n"
        "    if (future == NULL) {\n"
        "        if (!xrtGetError()) xrtSetErrorTake(xlir_error_create(\"TaskError\", \"task.spawn submission failed\"));\n"
        "        xlir_task_spawn_frame_free_%lu(frame, NULL);\n"
        "    }\n"
        "    return future;\n"
        "}\n\n",
        xl6IrCgenAsyncPayloadKind(instruction->asyncResultAbi), (unsigned long)instructionIndex);
    return !xl6OutputHasFailed(out);
}

/* A continuation stages its owned result, runs the capture cleanup transaction,
 * then publishes. Pass-through transfers the source Future owner, never a
 * type-dependent payload clone. The runtime finalizer is an abandoned-frame
 * fallback; all executed callback captures have already been detached. */
static int xl6IrCgenEmitFutureContinuationHelper(
    XL6_Output* out, const XL6_IrModule* module,
    const XL6_IrInstruction* instruction, size_t instructionIndex,
    XL6_DiagList* diags)
{
    const XL6_IrCallbackBoundary* boundary = xl6IrCgenCallbackBoundary(module, instruction);
    const char* sourceType = xl6IrCgenType(module, instruction->sourcePayloadType);
    const char* outputType = xl6IrCgenType(module, instruction->payloadType);
    const char* destroy = xl6IrCgenAsyncPayloadDestroy(instruction->asyncResultAbi);
    int isThen = instruction->futureContinuationKind == XL6_IR_FUTURE_CONTINUATION_THEN;
    int isCatch = instruction->futureContinuationKind == XL6_IR_FUTURE_CONTINUATION_CATCH;
    int isFinally = instruction->futureContinuationKind == XL6_IR_FUTURE_CONTINUATION_FINALLY;
    if (!xl6IrCgenRuntimeFutureContinuationAbi(module, instruction) || !boundary ||
        boundary->transportAbi != XL6_IR_CALLBACK_TRANSPORT_FUTURE_WATCH ||
        boundary->errorAbi != XL6_IR_CALLBACK_ERROR_PRESERVE ||
        boundary->forwardAbi != XL6_IR_CALLBACK_FORWARD_SOURCE_OWNER ||
        !sourceType || !outputType || !destroy)
        return xl6IrCgenDiag(diags, module, instruction->origin, "XLIR continuation callback boundary is unresolved");
    if (!xl6IrCgenEmitCallbackFrame(out, module, boundary, "xlir_future_continue", instructionIndex)) return 0;
    xl6OutputPuts("extern bool xrtPromiseForward(xpromise*, xfuture*);\n", out);
    xl6OutputPrintf(out,
        "static void xlir_future_continue_proc_%lu(const xfutureresult* input, xpromise* output, xcancel* cancel, void* data)\n"
        "{\n"
        "    xlir_future_continue_frame_%lu* frame = (xlir_future_continue_frame_%lu*)data;\n"
        "    xerror* saved = xrtTakeError();\n"
        "    xfuture* source = NULL;\n"
        "    bool ok = true, forward = false, terminal = false, cancelled = false;\n"
        "    %s* value = NULL;\n"
        "    if (frame == NULL || frame->callable == NULL || frame->source == NULL || input == NULL || output == NULL) {\n"
        "        xrtSetErrorTake(xlir_error_create(\"AsyncError\", \"Future continuation frame is invalid\")); ok = false; goto complete;\n"
        "    }\n"
        "    if (xrtCancelRequested(cancel)) { cancelled = true; goto complete; }\n",
        (unsigned long)instructionIndex, (unsigned long)instructionIndex, (unsigned long)instructionIndex, outputType);
    if (isThen || isCatch) xl6OutputPrintf(out,
        "    if (input->State != %s) { forward = true; goto complete; }\n",
        isThen ? "XFUTURE_RESOLVED" : "XFUTURE_FAILED");
    if (isFinally) {
        xl6OutputPuts(
            "    ok = ((bool (*)(void*))frame->callable->invoke)(frame->callable->environment);\n"
            "    forward = ok;\n", out);
    } else {
        xl6OutputPuts(
            "    if (input->State == XFUTURE_RESOLVED && input->Value == NULL) {\n"
            "        xrtSetErrorTake(xlir_error_create(\"AsyncError\", \"Future source payload is missing\")); ok = false; goto complete;\n"
            "    }\n", out);
        xl6OutputPrintf(out,
            "    value = (%s*)xrtMalloc(sizeof(*value));\n"
            "    if (value == NULL) { if (!xrtGetError()) xrtSetErrorTake(xlir_error_create(\"MemoryError\", \"Future result allocation failed\")); ok = false; goto complete; }\n"
            "    *value = (%s){0};\n", outputType, outputType);
        if (isThen) xl6OutputPrintf(out,
        "    ok = ((bool (*)(void*, %s*, %s))frame->callable->invoke)(frame->callable->environment, value, *(%s const*)input->Value);\n",
            outputType, sourceType, sourceType);
        else xl6OutputPrintf(out,
            "    ok = ((bool (*)(void*, %s*, xerror*))frame->callable->invoke)(frame->callable->environment, value, (xerror*)input->Error);\n",
            outputType);
    }
    xl6OutputPrintf(out,
        "complete:\n"
        "    if (!ok && !xrtGetError()) xrtSetErrorTake(xlir_error_create(\"AsyncError\", \"Future callback failed\"));\n"
        "    terminal = cancelled || (forward && input && input->State != XFUTURE_RESOLVED && ok && !xrtGetError());\n"
        "    if (forward && frame) { source = frame->source; frame->source = NULL; }\n"
        "    if (frame) xlir_future_continue_frame_cleanup_%lu(frame);\n"
        "    if (terminal) xrtClearError();\n"
        "    if (cancelled) { (void)xrtPromiseCancel(output); }\n"
        "    else if (!ok || xrtGetError() != NULL) {\n"
        "        xerror* primary = xrtTakeError();\n"
        "        if (value) { %s(value, NULL); value = NULL; }\n"
        "        xrtFutureDestroy(source); source = NULL; xrtClearError();\n"
        "        if (primary) (void)xrtPromiseReject(output, primary); else (void)xrtPromiseClose(output);\n"
        "        xrtErrorFree(primary);\n"
        "    } else if (forward) {\n"
        "        (void)xrtPromiseForward(output, source);\n"
        "    } else {\n"
        "        if (!",
        (unsigned long)instructionIndex, destroy);
    if (!xl6IrCgenEmitAsyncPayloadPublish(out, instruction->asyncResultAbi, "output", "value")) return 0;
    xl6OutputPrintf(out,
        ") %s(value, NULL);\n"
        "    }\n"
        "    xrtFutureDestroy(source); xrtClearError(); xrtSetErrorTake(saved);\n"
        "}\n"
        "static xfuture* xlir_future_continue_start_%lu(xfuture* source, xlir_closure* callable)\n"
        "{\n"
        "    xfuture* future = NULL; xlir_future_continue_frame_%lu* frame;\n"
        "    if (source == NULL || callable == NULL) { if (!xrtGetError()) xrtSetErrorTake(xlir_error_create(\"AsyncError\", \"Future continuation source or callable is null\")); return NULL; }\n",
        destroy,
        (unsigned long)instructionIndex, (unsigned long)instructionIndex);
    if (!xl6IrCgenEmitCallbackCapture(out, module, boundary, "xlir_future_continue", instructionIndex)) return 0;
    xl6OutputPrintf(out,
        "    future = xl6FutureCallbackRuntimeV1Get()->Start(frame, source);\n"
        "    if (future == NULL) xlir_future_continue_frame_free_%lu(frame, NULL);\n"
        "    return future;\n"
        "}\n\n",
        (unsigned long)instructionIndex);
    return !xl6OutputHasFailed(out);
}
