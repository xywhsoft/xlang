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
    const XL6_IrType* payload = xl6IrTypeGet(&module->types, instruction->payloadType);
    const char* absent = payload && payload->kind == XL6_IR_TYPE_NULLABLE
        ? "xrtValueRetain(xrtValueNull())" : "NULL";
    switch ( instruction->asyncResultAbi ) {
    case XL6_IR_ASYNC_RESULT_COPY_TAGGED:
        {
            XL6_IrCAbi abi = xl6IrNullableScalarElementAbi(module, instruction->payloadType);
            const XL6_IrType* element = payload && payload->params
                ? xl6IrTypeGet(&module->types, payload->params[0]) : NULL;
            const char* type = xl6IrCgenType(module, instruction->payloadType);
            if (!element || !type || abi == XL6_IR_C_ABI_UNSUPPORTED)
                return xl6IrCgenDiag(diags, module, instruction->origin,
                    "XLIR task.all optional scalar has no exact carrier ABI");
            xl6OutputPrintf(out,
                "    const %s* value = (const %s*)payload;\n"
                "    if (!value->hasValue) return xrtValueRetain(xrtValueNull());\n", type, type);
            XL6_NativeScalarProfileV1 profile = xl6IrNativeScalarProfileForCAbi(abi);
            if (profile != XL6_NATIVE_SCALAR_NONE) {
                xl6OutputPrintf(out,
                    "    return xl6ValueBoxNativeScalarV1(&value->value, %u, 0x%016llXULL);\n",
                    (unsigned)profile, (unsigned long long)element->typeValueId);
            } else {
                const char* boxer = abi == XL6_IR_C_ABI_BOOL ? "xrtValueBool"
                    : abi == XL6_IR_C_ABI_I64 ? "xrtValueInt"
                    : abi == XL6_IR_C_ABI_U64 || abi == XL6_IR_C_ABI_TYPE_ID ? "xrtValueUInt"
                    : abi == XL6_IR_C_ABI_CHAR_U32 ? "xrtValueChar"
                    : abi == XL6_IR_C_ABI_TIME_I64 ? "xrtValueTime" : "xrtValueFloat";
                xl6OutputPrintf(out, "    return %s%s(value->value)%s;\n",
                    abi == XL6_IR_C_ABI_BOOL ? "xrtValueRetain(" : "", boxer,
                    abi == XL6_IR_C_ABI_BOOL ? ")" : "");
            }
        }
        break;
    case XL6_IR_ASYNC_RESULT_COPY_C_SCALAR:
        {
            const XL6_IrType* type = xl6IrTypeGet(&module->types, instruction->payloadType);
            XL6_NativeScalarProfileV1 profile = type
                ? xl6IrNativeScalarProfileForCAbi(type->cAbi) : XL6_NATIVE_SCALAR_NONE;
            if (!type || !type->typeValueId || profile == XL6_NATIVE_SCALAR_NONE)
                return xl6IrCgenDiag(diags, module, instruction->origin,
                    "XLIR task.all native scalar has no exact profile and identity");
            xl6OutputPrintf(out,
                "    return xl6ValueBoxNativeScalarV1(payload, %u, 0x%016llXULL);\n",
                (unsigned)profile, (unsigned long long)type->typeValueId);
        }
        break;
    case XL6_IR_ASYNC_RESULT_COPY_BOOL:
        xl6OutputPuts(
            "    return xrtValueRetain(xrtValueBool(*(const bool*)payload));\n", out);
        break;
    case XL6_IR_ASYNC_RESULT_COPY_CHAR:
        xl6OutputPuts(
            "    return xrtValueChar(*(const uint32_t*)payload);\n", out);
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
    case XL6_IR_ASYNC_RESULT_COPY_F32:
        xl6OutputPuts(
            "    return xrtValueFloat(*(const float*)payload);\n", out);
        break;
    case XL6_IR_ASYNC_RESULT_COPY_TIME:
        xl6OutputPuts(
            "    return xrtValueTime(*(const int64_t*)payload);\n", out);
        break;
    case XL6_IR_ASYNC_RESULT_OWNED_STRING:
        xl6OutputPrintf(out,
            "    return ((const xlir_string*)payload)->Data != NULL ? xrtValueString(*(const xlir_string*)payload) : %s;\n",
            absent);
        break;
    case XL6_IR_ASYNC_RESULT_RETAINED_XVALUE:
        xl6OutputPuts(
            "    return *(xvalue* const*)payload != NULL ? xrtValueRetain(*(xvalue* const*)payload) : NULL;\n",
            out);
        break;
    case XL6_IR_ASYNC_RESULT_RETAINED_CLOSURE:
        {
            XL6_IrTypeId signature = xl6IrRuntimeCallableFieldSourceSignature(module, instruction->payloadType);
            const XL6_IrType* callable = xl6IrTypeGet(&module->types, signature);
            if (!callable) return xl6IrCgenDiag(diags, module, instruction->origin,
                "XLIR task.all closure has no underlying signature");
            xl6OutputPrintf(out,
                "    return *(xlir_closure* const*)payload != NULL ? xlir_closure_to_xvalue_scalar(*(xlir_closure* const*)payload, xlir_xcall_entry_t%u, 0x%016llXULL) : %s;\n",
                (unsigned)signature, (unsigned long long)callable->typeValueId, absent);
        }
        break;
    case XL6_IR_ASYNC_RESULT_RETAINED_ERROR:
        xl6OutputPrintf(out,
            "    return xl6ErrorValueRefV1(*(xerror* const*)payload, 0x%016llXULL);\n",
            (unsigned long long)xl6IrTypeGet(
                &module->types, instruction->payloadType)->typeValueId);
        break;
    case XL6_IR_ASYNC_RESULT_RETAINED_FUTURE:
        xl6OutputPrintf(out,
            "    return *(xfuture* const*)payload != NULL ? xrtValueFuture(*(xfuture* const*)payload) : %s;\n",
            absent);
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
    static const XL6_IrCgenAsyncPayloadContract native = {"xl6NativeFuturePayloadRuntimeV1Get()->Drop", NULL};
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
    case XL6_IR_ASYNC_RESULT_OWNED_NATIVE_VALUE:
        return &native;
    case XL6_IR_ASYNC_RESULT_COPY_BOOL:
    case XL6_IR_ASYNC_RESULT_COPY_CHAR:
    case XL6_IR_ASYNC_RESULT_COPY_I64:
    case XL6_IR_ASYNC_RESULT_COPY_U64:
    case XL6_IR_ASYNC_RESULT_COPY_TYPE_ID:
    case XL6_IR_ASYNC_RESULT_COPY_F64:
    case XL6_IR_ASYNC_RESULT_COPY_F32:
    case XL6_IR_ASYNC_RESULT_COPY_TIME:
    case XL6_IR_ASYNC_RESULT_COPY_TAGGED:
    case XL6_IR_ASYNC_RESULT_COPY_RAW_POINTER:
    case XL6_IR_ASYNC_RESULT_COPY_C_SCALAR:
    case XL6_IR_ASYNC_RESULT_COPY_UNION:
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
    if(abi==XL6_IR_ASYNC_RESULT_OWNED_NATIVE_VALUE)
        xl6OutputPrintf(out,"xl6NativeFuturePayloadRuntimeV1Get()->ResolveOwned(%s, %s)",promise,value);
    else xl6OutputPrintf(out, "xl6PayloadResolveOwned(%s, %s, %s)", promise, value, contract->residentKind);
    return !xl6OutputHasFailed(out);
}

