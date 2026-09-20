/* XLIR owns the semantic shape of network values and Futures.  The shared
 * network component below only owns XRT transport mechanics; this adapter
 * derives every language object TypeId, field and destructor from verified IR. */
static int xl6IrCgenNetHasBridge(
    const XL6_IrModule* module, XL6_IrRuntimeBridgeKind bridge)
{
    size_t i;
    if ( module == NULL ) return 0;
    for ( i = 0; i < module->callTargetCount; ++i )
        if ( module->callTargets[i].reachability !=
                 XL6_IR_REACHABILITY_UNREACHABLE &&
             module->callTargets[i].runtimeBridge == bridge ) return 1;
    return 0;
}

static int xl6IrCgenNetHasBridgeOperation(
    const XL6_IrModule* module, XL6_IrRuntimeBridgeOperation operation,
    int reachableOnly)
{
    size_t i;
    if ( module == NULL ||
         operation == XL6_IR_RUNTIME_BRIDGE_OPERATION_NONE ) return 0;
    for ( i = 0; i < module->callTargetCount; ++i ) {
        const XL6_IrCallTarget* target = &module->callTargets[i];
        if ( (!reachableOnly || target->reachability !=
                  XL6_IR_REACHABILITY_UNREACHABLE) &&
             target->runtimeBridgeOperation == operation ) return 1;
    }
    return 0;
}

static XL6_IrTypeId xl6IrCgenNetReceiverClassType(
    const XL6_IrModule* module, XL6_IrRuntimeBridgeKind bridge)
{
    XL6_IrTypeId result = XL6_IR_TYPE_INVALID;
    size_t i;
    if ( module == NULL ) return XL6_IR_TYPE_INVALID;
    for ( i = 0; i < module->callTargetCount; ++i ) {
        const XL6_IrCallTarget* target = &module->callTargets[i];
        size_t parameter;
        if ( target->reachability == XL6_IR_REACHABILITY_UNREACHABLE ||
             target->runtimeBridge != bridge ) continue;
        for ( parameter = 0; parameter < target->parameterCount; ++parameter ) {
            const XL6_IrType* type;
            if ( target->parameterPhysicalAbis == NULL ||
                 target->parameterPhysicalAbis[parameter] !=
                     XL6_IR_CALL_PARAMETER_PHYSICAL_NATIVE_CLASS_HANDLE_BORROW )
                continue;
            type = xl6IrTypeGet(&module->types, target->parameterTypes[parameter]);
            if ( type == NULL || type->kind != XL6_IR_TYPE_CLASS )
                return XL6_IR_TYPE_INVALID;
            if ( result != XL6_IR_TYPE_INVALID &&
                 result != target->parameterTypes[parameter] )
                return XL6_IR_TYPE_INVALID;
            result = target->parameterTypes[parameter];
        }
    }
    return result;
}

static XL6_IrTypeId xl6IrCgenNetFutureClassPayload(
    const XL6_IrModule* module, XL6_IrRuntimeBridgeKind bridge)
{
    XL6_IrTypeId result = XL6_IR_TYPE_INVALID;
    size_t i;
    if ( module == NULL ) return XL6_IR_TYPE_INVALID;
    for ( i = 0; i < module->callTargetCount; ++i ) {
        const XL6_IrCallTarget* target = &module->callTargets[i];
        const XL6_IrType* future;
        const XL6_IrType* payload;
        if ( target->reachability == XL6_IR_REACHABILITY_UNREACHABLE ||
             target->runtimeBridge != bridge ||
             target->returnType == XL6_IR_TYPE_INVALID ) continue;
        future = xl6IrTypeGet(&module->types, target->returnType);
        if ( future == NULL || future->cAbi != XL6_IR_C_ABI_RUNTIME_FUTURE_PTR ||
             future->paramCount != 1 || future->params == NULL ) continue;
        payload = xl6IrTypeGet(&module->types, future->params[0]);
        if ( payload == NULL || payload->kind != XL6_IR_TYPE_CLASS ||
             payload->cAbi != XL6_IR_C_ABI_RUNTIME_XVALUE_PTR ) continue;
        if ( result != XL6_IR_TYPE_INVALID && result != future->params[0] )
            return XL6_IR_TYPE_INVALID;
        result = future->params[0];
    }
    return result;
}

static XL6_IrTypeId xl6IrCgenNetDeclaredFutureClassPayload(
    const XL6_IrModule* module, XL6_IrRuntimeBridgeOperation operation)
{
    XL6_IrTypeId result = XL6_IR_TYPE_INVALID;
    size_t i;
    if ( module == NULL ||
         operation == XL6_IR_RUNTIME_BRIDGE_OPERATION_NONE )
        return XL6_IR_TYPE_INVALID;
    for ( i = 0; i < module->callTargetCount; ++i ) {
        const XL6_IrCallTarget* target = &module->callTargets[i];
        const XL6_IrType* future;
        const XL6_IrType* payload;
        if ( target->runtimeBridgeOperation != operation ||
             target->returnType == XL6_IR_TYPE_INVALID ) continue;
        future = xl6IrTypeGet(&module->types, target->returnType);
        if ( future == NULL || future->cAbi != XL6_IR_C_ABI_RUNTIME_FUTURE_PTR ||
             future->paramCount != 1 || future->params == NULL ) continue;
        payload = xl6IrTypeGet(&module->types, future->params[0]);
        if ( payload == NULL || payload->kind != XL6_IR_TYPE_CLASS ||
             payload->cAbi != XL6_IR_C_ABI_RUNTIME_XVALUE_PTR ) continue;
        if ( result != XL6_IR_TYPE_INVALID && result != future->params[0] )
            return XL6_IR_TYPE_INVALID;
        result = future->params[0];
    }
    return result;
}

