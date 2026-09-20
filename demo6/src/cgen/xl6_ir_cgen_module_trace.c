/* Module-owned globals are explicit lowered storage, not a flattened list of
 * object references. Retained Value shells and shared backing stay visible.
 * The host supplies whole-graph quiescence and keeps this code resident. */
static int xl6IrCgenEmitModuleOwnershipTrace(XL6_Output* out,
    const XL6_IrModule* module, const XL6_IrCgenNeeds* needs)
{
    size_t i;
    if (module->abi.codeOwnerAbi != XL6_IR_MODULE_CODE_OWNER_BIND_BEFORE_INIT) return 1;
    xl6OutputPuts("XLIR_C_API bool xl_script_module_ownership_trace(XL6_ModuleOwnershipVisit visit, void* context)\n"
        "{\n    (void)context; if (visit == NULL) return false;\n", out);
    if (module->abi.continuationAbi != XL6_IR_MODULE_CONTINUATION_NONE)
        xl6OutputPuts("    if (xlir_module_initializing) return false;\n", out);
    if (module->abi.networkServiceAbi == XL6_IR_MODULE_NETWORK_SERVICE_RESIDENT_RETRYABLE_V1)
        xl6OutputPuts("    if (!xl6NetworkServiceRuntimeV1Get()->IsEmpty(&xlir_network_services))\n"
            "        return false; /* opaque live service ownership, including partial retirement */\n", out);
    xl6OutputPuts("    if (xlir_module_initialized == 0) return true;\n", out);
    if (module->abi.finalizerFunction != 0)
        /* After the initializer callback returns, phase 1 is a stable partial
         * module. Async cleanup may keep it alive, so committed nullable slots
         * must participate in the same ownership graph as phase 2. */
        xl6OutputPuts("    if (xlir_module_initialized != 1 && xlir_module_initialized != 2) return false;\n", out);
    if (module->abi.executorOwnershipAbi == XL6_IR_MODULE_EXECUTOR_OWNERSHIP_RESIDENT_WAIT_SLOTS_JOIN_V1)
        xl6OutputPuts("    if (xlir_continuation_executor != NULL &&\n"
            "        !visit(xl6ContinuationExecutorOwnership(xlir_continuation_executor), context)) return false;\n", out);
    else if (module->abi.executorOwnershipAbi != XL6_IR_MODULE_EXECUTOR_OWNERSHIP_NONE) return 0;
    if (module->abi.taskPoolOwnershipAbi == XL6_IR_MODULE_TASK_POOL_OWNERSHIP_RESIDENT_NATIVE_JOBS_JOIN_V1)
        xl6OutputPuts("    if (xlir_async_pool != NULL &&\n"
            "        !visit(xl6TaskPoolOwnership(xlir_async_pool), context)) return false;\n", out);
    else if (module->abi.taskPoolOwnershipAbi != XL6_IR_MODULE_TASK_POOL_OWNERSHIP_NONE) return 0;
    (void)needs;
    for (i = 0; i < module->storageCount; ++i) {
        const XL6_IrStorage* storage = &module->storages[i];
        const char* view;
        if (storage->kind != XL6_IR_STORAGE_GLOBAL || storage->moduleTraceAbi == XL6_IR_FRAME_TRACE_NONE) continue;
        if (storage->moduleTraceAbi == XL6_IR_FRAME_TRACE_OPAQUE) {
            xl6OutputPuts("    return false; /* explicit opaque global storage */\n", out);
            continue;
        }
        view = xl6IrCgenOwnershipView(storage->moduleTraceAbi);
        if (view == NULL) return 0;
        xl6OutputPrintf(out, "    if (!visit(%s(", view);
        xl6IrCgenPutStorageName(out, module, storage->symbol);
        xl6OutputPuts("), context)) return false;\n", out);
    }
    xl6OutputPuts("    return true;\n}\n\n", out);
    return !xl6OutputHasFailed(out);
}