static const char* xl6IrCgenAsyncPayloadKind(XL6_IrAsyncResultAbi abi)
{
    /* Native void returns no owned slot; use the closed trivial policy so
     * submission remains certified and still rejects non-void mismatches. */
    if (abi == XL6_IR_ASYNC_RESULT_VOID) return "XL6_FUTURE_PAYLOAD_VALUE";
    const XL6_IrCgenAsyncPayloadContract* contract = xl6IrCgenAsyncPayloadContract(abi);
    return contract != NULL && contract->residentKind != NULL ? contract->residentKind : "XL6_FUTURE_PAYLOAD_INVALID";
}

/* One result ABI selects allocation, resident Drop and publication together.
 * Native payloads preserve their direct typed value address; the resident
 * prefix never becomes an erased xvalue or a generated destructor/context. */
static int xl6IrCgenEmitAsyncPayloadAllocation(XL6_Output* out,const XL6_IrModule* module,
    XL6_IrTypeId id,XL6_IrAsyncResultAbi abi,const char* destination)
{
    const char* type=xl6IrCgenType(module,id);if(!type)return 0;
    if(abi==XL6_IR_ASYNC_RESULT_OWNED_NATIVE_VALUE) {
        const XL6_IrNativeValuePlan* native=xl6IrNativeValuePlanForType(xl6IrTypeGet(&module->types,id));
        if(!native)return 0;
        xl6OutputPrintf(out,"    %s = (%s*)xl6NativeFuturePayloadRuntimeV1Get()->Allocate(%s());\n",
            destination,type,native->captureProfileSymbol);
    } else xl6OutputPrintf(out,"    %s = (%s*)xrtMalloc(sizeof(*%s));\n",destination,type,destination);
    return !xl6OutputHasFailed(out);
}