static const XL6_IrClassField* xl6IrCgenNetNativeClassField(
    const XL6_IrModule* module, XL6_IrTypeId type)
{
    const XL6_IrClassLayout* layout = xl6IrCgenClassLayout(module, type);
    const XL6_IrClassField* field;
    const XL6_IrType* classType = xl6IrTypeGet(&module->types, type);
    const XL6_IrType* fieldType;
    const XL6_IrCallTarget* destroy;
    if ( layout == NULL || classType == NULL ||
         classType->kind != XL6_IR_TYPE_CLASS ||
         classType->cAbi != XL6_IR_C_ABI_RUNTIME_XVALUE_PTR ||
         classType->ownershipAbi != XL6_IR_OWNERSHIP_ABI_MANAGED ||
         classType->typeValueId == 0 || layout->runtimeTypeTag == 0 ||
         layout->fieldCount != 1 || layout->firstField >= module->classFieldCount ||
         layout->hashAbi != XL6_IR_CLASS_HASH_ABI_RUNTIME_REFERENCE ||
         layout->equalityAbi != XL6_IR_CLASS_EQUALITY_ABI_RUNTIME_REFERENCE )
        return NULL;
    field = &module->classFields[layout->firstField];
    fieldType = xl6IrTypeGet(&module->types, field->type);
    destroy = xl6IrCallTargetGet(module, field->nativeDestroySymbol);
    if ( field->ownerType != type || field->name == NULL ||
         field->name[0] == '\0' || fieldType == NULL ||
         fieldType->cAbi != XL6_IR_C_ABI_RAW_POINTER ||
         field->nativeDestroySymbol == 0 || destroy == NULL ||
         !xl6IrCgenNativeDestructorAbi(destroy) ||
         destroy->returnType != XL6_IR_TYPE_INVALID ||
         destroy->parameterCount != 1 || destroy->parameterTypes == NULL ||
         destroy->parameterTypes[0] != type ||
         destroy->parameterAbis == NULL ||
         destroy->parameterAbis[0] != XL6_IR_PARAMETER_ABI_BORROW ||
         destroy->parameterPhysicalAbis == NULL ||
         destroy->parameterPhysicalAbis[0] !=
             XL6_IR_CALL_PARAMETER_PHYSICAL_NATIVE_CLASS_HANDLE_BORROW )
        return NULL;
    return field;
}

/* Projection is paired with the verified owning layout, never inferred from
 * the runtime value's shape. It borrows; the shared transport owns ref/take. */
static void xl6IrCgenEmitNetClassHandleProjection(
    XL6_Output* out, const XL6_IrClassField* field, const char* label)
{
    xl6OutputPrintf(out,
        "static void* xlir_net_%s_borrow_handle(const xvalue* value)\n"
        "{ void* handle = NULL; xvalue* field; if (value == NULL) return NULL; "
        "field = xrtValueObjectGet(value, xrtStrView(", label);
    xl6IrCgenEmitCStringLiteral(out, field->name);
    xl6OutputPuts(")); return field != NULL && xrtValueGetHandle(field, &handle, NULL, NULL) ? handle : NULL; }\n", out);
}

static int xl6IrCgenEmitNetClassWrapper(
    XL6_Output* out, const XL6_IrModule* module, XL6_IrTypeId type,
    const char* label, const char* rawType,
    const char* refProc, XL6_DiagList* diags)
{
    const XL6_IrType* classType = xl6IrTypeGet(&module->types, type);
    const XL6_IrClassLayout* layout = xl6IrCgenClassLayout(module, type);
    const XL6_IrClassField* field = xl6IrCgenNetNativeClassField(module, type);
    char callback[80];
    if ( out == NULL || classType == NULL || layout == NULL || field == NULL ||
         module->abi.classStorageAbi != XL6_IR_CLASS_STORAGE_RESIDENT_GRAPH_V2 ||
         label == NULL || rawType == NULL || refProc == NULL )
        return xl6IrCgenDiag(diags, module, XL6_IR_NODE_INVALID,
            "XLIR TCP Future class payload has no exact native owning layout");
    snprintf(callback, sizeof(callback), "xlir_net_%s_handle_drop", label);
    xl6IrCgenEmitNetClassHandleProjection(out, field, label);
    if (!xl6IrCgenEmitNativeDestructorCallback(out,
            xl6IrCallTargetGet(module, field->nativeDestroySymbol), callback)) return 0;
    xl6OutputPrintf(out,
        "static uint64_t xlir_net_%s_hash(const xvalue* value, void* data)\n"
        "{ (void)data; if (value == NULL) abort(); return (uint64_t)(uintptr_t)value; }\n"
        "static bool xlir_net_%s_equal(const xvalue* left, const xvalue* right, void* data)\n"
        "{ (void)data; return left == right; }\n"
        "static const xvaluehandleops xlir_net_%s_handle_ops = "
        "{ NULL, xlir_net_%s_handle_drop, NULL, NULL };\n"
        "static xvalue* xlir_net_%s_wrap_borrowed(void* borrowed)\n"
        "{\n"
        "    void* owned; xvalue* handle; xvalue* object; xvalue* tag;\n"
        "    if (borrowed == NULL) return NULL;\n"
        "    owned = (void*)%s((%s*)borrowed);\n"
        "    if (owned == NULL) return NULL;\n"
        "    handle = xrtValueHandleTake(&owned, &xlir_net_%s_handle_ops, NULL);\n"
        "    if (handle == NULL) { if (owned != NULL) xlir_net_%s_handle_drop(owned, NULL); return NULL; }\n"
        "    object = xl6ClassRuntimeV2Get()->Create(xlir_net_owner, NULL);\n"
        "    if (object == NULL) { xrtValueRelease(handle); return NULL; }\n"
        "    if (!xrtValueTypeIdBind(object, 0x%016llXULL) ||\n"
        "        !xl6ClassRuntimeV2Get()->BindIdentity(object, xlir_net_%s_hash, xlir_net_%s_equal))\n"
        "        { xrtValueRelease(handle); xrtValueRelease(object); return NULL; }\n"
        "    tag = xrtValueInt(INT64_C(%u));\n"
        "    if (tag == NULL || !xrtValueObjectSetNew(object, "
        "xrtStrView(\"$xlir.class.runtime-type\"), tag))\n"
        "        { xrtValueRelease(handle); xrtValueRelease(object); return NULL; }\n"
        "    if (!xrtValueObjectSetNew(object, xrtStrView(",
        label, label, label, label, label,
        refProc, rawType, label, label,
        (unsigned long long)classType->typeValueId, label, label,
        (unsigned int)layout->runtimeTypeTag);
    xl6IrCgenEmitCStringLiteral(out, field->name);
    xl6OutputPuts(
        "), handle)) { xrtValueRelease(object); return NULL; }\n"
        "    return xl6ClassRuntimeV2Get()->CommitTake(object);\n"
        "}\n\n", out);
    return !xl6OutputHasFailed(out);
}

static int xl6IrCgenEmitNetOwnedClassWrapper(
    XL6_Output* out, const XL6_IrModule* module, XL6_IrTypeId type,
    const char* label, XL6_DiagList* diags)
{
    const XL6_IrType* classType = xl6IrTypeGet(&module->types, type);
    const XL6_IrClassLayout* layout = xl6IrCgenClassLayout(module, type);
    const XL6_IrClassField* field = xl6IrCgenNetNativeClassField(module, type);
    const XL6_IrCallTarget* destroy = field != NULL
        ? xl6IrCallTargetGet(module, field->nativeDestroySymbol) : NULL;
    char callback[80];
    if ( out == NULL || classType == NULL || layout == NULL || field == NULL ||
         module->abi.classStorageAbi != XL6_IR_CLASS_STORAGE_RESIDENT_GRAPH_V2 ||
         destroy == NULL || destroy->cName == NULL || label == NULL )
        return xl6IrCgenDiag(diags, module, XL6_IR_NODE_INVALID,
            "XLIR TLS Future class payload has no exact native owning layout");
    snprintf(callback, sizeof(callback), "xlir_net_%s_handle_drop", label);
    if (!xl6IrCgenEmitNativeDestructorCallback(out, destroy, callback)) return 0;
    xl6IrCgenEmitNetClassHandleProjection(out, field, label);
    xl6OutputPrintf(out,
        "static uint64_t xlir_net_%s_hash(const xvalue* value, void* data)\n"
        "{ (void)data; if (value == NULL) abort(); return (uint64_t)(uintptr_t)value; }\n"
        "static bool xlir_net_%s_equal(const xvalue* left, const xvalue* right, void* data)\n"
        "{ (void)data; return left == right; }\n"
        "static const xvaluehandleops xlir_net_%s_handle_ops = "
        "{ NULL, xlir_net_%s_handle_drop, NULL, NULL };\n"
        "static xvalue* xlir_net_%s_wrap_owned(void* owned)\n"
        "{\n"
        "    xvalue* handle; xvalue* object; xvalue* tag;\n"
        "    if (owned == NULL) return NULL;\n"
        "    handle = xrtValueHandleTake(&owned, &xlir_net_%s_handle_ops, NULL);\n"
        "    if (handle == NULL) { if (owned != NULL) xlir_net_%s_handle_drop(owned, NULL); return NULL; }\n"
        "    object = xl6ClassRuntimeV2Get()->Create(xlir_net_owner, NULL);\n"
        "    if (object == NULL) { xrtValueRelease(handle); return NULL; }\n"
        "    if (!xrtValueTypeIdBind(object, 0x%016llXULL) ||\n"
        "        !xl6ClassRuntimeV2Get()->BindIdentity(object, xlir_net_%s_hash, xlir_net_%s_equal))\n"
        "        { xrtValueRelease(handle); xrtValueRelease(object); return NULL; }\n"
        "    tag = xrtValueInt(INT64_C(%u));\n"
        "    if (tag == NULL || !xrtValueObjectSetNew(object, "
        "xrtStrView(\"$xlir.class.runtime-type\"), tag))\n"
        "        { xrtValueRelease(handle); xrtValueRelease(object); return NULL; }\n"
        "    if (!xrtValueObjectSetNew(object, xrtStrView(",
        label, label, label, label, label, label, label,
        (unsigned long long)classType->typeValueId, label, label,
        (unsigned int)layout->runtimeTypeTag);
    xl6IrCgenEmitCStringLiteral(out, field->name);
    xl6OutputPuts(
        "), handle)) { xrtValueRelease(object); return NULL; }\n"
        "    return xl6ClassRuntimeV2Get()->CommitTake(object);\n"
        "}\n\n", out);
    return !xl6OutputHasFailed(out);
}