static void xl6IrCgenEmitAsyncTaskSubmission(XL6_Output* out,XL6_IrAsyncResultAbi abi)
{
    if(abi==XL6_IR_ASYNC_RESULT_OWNED_NATIVE_VALUE)
        xl6OutputPuts("    future = xl6TaskSubmitOwnedPolicyV1(xlir_async_pool, frame, NULL, xl6NativeFuturePayloadRuntimeV1Get()->Policy(), false);\n",out);
    else xl6OutputPrintf(out,"    future = xlir_async_pool_submit(frame, NULL, %s);\n",
        xl6IrCgenAsyncPayloadKind(abi));
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
    case XL6_IR_ASYNC_FRAME_TRANSFER_COPY_STRING: return "xlir_string_copy";
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
    case XL6_IR_ASYNC_FRAME_DESTROY_FREE_STRING: return "xlir_string_drop";
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
static int xl6IrCgenEmitActivationCapture(XL6_Output*,const XL6_IrModule*,XL6_IrTypeId,
    XL6_IrAsyncFrameTransferAbi,const char*,const char*,const char*,const char*);
static int xl6IrCgenEmitCallbackFrame(XL6_Output* out, const XL6_IrModule* module,
    const XL6_IrCallbackBoundary* boundary, const char* prefix, size_t index)
{
    size_t i;
    if (!boundary) return 0;
    int task = boundary->storageAbi == XL6_IR_CALLBACK_STORAGE_RESIDENT_LAYOUT_TASK_FRAME_V3;
    int queued = boundary->storageAbi == XL6_IR_CALLBACK_STORAGE_RESIDENT_QUEUED_FUTURE_FRAME_V1;
    if ((!task && !queued) || boundary->cleanupAbi != (task
        ? XL6_IR_CALLBACK_CLEANUP_TRANSACTIONAL : XL6_IR_CALLBACK_CLEANUP_COMPLETE_BEFORE_PUBLISH_V1)) return 0;
    xl6OutputPrintf(out, "typedef struct %s_frame_%lu {\n", prefix, (unsigned long)index);
    for (i = 0; i < boundary->captureCount; ++i) {
        const XL6_IrCallbackCapture* c = &module->callbackCaptures[boundary->firstCapture + i];
        const char* type = xl6IrCgenType(module, c->type); char field[64];
        if (!type) return 0;
        xl6IrCgenCallbackField(field, sizeof(field), c, 0);
        xl6OutputPrintf(out, "    %s %s;\n", type, field);
    }
    xl6OutputPrintf(out, "} %s_frame_%lu;\nstatic const XL6_CaptureSlotV2 %s_slots_%lu[] = {\n",
        prefix, (unsigned long)index, prefix, (unsigned long)index);
    for (i = 0; i < boundary->captureCount; ++i) {
        const XL6_IrCallbackCapture* c = &module->callbackCaptures[boundary->firstCapture + i];
        char field[64],frameType[96];
        xl6IrCgenCallbackField(field, sizeof(field), c, 0);
        snprintf(frameType,sizeof(frameType),"%s_frame_%lu",prefix,(unsigned long)index);
        if(!xl6IrCgenEmitCaptureSlotV2(out,module,c->type,c->destroyAbi,frameType,field))return 0;
    }
    xl6OutputPrintf(out,
        "};\nstatic const XL6_CaptureLayoutV2 %s_layout_%lu = {\n"
        "    sizeof(XL6_CaptureLayoutV2), sizeof(%s_frame_%lu), %lu, %s_slots_%lu\n};\n"
        "#define %s_frame_cleanup_%lu(%s) (%s()->Cleanup)(%s)\n",
        prefix, (unsigned long)index, prefix, (unsigned long)index,
        (unsigned long)boundary->captureCount, prefix, (unsigned long)index,
        prefix, (unsigned long)index,
        boundary->storageAbi == XL6_IR_CALLBACK_STORAGE_RESIDENT_LAYOUT_TASK_FRAME_V3 ? "frame" : "frame, outcome",
        boundary->storageAbi == XL6_IR_CALLBACK_STORAGE_RESIDENT_LAYOUT_TASK_FRAME_V3
            ? "xl6TaskRuntimeV3Get" : "xl6FutureCallbackRuntimeV5Get",
        boundary->storageAbi == XL6_IR_CALLBACK_STORAGE_RESIDENT_LAYOUT_TASK_FRAME_V3 ? "frame" : "frame, outcome");
    xl6OutputPrintf(out, "static void %s_frame_free_%lu(void* data, void* user)\n{ ", prefix, (unsigned long)index);
    if (boundary->storageAbi == XL6_IR_CALLBACK_STORAGE_RESIDENT_LAYOUT_TASK_FRAME_V3)
        xl6OutputPuts("(void)user; xlir_task_discard(data);",out);
    else if (boundary->storageAbi == XL6_IR_CALLBACK_STORAGE_RESIDENT_QUEUED_FUTURE_FRAME_V1)
        xl6OutputPuts("(void)user; xlir_future_callback_discard(data);",out);
    else return 0;
    xl6OutputPuts(" }\n",out);
    return !xl6OutputHasFailed(out);
}
static int xl6IrCgenEmitCallbackCapture(XL6_Output* out, const XL6_IrModule* module,
    const XL6_IrCallbackBoundary* boundary, const char* prefix, size_t index)
{
    size_t i;
    if (boundary->storageAbi == XL6_IR_CALLBACK_STORAGE_RESIDENT_LAYOUT_TASK_FRAME_V3)
        xl6OutputPrintf(out,
            "    frame = (%s_frame_%lu*)(xl6TaskRuntimeV3Get()->Allocate)(xlir_task_owner, sizeof(*frame), %s_task_%lu, &%s_layout_%lu);\n",
            prefix, (unsigned long)index, prefix, (unsigned long)index, prefix, (unsigned long)index);
    else if (boundary->storageAbi == XL6_IR_CALLBACK_STORAGE_RESIDENT_QUEUED_FUTURE_FRAME_V1)
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
        if(c->transferAbi==XL6_IR_ASYNC_FRAME_TRANSFER_COPY_NATIVE_VALUE) {
            char place[96],rollback[160];snprintf(place,sizeof(place),"frame->%s",field);
            snprintf(rollback,sizeof(rollback),"%s_frame_free_%lu(frame, NULL);",prefix,(unsigned long)index);
            if(!xl6IrCgenEmitActivationCapture(out,module,c->type,c->transferAbi,place,input,NULL,rollback))return 0;
            continue;
        }
        transfer = xl6IrCgenCallbackTransfer(c->transferAbi);
        if (!transfer) return 0;
        if (c->transferAbi == XL6_IR_ASYNC_FRAME_TRANSFER_COPY_STRING) {
            xl6OutputPrintf(out,
                "    frame->%s = xlir_string_copy(%s);\n"
                "    if (%s.Data != NULL && frame->%s.Data == NULL) {\n"
                "        if (!xrtGetError()) xrtSetErrorTake(xlir_error_create(\"MemoryError\", \"callback capture failed\"));\n"
                "        %s_frame_free_%lu(frame, NULL); return NULL;\n"
                "    }\n",
                field, input, input, field, prefix, (unsigned long)index);
            continue;
        }
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
        xl6OutputPrintf(out, "    %s invoked_%s = frame->%s; frame->%s = (%s){0};\n",
            type, field, field, field,type);
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
        "    if (frame == NULL || frame->callable == NULL ||\n"
        "        frame->callable->invoke == NULL || result == NULL) {\n"
        "        if (!xrtGetError()) xrtSetErrorTake(xlir_error_create(\"AsyncError\", \"task worker frame is invalid\"));\n"
        "        goto complete;\n"
        "    }\n", out);
    if (!isVoid) {
        if(!xl6IrCgenEmitAsyncPayloadAllocation(out,module,instruction->payloadType,instruction->asyncResultAbi,"value"))return 0;
        xl6OutputPrintf(out,
        "    if (value == NULL) {\n"
        "        if (!xrtGetError()) xrtSetErrorTake(xlir_error_create(\"MemoryError\", \"task result allocation failed\"));\n"
        "        goto complete;\n"
        "    }\n"
        "    *value = (%s){0};\n", payloadType);
    }
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
        "    { xl6_error_scope cleanup = {0}; xl6_error_result outcome;\n"
        "      if (!xl6ErrorScopeBeginCleanup(&cleanup, !ok)) abort();\n"
        "      if (frame) xlir_task_spawn_frame_cleanup_%lu(frame);\n"
        "      outcome = xl6ErrorScopeEnd(&cleanup);\n"
        "      ok = outcome.ok; xrtSetErrorTake(outcome.error); }\n"
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
        "    if (callable == NULL || callable->invoke == NULL) {\n"
        "        if (!xrtGetError()) xrtSetErrorTake(xlir_error_create(\"TaskError\", \"task.spawn requires a callable func value\"));\n"
        "        return NULL;\n"
        "    }\n",
        (unsigned long)instructionIndex);
    if (!xl6IrCgenEmitCallbackCapture(out, module, boundary, "xlir_task_spawn", instructionIndex)) return 0;
    xl6IrCgenEmitAsyncTaskSubmission(out,instruction->asyncResultAbi);
    xl6OutputPrintf(out,
        "    if (future == NULL) {\n"
        "        if (!xrtGetError()) xrtSetErrorTake(xlir_error_create(\"TaskError\", \"task.spawn submission failed\"));\n"
        "        xlir_task_spawn_frame_free_%lu(frame, NULL);\n"
        "    }\n"
        "    return future;\n"
        "}\n\n",
        (unsigned long)instructionIndex);
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
        boundary->transportAbi != XL6_IR_CALLBACK_TRANSPORT_FUTURE_READY_QUEUE_V1 ||
        boundary->cleanupAbi != XL6_IR_CALLBACK_CLEANUP_COMPLETE_BEFORE_PUBLISH_V1 ||
        boundary->errorAbi != XL6_IR_CALLBACK_ERROR_PRESERVE_STATUS_DIAGNOSTIC_V1 ||
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
        if(!xl6IrCgenEmitAsyncPayloadAllocation(out,module,instruction->payloadType,instruction->asyncResultAbi,"value"))return 0;
        xl6OutputPrintf(out,
            "    if (value == NULL) { if (!xrtGetError()) xrtSetErrorTake(xlir_error_create(\"MemoryError\", \"Future result allocation failed\")); ok = false; goto complete; }\n"
            "    *value = (%s){0};\n", outputType);
        if (isThen) xl6OutputPrintf(out,
        "    ok = ((bool (*)(void*, %s*, %s))frame->callable->invoke)(frame->callable->environment, value, *(%s const*)input->Value);\n",
            outputType, sourceType, sourceType);
        else xl6OutputPrintf(out,
            "    ok = ((bool (*)(void*, %s*, xerror*))frame->callable->invoke)(frame->callable->environment, value, (xerror*)input->Error);\n",
            outputType);
    }
    xl6OutputPrintf(out,
        "complete:\n"
        "    terminal = cancelled || (forward && input && input->State != XFUTURE_RESOLVED && ok && !xrtGetError());\n"
        "    if (forward && frame) { source = frame->source; frame->source = NULL; }\n"
        "    xerror* callbackError = xrtTakeError();\n"
        "    xl6_error_result outcome = { ok && !callbackError, callbackError };\n"
        "    if (frame) outcome = xlir_future_continue_frame_cleanup_%lu(frame, outcome);\n"
        "    if (terminal) { xrtErrorFree(outcome.error); outcome = (xl6_error_result){ true, NULL }; }\n"
        "    ok = outcome.ok; xrtSetErrorTake(outcome.error);\n"
        "    if (cancelled) { (void)xrtPromiseCancel(output); }\n"
        "    else if (!ok || xrtGetError() != NULL) {\n"
        "        xerror* primary = xrtTakeError();\n"
        "        if (value) { %s(value, NULL); value = NULL; }\n"
        "        xrtFutureDestroy(source); source = NULL; xrtClearError();\n"
        "        xl6FutureCallbackRuntimeV5Get()->Fail(output, primary);\n"
        "    } else if (forward) {\n"
        "        if (!xrtPromiseForward(output, source)) xl6FutureCallbackRuntimeV5Get()->Fail(output, xrtTakeError());\n"
        "    } else {\n"
        "        if (!",
        (unsigned long)instructionIndex, destroy);
    if (!xl6IrCgenEmitAsyncPayloadPublish(out, instruction->asyncResultAbi, "output", "value")) return 0;
    xl6OutputPrintf(out,
        ") { xerror* primary = xrtTakeError(); %s(value, NULL); xrtClearError(); xl6FutureCallbackRuntimeV5Get()->Fail(output, primary); }\n"
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
        "    future = xl6FutureCallbackRuntimeV5Get()->Start(xlir_continuation_executor, frame, source);\n"
        "    if (future == NULL) xlir_future_continue_frame_free_%lu(frame, NULL);\n"
        "    return future;\n"
        "}\n\n",
        (unsigned long)instructionIndex);
    return !xl6OutputHasFailed(out);
}