static void xl6IrCgenEmitNativeFutureMap(XL6_Output* out)
{
    xl6OutputPuts(
        "#include <__xl_runtime/xl6_future_callback.h>\n"
        "#include <__xl_runtime/xl6_future_payload.h>\n"
        "static void xlir_native_future_dispatch(const xfutureresult* input, xpromise* output, xcancel* cancel, xfuturecontinueproc mapper, void* argument)\n"
        "{\n"
        "    if (xrtCancelRequested(cancel)) { (void)xrtPromiseCancel(output); return; }\n"
        "    if (input == NULL) { (void)xrtPromiseClose(output); return; }\n"
        "    switch (input->State) {\n"
        "    case XFUTURE_RESOLVED: mapper(input, output, argument); return;\n"
        "    case XFUTURE_FAILED: if (input->Error != NULL && xrtPromiseReject(output, input->Error)) return; break;\n"
        "    case XFUTURE_CANCELLED: (void)xrtPromiseCancel(output); return;\n"
        "    default: break;\n"
        "    }\n    (void)xrtPromiseClose(output);\n}\n"
        "typedef struct xlir_native_future_map_frame { xfuturecontinueproc mapper; uintptr_t argument; } xlir_native_future_map_frame;\n"
        "static const XL6_CaptureSlot xlir_native_future_map_slots[] = {\n"
        "    {XL6_CAPTURE_TRIVIAL, offsetof(xlir_native_future_map_frame, mapper), sizeof(xfuturecontinueproc)},\n"
        "    {XL6_CAPTURE_TRIVIAL, offsetof(xlir_native_future_map_frame, argument), sizeof(uintptr_t)}\n};\n"
        "static const XL6_CaptureLayoutV1 xlir_native_future_map_layout = {sizeof(XL6_CaptureLayoutV1), sizeof(xlir_native_future_map_frame), 2, xlir_native_future_map_slots};\n"
        "static void xlir_native_future_map_proc(const xfutureresult* input, xpromise* output, xcancel* cancel, void* data)\n"
        "{ xlir_native_future_map_frame* frame = (xlir_native_future_map_frame*)data; xlir_native_future_dispatch(input, output, cancel, frame->mapper, (void*)frame->argument); }\n"
        "static xfuture* xlir_native_future_then(xfuture* source, xfuturecontinueproc mapper, uintptr_t argument)\n"
        "{\n"
        "    xlir_native_future_map_frame* frame; xfuture* result; if (source == NULL || mapper == NULL) return NULL;\n"
        "    frame = (xlir_native_future_map_frame*)xl6FutureCallbackRuntimeV2Get()->Allocate(xlir_net_owner, sizeof(*frame), xlir_native_future_map_proc, &xlir_native_future_map_layout);\n"
        "    if (frame == NULL) return NULL;\n    frame->mapper = mapper; frame->argument = argument;\n"
        "    result = xl6FutureCallbackRuntimeV3Get()->StartExclusive(frame, source);\n"
        "    if (result == NULL) xl6FutureCallbackRuntimeV1Get()->Discard(frame);\n    return result;\n}\n"
        "/* Native owning context is a real owner, never a TRIVIAL capture.\n"
        " * Until its physical native graph is described, Trace refuses it. */\n"
        "typedef struct xlir_native_future_owned_frame { xfuturecontinueproc mapper; void* argument; void (*drop)(void*); } xlir_native_future_owned_frame;\n"
        "static bool xlir_native_future_owned_trace(const void* data, XL6_ModuleOwnershipVisit visit, void* context)\n"
        "{ const xlir_native_future_owned_frame* frame = (const xlir_native_future_owned_frame*)data; (void)visit; (void)context; return frame->argument == NULL; }\n"
        "static void xlir_native_future_owned_drop(void* data, void* unused)\n"
        "{ xlir_native_future_owned_frame* frame = (xlir_native_future_owned_frame*)data; void* argument = frame->argument; (void)unused; frame->argument = NULL; if (argument != NULL) frame->drop(argument); }\n"
        "static void xlir_native_future_owned_proc(const xfutureresult* input, xpromise* output, xcancel* cancel, void* data)\n"
        "{ xlir_native_future_owned_frame* frame = (xlir_native_future_owned_frame*)data; xlir_native_future_dispatch(input, output, cancel, frame->mapper, frame->argument); }\n"
        "static xfuture* xlir_native_future_then_owned(xfuture* source, xfuturecontinueproc mapper, void* argument, void (*drop)(void*))\n"
        "{\n"
        "    xlir_native_future_owned_frame* frame; xfuture* result;\n"
        "    if (source == NULL || mapper == NULL || (argument != NULL && drop == NULL)) { xrtSetErrorKind(XERR_ARGUMENT); return NULL; }\n"
        "    frame = (xlir_native_future_owned_frame*)xl6FutureCallbackRuntimeV1Get()->Allocate(xlir_net_owner, sizeof(*frame), xlir_native_future_owned_proc, xlir_native_future_owned_drop, xlir_native_future_owned_trace);\n"
        "    if (frame == NULL) return NULL;\n    frame->mapper = mapper; frame->argument = argument; frame->drop = drop;\n"
        "    result = xl6FutureCallbackRuntimeV3Get()->StartExclusive(frame, source);\n"
        "    if (result == NULL) { frame->argument = NULL; xl6FutureCallbackRuntimeV1Get()->Discard(frame); } return result;\n}\n"
        "static void xlir_native_future_publish_xvalue(xpromise* output, xvalue* owned)\n"
        "{ xvalue** box = owned != NULL ? (xvalue**)xrtMalloc(sizeof(*box)) : NULL; if (box == NULL) { const xerror* error; xrtValueRelease(owned); error = xrtGetError(); if (error == NULL || !xrtPromiseReject(output, error)) (void)xrtPromiseClose(output); return; } *box = owned; if (!xl6PayloadResolveOwned(output, box, XL6_FUTURE_PAYLOAD_XVALUE)) { xl6PayloadDropGet(XL6_FUTURE_PAYLOAD_XVALUE)(box, NULL); (void)xrtPromiseClose(output); } }\n\n", out);
}

static void xl6IrCgenEmitNetLifecycle(XL6_Output* out, const XL6_IrModule* module)
{
    xl6OutputPuts("#include <__xl_runtime/xl6_code_lease.h>\n", out);
    xl6OutputPuts(module->abi.codeOwnerAbi == XL6_IR_MODULE_CODE_OWNER_BIND_BEFORE_INIT
        ? "#define xlir_net_owner (xlir_code_owner.size != 0 ? &xlir_code_owner : NULL)\n"
        : "#define xlir_net_owner NULL\n", out);
    if (module->abi.nativeFutureMapAbi == XL6_IR_NATIVE_FUTURE_MAP_RESIDENT_EXCLUSIVE_V1)
        xl6IrCgenEmitNativeFutureMap(out);
    xl6OutputPuts(
        "#ifndef XL6_NET_RUNTIME\n"
        "#define XL6_NET_RUNTIME\n"
        "#define XL6_XNET_OK XNET_RESULT_OK\n"
        "#define XL6_XNET_AGAIN XNET_RESULT_AGAIN\n"
        "#define XL6_XNET_CLOSED XNET_RESULT_CLOSED\n"
        "#define XL6_XNET_WAIT_OPEN XNET_STREAM_WAIT_OPEN\n"
        "#define XL6_XNET_WAIT_READ XNET_STREAM_WAIT_READ\n"
        "#define XL6_XNET_WAIT_WRITE XNET_STREAM_WAIT_WRITE\n"
        "#define XL6_XNET_WAIT_DRAIN XNET_STREAM_WAIT_DRAIN\n"
        "#define XL6_XNET_WAIT_CLOSE XNET_STREAM_WAIT_CLOSE\n"
        "#define XL6_XNET_FAMILY_UNSPEC XNET_FAMILY_UNSPEC\n"
        "#define XL6_XNET_FAMILY_IPV4 XNET_FAMILY_IPV4\n"
        "#define XL6_XNET_FAMILY_IPV6 XNET_FAMILY_IPV6\n"
        "#define XL6_DEADLINE_NEVER XRT_DEADLINE_NEVER\n"
        "typedef xbytesview XL6_XrtBytesView;\n"
        "#endif\n\n"
        "#include <__xl_runtime/xl6_network_service.h>\n"
        "static XL6_NetworkServices xlir_network_services;\n"
        "#define xl6_net_engine (xlir_network_services.engine)\n"
        "#define xl6_net_resolver (xlir_network_services.resolver)\n"
        "static xatomic64 xl6_net_next_dial_affinity;\n\n"
        "static uint64_t xl6_net_next_dial_worker(void)\n"
        "{ uint32_t workers; if (xl6_net_engine == NULL) return 0; workers = xrtNetEngineWorkerCount(xl6_net_engine); return workers != 0 ? xrtAtomic64FetchAdd(&xl6_net_next_dial_affinity, 1u, XMEMORY_RELAXED) % workers : 0; }\n"
        "static bool xl6_net_runtime_init(void)\n"
        "{\n"
        "    if (!xl6NetworkServiceRuntimeV1Get()->Initialize(&xlir_network_services)) return false;\n"
        "    xrtAtomic64Init(&xl6_net_next_dial_affinity, 0);\n"
        "    return true;\n"
        "}\n"
        "static XL6_ModuleRetireResult xlir_net_runtime_retire(bool wait)\n"
        "{ return xl6NetworkServiceRuntimeV1Get()->Retire(&xlir_network_services, wait); }\n\n"
        "static uint64_t xl6_net_timeout_us(long long timeoutMs)\n"
        "{ uint64_t value; if (timeoutMs < 0) return UINT64_MAX; value = (uint64_t)timeoutMs; return value > UINT64_MAX / 1000u ? UINT64_MAX : value * 1000u; }\n"
        "static xdeadline xl6_net_deadline(long long timeoutMs)\n"
        "{ uint64_t timeout = xl6_net_timeout_us(timeoutMs); return timeout == UINT64_MAX ? XL6_DEADLINE_NEVER : xrtDeadlineAfter(timeout); }\n"
        "static bool xl6_net_address_parse(xnetaddr* address, const char* host, uint16_t port, bool passive)\n"
        "{ if (address == NULL) return false; if (host == NULL || host[0] == '\\0') return xrtNetAddrAny(address, XL6_XNET_FAMILY_IPV4, port); if (xrtNetAddrParse(address, host, port)) return true; (void)passive; return xrtNetResolveOne(address, host, port, XL6_XNET_FAMILY_UNSPEC); }\n"
        "static char* xlir_net_string_dup(const char* text, size_t size)\n"
        "{ const char* source = text != NULL ? text : \"\"; return size == 0 ? xrtStrDup(source) : xrtStrDupN(source, size); }\n"
        "static char* xl6_net_address_string(const xnetaddr* address)\n"
        "{ char* text = address != NULL ? xrtNetAddrString(address) : NULL; return text != NULL ? text : xlir_net_string_dup(\"\", 0); }\n"
        "static void* xlir_net_value_handle_data(const xvalue* value)\n"
        "{ void* handle = NULL; return value != NULL && xrtValueGetHandle(value, &handle, NULL, NULL) ? handle : NULL; }\n\n",
        out);
}

static void xl6IrCgenEmitNetFutureAdapters(XL6_Output* out, int stream, int bytes)
{
    xl6OutputPuts(
        "static void xlir_net_future_fail(xpromise* output)\n"
        "{ const xerror* error = xrtGetError(); if (error == NULL || !xrtPromiseReject(output, error)) (void)xrtPromiseClose(output); }\n", out);
    if (stream) xl6OutputPuts(
        "static void xl6_net_map_stream(const xfutureresult* input, xpromise* output, void* data)\n"
        "{ (void)data; xlir_native_future_publish_xvalue(output, input != NULL ? xlir_net_stream_wrap_borrowed(input->Value) : NULL); }\n", out);
    if (bytes) xl6OutputPuts(
        "static void xl6_net_map_bytes(const xfutureresult* input, xpromise* output, void* data)\n"
        "{ (void)data; xlir_native_future_publish_xvalue(output, input != NULL ? xlir_net_bytes_wrap_borrowed(input->Value) : NULL); }\n"
        "static void xl6_net_map_text(const xfutureresult* input, xpromise* output, void* data)\n"
        "{ const xnetbytes* bytes = input != NULL ? (const xnetbytes*)input->Value : NULL; XL6_XrtBytesView view; char** box; (void)data; if (bytes == NULL) { xlir_net_future_fail(output); return; } view = xrtNetBytesView(bytes); box = (char**)xrtMalloc(sizeof(*box)); if (box == NULL) { xlir_net_future_fail(output); return; } *box = xrtStrDupN((const char*)view.Data, view.Size); if (*box == NULL) { xrtFree(box); xlir_net_future_fail(output); return; } if (!xl6PayloadResolveOwned(output, box, XL6_FUTURE_PAYLOAD_STRING)) { xl6PayloadDropGet(XL6_FUTURE_PAYLOAD_STRING)(box, NULL); (void)xrtPromiseClose(output); } }\n", out);
    xl6OutputPuts(
        "static void xl6_net_map_bool(const xfutureresult* input, xpromise* output, void* data)\n"
        "{ bool* box; (void)input; (void)data; box = (bool*)xrtMalloc(sizeof(*box)); if (box == NULL) { xlir_net_future_fail(output); return; } *box = true; if (!xl6PayloadResolveOwned(output, box, XL6_FUTURE_PAYLOAD_VALUE)) { xrtFree(box); (void)xrtPromiseClose(output); } }\n"
        "static xfuture* xl6_net_future_map(xfuture* source, xfuturecontinueproc mapper)\n"
        "{ xfuture* mapped; if (source == NULL) return NULL; mapped = xlir_native_future_then(source, mapper, 0); xrtFutureDestroy(source); return mapped; }\n\n",
        out);
}

static int xl6IrCgenEmitNetTcpRuntime(
    XL6_Output* out, const XL6_IrModule* module,
    const XL6_IrCgenNeeds* needs, XL6_DiagList* diags)
{
    XL6_CgenNetNeeds component = {0};
    XL6_IrTypeId streamType;
    XL6_IrTypeId bytesType;
    int needsStreamClass;
    int needsBytesClass;
    size_t i, p;
    if ( needs == NULL || !needs->needsNetLifecycleRuntime ) return 1;
    component.tcpDial = xl6IrCgenNetHasBridge(
        module, XL6_IR_RUNTIME_BRIDGE_NET_TCP_DIAL);
    component.tcpListen = xl6IrCgenNetHasBridge(
        module, XL6_IR_RUNTIME_BRIDGE_NET_TCP_LISTEN);
    component.tcpStream = xl6IrCgenNetHasBridge(
        module, XL6_IR_RUNTIME_BRIDGE_NET_TCP_STREAM);
    for (i = 0; i < module->callTargetCount; ++i) {
        const XL6_IrCallTarget* target = &module->callTargets[i];
        if (target->runtimeBridge != XL6_IR_RUNTIME_BRIDGE_NET_TCP_STREAM ||
            target->reachability == XL6_IR_REACHABILITY_UNREACHABLE) continue;
        for (p = 0; p < target->parameterCount; ++p)
            if (xl6IrTypeGet(&module->types, target->parameterTypes[p])->collectionAbi == XL6_IR_COLLECTION_ARRAY)
                component.tcpVectors = 1;
    }
    component.tcpFile = xl6IrCgenNetHasBridgeOperation(
        module, XL6_IR_RUNTIME_BRIDGE_OPERATION_NET_TCP_FILE_TRANSFER, 1);
    component.tcpServer = xl6IrCgenNetHasBridge(
        module, XL6_IR_RUNTIME_BRIDGE_NET_TCP_SERVER);
    component.tlsStream = needs->needsNetTlsStreamRuntime;
    component.bytes = component.tcpStream || component.tlsStream;
    component.engine = component.tcpDial || component.tcpListen ||
        component.tcpServer || needs->needsNetTlsDialRuntime ||
        needs->needsNetTlsServerRuntime;
    component.resolver = component.tcpDial || needs->needsNetTlsDialRuntime;
    component.future = component.tcpDial || component.tcpStream ||
        component.tcpServer;
    component.addressHelper = component.tcpListen || component.tcpStream ||
        component.tcpServer || needs->needsNetTlsServerRuntime;
    component.deadlineHelper = component.tcpDial || component.tcpServer ||
        needs->needsNetTlsDialRuntime || needs->needsNetTlsServerRuntime;
    component.stats = component.tcpStream || component.tcpServer ||
        needs->needsNetTlsServerRuntime;
    needsStreamClass = component.tcpDial || component.tcpStream ||
        component.tcpServer;
    needsBytesClass = component.bytes;
    streamType = XL6_IR_TYPE_INVALID;
    bytesType = XL6_IR_TYPE_INVALID;
    if ( needsStreamClass ) {
        streamType = xl6IrCgenNetReceiverClassType(
            module, XL6_IR_RUNTIME_BRIDGE_NET_TCP_STREAM);
        if ( streamType == XL6_IR_TYPE_INVALID )
            streamType = xl6IrCgenNetFutureClassPayload(
                module, XL6_IR_RUNTIME_BRIDGE_NET_TCP_DIAL);
        if ( streamType == XL6_IR_TYPE_INVALID )
            streamType = xl6IrCgenNetFutureClassPayload(
                module, XL6_IR_RUNTIME_BRIDGE_NET_TCP_SERVER);
        if ( streamType == XL6_IR_TYPE_INVALID )
            return xl6IrCgenDiag(diags, module, XL6_IR_NODE_INVALID,
                "XLIR reachable TCP component has no unique Stream class type");
    }
    if ( needsBytesClass ) {
        bytesType = xl6IrCgenNetDeclaredFutureClassPayload(
            module,
            XL6_IR_RUNTIME_BRIDGE_OPERATION_NET_TCP_RECV_OWNED_BYTES_ASYNC);
        if ( bytesType == XL6_IR_TYPE_INVALID )
            bytesType = xl6IrCgenNetDeclaredFutureClassPayload(
                module,
                XL6_IR_RUNTIME_BRIDGE_OPERATION_NET_TLS_RECV_OWNED_BYTES_ASYNC);
        if ( bytesType == XL6_IR_TYPE_INVALID || bytesType == streamType )
            return xl6IrCgenDiag(diags, module, XL6_IR_NODE_INVALID,
                "XLIR reachable network stream has no unique NetBytes class type");
    }
    xl6IrCgenEmitNetLifecycle(out, module);
    if ( needsStreamClass &&
         !xl6IrCgenEmitNetClassWrapper(
             out, module, streamType, "stream", "xnetstream",
             "xrtNetStreamRef", diags) ) return 0;
    if ( needsBytesClass &&
         !xl6IrCgenEmitNetClassWrapper(
             out, module, bytesType, "bytes", "xnetbytes",
             "xrtNetBytesRef", diags) ) return 0;
    if ( component.future || needs->needsNetTlsStreamRuntime || needs->needsNetTlsDialRuntime || needs->needsNetTlsServerRuntime )
        xl6IrCgenEmitNetFutureAdapters(out, needsStreamClass, needsBytesClass);
    if (component.tcpVectors) {
        const XL6_IrClassField* field = xl6IrRuntimeNetBufferField(module);
        const XL6_IrType* type = field != NULL ? xl6IrTypeGet(&module->types, field->ownerType) : NULL;
        if (type == NULL) return xl6IrCgenDiag(diags, module, XL6_IR_NODE_INVALID, "XLIR TCP Buffer projection is unresolved");
        xl6IrCgenEmitNetClassHandleProjection(out, field, "tcp_buffer");
        xl6OutputPrintf(out,
            "static void* xlir_net_tcp_buffer_borrow(const xvalue* value)\n"
            "{ return value != NULL && xrtValueTypeId(value) == UINT64_C(0x%016llX) ? xlir_net_tcp_buffer_borrow_handle(value) : NULL; }\n",
            (unsigned long long)type->typeValueId);
        xl6OutputPuts("#define xl6ValueHandleData xlir_net_tcp_buffer_borrow\n", out);
    }
    xl6OutputPuts(
        "#define xl6_xrt_string_dup xlir_net_string_dup\n",
        out);
    xl6CgenEmitNetTcpComponent(out, &component);
    xl6CgenEmitNetBufferComponent(out, &component);
    xl6CgenEmitNetTcpStatsComponent(out, &component);
    xl6OutputPuts(
        "#undef xl6ValueHandleData\n"
        "#undef xl6_xrt_string_dup\n\n", out);
    return !xl6OutputHasFailed(out);
}

static void xl6IrCgenEmitNetTlsAdapters(
    XL6_Output* out, int hasTransport, int hasServer)
{
    xl6OutputPuts("#include <stdarg.h>\n", out);
    if ( hasTransport )
        xl6OutputPuts(
        "static void xlir_tls_future_destroy(xfuture* future) { xrtFutureDestroy(future); }\n"
        "static bool xlir_tls_future_cancel(xfuture* future) { return future != NULL && xrtFutureCancel(future); }\n"
        "static bool xlir_tls_future_wait(xfuture* future) { return future != NULL && xrtFutureWait(future) == 0 && xrtFutureState(future) == XFUTURE_RESOLVED; }\n"
        "static bool xlir_tls_future_wait_for(xfuture* future, long long timeout_ms) { return future != NULL && xrtFutureWaitFor(future, timeout_ms < 0 ? UINT64_MAX : (uint64_t)timeout_ms * 1000u) == 0 && xrtFutureState(future) == XFUTURE_RESOLVED; }\n"
        "static void* xlir_tls_future_value(xfuture* future) { return future != NULL ? xrtFutureValue(future) : NULL; }\n",
        out);
    xl6OutputPuts(
        "static XL6_Function xlir_tls_function_copy_value(XL6_Function value) { if (value.retain != NULL && value.env != NULL) value.retain(value.env); return value; }\n"
        "static void xlir_tls_function_release_value(XL6_Function value) { if (value.release != NULL && value.env != NULL) value.release(value.env); }\n"
        "static xvalue* xlir_tls_function_call_var(XL6_Function function, int count, ...)\n"
        "{ xvalue* arguments[1] = {0}; xvalue* result; va_list values; if (count != 1 || function.invoke_var == NULL) return xrtValueNull(); va_start(values, count); arguments[0] = va_arg(values, xvalue*); va_end(values); result = function.invoke_var(function.env, count, arguments); xrtValueRelease(arguments[0]); return result != NULL ? result : xrtValueNull(); }\n"
        "static bool xlir_tls_value_object_set(xvalue* object, const char* key, size_t key_size, xvalue* value, bool take) { xstrview name = { key != NULL ? key : \"\", key_size != 0 ? key_size : (key != NULL ? strlen(key) : 0u) }; (void)take; return object != NULL && value != NULL && xrtValueObjectSetNew(object, name, value); }\n"
        "static xvalue* xlir_tls_value_text_create(void* text, size_t size, bool take) { const char* value = text != NULL ? (const char*)text : \"\"; xvalue* result = xrtValueString((xstrview){ value, size != 0 ? size : strlen(value) }); if (take) xrtFree(text); return result; }\n"
        "static bool xlir_tls_value_bool(const xvalue* value) { bool result = false; return value != NULL && xrtValueGetBool(value, &result) && result; }\n",
        out);
    if ( hasServer )
        xl6OutputPuts(
        "#ifndef XL6_NET_STATS_RUNTIME\n#define XL6_NET_STATS_RUNTIME\n"
        "typedef struct xl6_net_stats_snapshot { size_t Count; uint64_t Values[34]; } xl6_net_stats_snapshot;\n"
        "static xl6_net_stats_snapshot* xl6_net_stats_create(size_t count) { xl6_net_stats_snapshot* value = (xl6_net_stats_snapshot*)xrtCalloc(1u, sizeof(*value)); if (value != NULL) value->Count = count; return value; }\n"
        "#endif\n",
        out);
}

static int xl6IrCgenEmitNetTlsRuntime(
    XL6_Output* out, const XL6_IrModule* module,
    const XL6_IrCgenNeeds* needs, XL6_DiagList* diags)
{
    XL6_IrTypeId streamType;
    int hasTransport;
    if ( needs == NULL || !needs->needsNetTlsRuntime ) return 1;
    hasTransport = needs->needsNetTlsStreamRuntime ||
        needs->needsNetTlsDialRuntime || needs->needsNetTlsServerRuntime;
    if (hasTransport && module->abi.nativeFutureMapAbi != XL6_IR_NATIVE_FUTURE_MAP_RESIDENT_EXCLUSIVE_V1)
        return xl6IrCgenDiag(diags, module, XL6_IR_NODE_INVALID, "XLIR TLS native Future mapper ABI is unresolved");
    if ( hasTransport ) {
        streamType = xl6IrCgenNetReceiverClassType(
            module, XL6_IR_RUNTIME_BRIDGE_NET_TLS_STREAM);
        if ( streamType == XL6_IR_TYPE_INVALID )
            streamType = xl6IrCgenNetFutureClassPayload(
                module, XL6_IR_RUNTIME_BRIDGE_NET_TLS_DIAL);
        if ( streamType == XL6_IR_TYPE_INVALID )
            streamType = xl6IrCgenNetFutureClassPayload(
                module, XL6_IR_RUNTIME_BRIDGE_NET_TLS_SERVER);
        if ( streamType == XL6_IR_TYPE_INVALID ||
             !xl6IrCgenEmitNetOwnedClassWrapper(
                 out, module, streamType, "tls_stream", diags) ) return 0;
    }
    xl6IrCgenEmitNetTlsAdapters(
        out, hasTransport, needs->needsNetTlsServerRuntime);
    xl6OutputPuts(
        "#define xl6FutureDestroy xlir_tls_future_destroy\n"
        "#define xl6FutureCancel xlir_tls_future_cancel\n"
        "#define xl6FutureWait xlir_tls_future_wait\n"
        "#define xl6FutureWaitFor xlir_tls_future_wait_for\n"
        "#define xl6FutureValue xlir_tls_future_value\n"
        "#define xl6_function_copy_value xlir_tls_function_copy_value\n"
        "#define xl6_function_release_value xlir_tls_function_release_value\n"
        "#define xl6_function_call_var xlir_tls_function_call_var\n"
        "#define xl6_xrt_value_object_set xlir_tls_value_object_set\n"
        "#define xl6_xrt_value_text_create xlir_tls_value_text_create\n"
        "#define xl6_xrt_value_bool xlir_tls_value_bool\n"
        "#define xl6_xrt_string_dup xlir_net_string_dup\n"
        "#define xl6ValueHandleData xlir_net_value_handle_data\n"
        "#define xl6_xrt_tls_config_destroy xlir_tls_component_config_destroy\n"
        "#define xl6_xrt_tls_session_destroy xlir_tls_component_session_destroy\n"
        "#define xl6_xrt_tls_resume_destroy xlir_tls_component_resume_destroy\n"
        "#define xl6_xrt_tls_stream_destroy xlir_tls_component_stream_destroy\n"
        "#define xl6_xrt_tls_server_destroy xlir_tls_component_server_destroy\n",
        out);
    xl6CgenEmitTlsRuntime(
        out,
        needs->needsNetTlsStreamRuntime,
        needs->needsNetTlsDialRuntime,
        needs->needsNetTlsServerRuntime,
        "xlir_net_tls_stream_borrow_handle",
        "xlir_net_bytes_borrow_handle",
        XL6_CGEN_TLS_FUTURE_RESIDENT_EXCLUSIVE_V1);
    xl6OutputPuts(
        "#undef xl6_xrt_tls_server_destroy\n"
        "#undef xl6_xrt_tls_stream_destroy\n"
        "#undef xl6_xrt_tls_resume_destroy\n"
        "#undef xl6_xrt_tls_session_destroy\n"
        "#undef xl6_xrt_tls_config_destroy\n"
        "#undef xl6ValueHandleData\n"
        "#undef xl6_xrt_string_dup\n"
        "#undef xl6_xrt_value_bool\n"
        "#undef xl6_xrt_value_text_create\n"
        "#undef xl6_xrt_value_object_set\n"
        "#undef xl6_function_call_var\n"
        "#undef xl6_function_release_value\n"
        "#undef xl6_function_copy_value\n"
        "#undef xl6FutureValue\n"
        "#undef xl6FutureWaitFor\n"
        "#undef xl6FutureWait\n"
        "#undef xl6FutureCancel\n"
        "#undef xl6FutureDestroy\n"
        "void xl6_xrt_tls_config_destroy(void* value) { (void)xlir_tls_component_config_destroy(value); }\n"
        "void xl6_xrt_tls_session_destroy(void* value) { (void)xlir_tls_component_session_destroy(value); }\n"
        "void xl6_xrt_tls_resume_destroy(void* value) { (void)xlir_tls_component_resume_destroy(value); }\n",
        out);
    if ( needs->needsNetTlsStreamRuntime || needs->needsNetTlsDialRuntime ||
         needs->needsNetTlsServerRuntime )
        xl6OutputPuts(
            "void xl6_xrt_tls_stream_destroy(void* value) { (void)xlir_tls_component_stream_destroy(value); }\n",
            out);
    if ( needs->needsNetTlsServerRuntime )
        xl6OutputPuts(
            "void xl6_xrt_tls_server_destroy(void* value) { (void)xlir_tls_component_server_destroy(value); }\n",
            out);
    xl6OutputPuts("\n", out);
    return !xl6OutputHasFailed(out);
}
