/* Fixed artifact ABI adapters, not language bodies. Existing verified CFGs
 * retain all semantic cleanup; these thin wrappers only register activity.
 * Residency belongs to the native caller, so Close/OOM cannot skip finalizers. */
static void xl6IrCgenEmitHostActivityWrapper(XL6_Output* out, const XL6_IrModule* module,
    const char* linkage, const char* resultType, const char* name,
    const char* parameters, const char* invocation)
{
    xl6OutputPrintf(out, "%s%s %s(%s)\n{\n", linkage, resultType, name, parameters);
    if (module->abi.hostExecutionAbi == XL6_IR_MODULE_HOST_EXECUTION_RESIDENT_BORROWED_ACTIVITY_V1) {
        xl6OutputPuts("    XL6_CodeActivityV1 execution = {0};\n"
            "    xl6CodeActivityEnterV1(&execution, xlir_code_owner.size ? &xlir_code_owner : NULL);\n", out);
        xl6OutputPrintf(out, "    %s result = %s;\n", resultType, invocation);
        xl6OutputPuts("    xl6CodeActivityLeaveV1(&execution);\n    return result;\n", out);
    } else xl6OutputPrintf(out, "    return %s;\n", invocation);
    xl6OutputPuts("}\n\n", out);
}

static int xl6IrCgenEmitModuleEntry(XL6_Output* out, const XL6_IrModule* module, XL6_DiagList* diags)
{
    const XL6_IrCallTarget* target;
    if ( module->abi.entryKind == XL6_IR_MODULE_ENTRY_NONE ) return 1;
    target = xl6IrCallTargetGet(module, module->abi.entrySymbol);
    if ( module->abi.entryKind != XL6_IR_MODULE_ENTRY_XLANG_MAIN || target == NULL ||
         target->kind != XL6_IR_CALL_TARGET_INTERNAL || target->parameterCount != 1 ||
         module->abi.entryArgumentCarrierAbi!=XL6_IR_COLLECTION_CARRIER_SEQUENCE_V1 )
        return xl6IrCgenDiag(diags, module, target != NULL ? target->declaration : XL6_IR_NODE_INVALID,
                             "XLIR C emitter module entry ABI is unresolved");
    /* Only the verified language outcome denotes an uncaught exception.
     * Successful status-returning native calls may leave XRT diagnostics;
     * discard those at a successful host boundary, never inside the body. */
    xl6OutputPuts("extern void xrtClearError(void);\n", out);
    const XL6_IrType* argument=xl6IrTypeGet(&module->types,module->abi.entryArgumentType);
    if(!argument) return xl6IrCgenDiag(diags,module,target->declaration,
        "XLIR C emitter module entry has no canonical argument TypeId");
    xl6OutputPuts("extern xvalue* xl6ArrayEntryAcquireV1(uint64_t, uint64_t, const xvalue*);\n",out);
    xl6OutputPuts("static int xlir_script_main_checked(xvalue* arg, int* exit_code)\n{\n", out);
    xl6OutputPuts("    xl6_error_scope cleanup = {0}; xl6_error_result outcome;\n"
        "    int code = 0, ok;\n    *exit_code = 1;\n",out);
    xl6OutputPrintf(out,"    xvalue* owned_arg = xl6ArrayEntryAcquireV1(0x%016llXULL, 0x%016llXULL, arg);\n",
        (unsigned long long)argument->typeValueId,(unsigned long long)module->abi.entryArgumentElementIdentity);
    xl6OutputPuts("    if (!owned_arg) return 0;\n"
        "    ok = xlir_module_init();\n    if (!ok) goto entry_cleanup;\n",out);
    if ( target->errorAbi == XL6_IR_ERROR_ABI_MAY_RAISE ) {
        const char* resultType = xl6IrCgenErrorResultType(module, target->returnType);
        if ( resultType == NULL )
            return xl6IrCgenDiag(diags, module, target->declaration,
                                 "XLIR C emitter entry error-result ABI is unresolved");
        xl6OutputPrintf(out, "    %s result = xlir_fn_%u(owned_arg);\n", resultType,
                        (unsigned int)target->symbol);
        xl6OutputPuts("    ok = result.ok;\n",out);
        if(target->returnType!=XL6_IR_TYPE_INVALID)
            xl6OutputPuts("    if (ok) code = (int)result.value;\n",out);
    } else if ( target->returnType == XL6_IR_TYPE_INVALID ) {
        xl6OutputPrintf(out, "    xlir_fn_%u(owned_arg);\n", (unsigned int)target->symbol);
    } else {
        xl6OutputPrintf(out, "    code = (int)xlir_fn_%u(owned_arg);\n", (unsigned int)target->symbol);
    }
    xl6OutputPuts("entry_cleanup:\n"
        "    if (ok) xrtClearError();\n"
        "    if (!xl6ErrorScopeBeginCleanup(&cleanup, !ok)) abort();\n"
        "    xrtValueRelease(owned_arg);\n"
        "    if (xrtGetError()) xl6ErrorScopeCaptureFailure(&cleanup);\n"
        "    outcome = xl6ErrorScopeEnd(&cleanup); xrtSetErrorTake(outcome.error);\n"
        "    if (!outcome.ok) return 0;\n"
        "    *exit_code = code;\n    return 1;\n",out);
    xl6OutputPuts("}\n\n", out);
    xl6IrCgenEmitHostActivityWrapper(out, module, "XLIR_C_API ", "int", "xl_script_main_checked",
        "xvalue* arg, int* exit_code", "xlir_script_main_checked(arg, exit_code)");
    xl6OutputPuts(
        "XLIR_C_API int xlang_main(xvalue* arg)\n{\n"
        "    int code;\n"
        "    return xl_script_main_checked(arg, &code) ? code : 1;\n"
        "}\n\n", out);
    return !xl6OutputHasFailed(out);
}

static int xl6IrCgenEmitSyntheticScriptEntry(
    XL6_Output* out, const XL6_IrModule* module, int emitScriptEntry)
{
    if ( !emitScriptEntry ||
         module->abi.entryKind != XL6_IR_MODULE_ENTRY_NONE ) return 1;
    xl6OutputPuts(
        "extern void xrtClearError(void);\n"
        "static int xlir_script_main_checked(xvalue* arg, int* exit_code)\n"
        "{\n"
        "    (void)arg; *exit_code = 1;\n"
        "    if (!xlir_module_init()) return 0;\n"
        "    xrtClearError();\n"
        "    *exit_code = 0;\n    return 1;\n"
        "}\n\n", out);
    xl6IrCgenEmitHostActivityWrapper(out, module, "XLIR_C_API ", "int", "xl_script_main_checked",
        "xvalue* arg, int* exit_code", "xlir_script_main_checked(arg, exit_code)");
    xl6OutputPuts(
        "XLIR_C_API int xlang_main(xvalue* arg)\n{\n"
        "    int code;\n"
        "    return xl_script_main_checked(arg, &code) ? code : 1;\n"
        "}\n\n",
        out);
    return !xl6OutputHasFailed(out);
}

/* One semantic finalizer boundary, with a distinct physical-retirement step.
 * BUSY preserves services/code; a retry never repeats committed IR cleanup. */
static void xl6IrCgenEmitRetireSteps(XL6_Output* out, const XL6_IrModule* module)
{
    xl6OutputPuts("static XL6_ModuleRetireResult xlir_module_retire(bool wait)\n{\n"
        "    XL6_ModulePrepareResult prepared; XL6_ModuleRetireResult retired;\n"
        "    xl6_error_scope cleanup = {0}; xl6_error_result outcome;\n", out);
    if (module->abi.continuationAbi != XL6_IR_MODULE_CONTINUATION_NONE)
        xl6OutputPuts("    if (xlir_module_initializing) return XL6_MODULE_RETIRE_BUSY;\n", out);
    xl6OutputPuts("    prepared = xlir_module_prepare_resources(true, wait);\n"
        "    if (prepared == XL6_MODULE_PREPARE_BUSY) return XL6_MODULE_RETIRE_BUSY;\n"
        "    if (prepared != XL6_MODULE_PREPARE_READY) return XL6_MODULE_RETIRE_FAILED;\n"
        "    if (!xl6ErrorScopeBeginCleanup(&cleanup, false)) abort();\n"
        "    xlir_module_fini();\n",out);
    if(module->abi.executionOwnershipAbi==XL6_IR_MODULE_EXECUTION_OWNERSHIP_RESIDENT_SOURCE_V1){
        xl6OutputPuts("    retired = xlir_module_initialized == 0\n"
            "        ? xl6ExecutionModuleRetireV1(&xlir_execution_source, wait) : XL6_MODULE_RETIRE_BUSY;\n",out);
        if(module->abi.networkServiceAbi!=XL6_IR_MODULE_NETWORK_SERVICE_NONE)
            xl6OutputPuts("    if (retired == XL6_MODULE_RETIRE_FAILED) xl6ErrorScopeCaptureFailure(&cleanup);\n"
                "    if (xlir_module_initialized == 0 && xlir_execution_source == NULL)\n"
                "        retired = xlir_net_runtime_retire(wait);\n",out);
    }else xl6OutputPuts(module->abi.networkServiceAbi!=XL6_IR_MODULE_NETWORK_SERVICE_NONE
        ? "    retired = xlir_module_initialized == 0 ? xlir_net_runtime_retire(wait) : XL6_MODULE_RETIRE_BUSY;\n"
        : "    retired = xlir_module_initialized == 0 ? XL6_MODULE_RETIRE_READY : XL6_MODULE_RETIRE_BUSY;\n",out);
    xl6OutputPuts(
        "    if (retired == XL6_MODULE_RETIRE_FAILED) xl6ErrorScopeCaptureFailure(&cleanup);\n"
        "    outcome = xl6ErrorScopeEnd(&cleanup); xrtSetErrorTake(outcome.error);\n"
        "    return outcome.ok ? retired : XL6_MODULE_RETIRE_FAILED;\n}\n"
        "static XL6_ModuleRetireResult xlir_script_module_retire_step(void)\n"
        "{ return xlir_module_retire(false); }\n\n"
        "static int xlir_script_module_unit_checked(void)\n"
        "{ return xlir_module_retire(true) == XL6_MODULE_RETIRE_READY; }\n\n", out);
    xl6IrCgenEmitHostActivityWrapper(out, module, "XLIR_C_API ", "XL6_ModuleRetireResult",
        "xl_script_module_retire_step", "void", "xlir_script_module_retire_step()");
}

/* These host adapters invoke the verified module entry/finalization program.
 * Owned storage and error preservation are instructions in that program,
 * never inferred from declarations by this artifact boundary. */
static int xl6IrCgenEmitHostBindings(
    XL6_Output* out, const XL6_IrModule* module,
    int emitScriptEntry, int emitMainWrapper,
    int emitErrorKind, int unitNet, const XL6_IrCgenEntryContractV1* entry, XL6_DiagList* diags)
{
    if ( module->abi.entryKind == XL6_IR_MODULE_ENTRY_NONE &&
         !emitScriptEntry ) return 1;
    if ( module->abi.entryKind != XL6_IR_MODULE_ENTRY_NONE &&
         module->abi.entryKind != XL6_IR_MODULE_ENTRY_XLANG_MAIN )
        return xl6IrCgenDiag(diags, module, XL6_IR_NODE_INVALID,
                             "XLIR C emitter host entry ABI is unresolved");
    if (unitNet && module->abi.retirementAbi != XL6_IR_MODULE_RETIREMENT_RETRYABLE_SERVICES_V4)
        return xl6IrCgenDiag(diags, module, XL6_IR_NODE_INVALID,
            "XLIR network service requires explicit retryable retirement ABI");
    xl6OutputPuts(
        "#ifndef XLIR_XRT_DECLARATIONS\n"
        "typedef enum xconsolestream { XCONSOLE_STDOUT = 1, XCONSOLE_STDERR } xconsolestream;\n"
        "extern const xerror* xrtGetError(void);\n"
        "extern xstrview xrtErrorMessageView(const xerror*);\n"
        "extern void xrtClearError(void);\n"
        "extern xstrview xrtStrView(const char*);\n"
        "extern bool xrtConsoleWrite(xconsolestream, xstrview);\n"
        "extern bool xrtConsoleWriteLine(xconsolestream, xstrview);\n"
        "#endif\n"
        "int xl_script_has_error(void)\n"
        "{ return xrtGetError() != NULL; }\n\n"
        "XLIR_C_API void xl_script_print_uncaught(void)\n"
        "{\n"
        "    const xerror* error = xrtGetError();\n"
        "    if (error != NULL) {\n"
        "        (void)xrtConsoleWrite(XCONSOLE_STDERR, xrtStrView(\"uncaught xlang error: \"));\n",
        out);
    if ( emitErrorKind )
        xl6OutputPuts(
            "        (void)xrtConsoleWrite(XCONSOLE_STDERR, xrtStrView(xlir_error_kind_name(error)));\n"
            "        (void)xrtConsoleWrite(XCONSOLE_STDERR, xrtStrView(\": \"));\n",
            out);
    xl6OutputPuts(
        "        (void)xrtConsoleWriteLine(XCONSOLE_STDERR, xrtErrorMessageView(error));\n"
        "    }\n"
        "}\n\n"
        "XLIR_C_API void xl_script_clear_error(void)\n"
        "{ xrtClearError(); }\n\n"
        "XLIR_C_API void xl_script_error_unit(void)\n"
        "{ xrtClearError(); }\n\n"
        "static int xlir_script_module_init(xvalue* arg)\n"
        "{\n"
        "    int ok; (void)arg; ok = xlir_module_init();\n"
        "    if (ok) xrtClearError();\n"
        "    return ok;\n"
        "}\n\n",
        out);
    xl6IrCgenEmitHostActivityWrapper(out, module, "XLIR_C_API ", "int", "xl_script_module_init",
        "xvalue* arg", "xlir_script_module_init(arg)");
    if (module->abi.retirementAbi == XL6_IR_MODULE_RETIREMENT_RETRYABLE_SERVICES_V4) {
        xl6IrCgenEmitRetireSteps(out, module);
    } else {
    xl6OutputPuts("static int xlir_script_module_unit_checked(void)\n{\n", out);
    if (xl6IrModuleHasFinalizerWork(module) || unitNet)
        xl6OutputPuts("    xl6_error_scope cleanup = {0}; xl6_error_result outcome;\n", out);
    if (module->abi.continuationAbi != XL6_IR_MODULE_CONTINUATION_NONE)
        xl6OutputPuts("    if (xlir_module_initializing || (xlir_module_initialized != 1 && xlir_module_initialized != 2)) return 1;\n", out);
    else if ( module->abi.finalizerFunction != 0 )
        /* Only an initialized host module may be unloaded. The initializer
         * invokes the IR finalizer directly on failure, while phase 1 is
         * still active. An external callback must not cancel that transaction. */
        xl6OutputPuts("    if (xlir_module_initialized != 2) return 1;\n", out);
    if (xl6IrModuleHasFinalizerWork(module) || unitNet)
        xl6OutputPuts("    if (!xl6ErrorScopeBeginCleanup(&cleanup, false)) abort();\n", out);
    if ( module->abi.finalizerFunction != 0 )
        xl6OutputPuts("    xlir_module_fini();\n", out);
    if (xl6IrModuleHasFinalizerWork(module) || unitNet)
        xl6OutputPuts("    outcome = xl6ErrorScopeEnd(&cleanup);\n"
            "    xrtSetErrorTake(outcome.error);\n    return outcome.ok;\n", out);
    else xl6OutputPuts("    return 1;\n", out);
    xl6OutputPuts("}\n\n", out);
    }
    xl6IrCgenEmitHostActivityWrapper(out, module, "XLIR_C_API ", "int", "xl_script_module_unit_checked",
        "void", "xlir_script_module_unit_checked()");
    xl6OutputPuts("XLIR_C_API void xl_script_module_unit(void)\n{\n", out);
    if (xl6IrModuleHasFinalizerWork(module) || unitNet)
        xl6OutputPuts("    if (!xl_script_module_unit_checked())\n"
            "        xl6ErrorScopePropagateFailure((xl6_error_result){ false, xrtTakeError() });\n", out);
    else xl6OutputPuts("    (void)xl_script_module_unit_checked();\n", out);
    xl6OutputPuts("}\n\n", out);
    if (module->abi.retirementAbi == XL6_IR_MODULE_RETIREMENT_PREPARED_GRAPH_V2 ||
        module->abi.retirementAbi == XL6_IR_MODULE_RETIREMENT_OWNED_SERVICES_GRAPH_V3 ||
        module->abi.retirementAbi == XL6_IR_MODULE_RETIREMENT_RETRYABLE_SERVICES_V4)
        xl6IrCgenEmitHostActivityWrapper(out, module, "static ", "XL6_ModulePrepareResult",
            "xlir_host_prepare_retirement", "void", "xlir_module_prepare_retirement()");
    if (module->abi.retirementAbi == XL6_IR_MODULE_RETIREMENT_PREPARED_GRAPH_V2 ||
        module->abi.retirementAbi == XL6_IR_MODULE_RETIREMENT_OWNED_SERVICES_GRAPH_V3 ||
        module->abi.retirementAbi == XL6_IR_MODULE_RETIREMENT_RETRYABLE_SERVICES_V4)
        xl6OutputPuts("static const XL6_ModuleRetirementV1 xlir_module_retirement_graph = {\n"
            "    sizeof(XL6_ModuleRetirementV1), 0x584C364D525431ULL,\n"
            "    xl_script_module_ownership_trace, xl_script_module_unit_checked};\n"
            "XLIR_C_API const XL6_ModuleRetirementV1* xl_script_module_retirement_v1(void)\n"
            "{ return &xlir_module_retirement_graph; }\n\n", out);
    if (module->abi.retirementAbi == XL6_IR_MODULE_RETIREMENT_PREPARED_GRAPH_V2)
        xl6OutputPuts("XLIR_C_API const XL6_ModuleRetirementV2* xl_script_module_retirement_v2(void)\n"
            "{\n    static const XL6_ModuleRetirementV2 api = {sizeof(api), 0x584C364D525432ULL,\n"
            "        &xlir_module_retirement_graph, xlir_host_prepare_retirement};\n"
            "    return &api;\n}\n\n", out);
    if (module->abi.retirementAbi == XL6_IR_MODULE_RETIREMENT_OWNED_SERVICES_GRAPH_V3)
        xl6OutputPuts("XLIR_C_API const XL6_ModuleRetirementV3* xl_script_module_retirement_v3(void)\n"
            "{\n    static const XL6_ModuleRetirementV3 api = {sizeof(api), 0x584C364D525433ULL,\n"
            "        &xlir_module_retirement_graph, xlir_host_prepare_retirement};\n"
            "    return &api;\n}\n\n", out);
    if (module->abi.retirementAbi == XL6_IR_MODULE_RETIREMENT_RETRYABLE_SERVICES_V4)
        xl6OutputPuts("XLIR_C_API const XL6_ModuleRetirementV4* xl_script_module_retirement_v4(void)\n"
            "{\n    static const XL6_ModuleRetirementV4 api = {sizeof(api), 0x584C364D525434ULL,\n"
            "        &xlir_module_retirement_graph, xlir_host_prepare_retirement, xl_script_module_retire_step};\n"
            "    return &api;\n}\n\n", out);
    xl6OutputPuts("#if defined(_WIN32) && defined(XL6_BUILD_SHARED)\n"
        "extern bool xrtRuntimeRetireThreadStorage(void);\n"
        "/* Host must stop all entry/callback admission and release external\n"
        " * resources before this terminal call; not a module reset operation. */\n"
        "XLIR_C_API int xl_script_runtime_retire_checked(void)\n"
        "{\n"
        "    static int phase;\n"
        "    if (phase == 2) return 1;\n"
        "    if (phase == 0) {\n"
        "        if (!xl_script_module_unit_checked()) return 0;\n"
        "        phase = 1;\n"
        "    }\n"
        "    if (!xrtRuntimeRetireThreadStorage()) return 0;\n"
        "    phase = 2; return 1;\n"
        "}\n"
        "#endif\n\n", out);
    if ( emitMainWrapper ) {
        if(entry->managed)return xl6IrCgenEmitStaticEntry(out,module,entry);
        xl6OutputPuts(
            "#ifndef XLIR_XRT_DECLARATIONS\n"
            "extern void xrtValueRelease(xvalue*);\n"
            "#endif\n",out);
        if(module->abi.entryKind==XL6_IR_MODULE_ENTRY_XLANG_MAIN) {
            const XL6_IrType* argument=xl6IrTypeGet(&module->types,module->abi.entryArgumentType);
            xl6OutputPuts("extern xvalue* xl6ArrayOsArgvCreateV1(uint64_t, uint64_t, int, char* const*);\n"
                "int main(int argc, char** argv)\n{\n",out);
            xl6OutputPrintf(out,"    xvalue* arg = xl6ArrayOsArgvCreateV1(0x%016llXULL, 0x%016llXULL, argc, argv);\n",
                (unsigned long long)argument->typeValueId,(unsigned long long)module->abi.entryArgumentElementIdentity);
            xl6OutputPuts("    if (!arg) return 1;\n",out);
        } else {
            xl6OutputPuts("int main(int argc, char** argv)\n{\n"
                "    (void)argc; (void)argv; xvalue* arg = NULL;\n",out);
        }
        xl6OutputPuts(
            "    xl6_error_scope cleanup = {0}; xl6_error_result outcome;\n"
            "    int code, ok;\n"
            "    ok = xl_script_main_checked(arg, &code);\n"
            "    if (!xl6ErrorScopeBeginCleanup(&cleanup, !ok)) abort();\n"
            "    xrtValueRelease(arg);\n"
            "    if (xrtGetError() != NULL) xl6ErrorScopeCaptureFailure(&cleanup);\n"
            "    if (!xl_script_module_unit_checked()) xl6ErrorScopeCaptureFailure(&cleanup);\n"
            "    outcome = xl6ErrorScopeEnd(&cleanup);\n"
            "    xrtSetErrorTake(outcome.error);\n"
            "    if (!outcome.ok) {\n"
            "        xl_script_print_uncaught();\n"
            "        xl_script_clear_error();\n"
            "        return 1;\n"
            "    }\n"
            "    return code;\n"
            "}\n",
            out);
    }
    return !xl6OutputHasFailed(out);
}

#include "src/cgen/xl6_ir_cgen_export_result.c"

static void xl6IrCgenEmitExportFailureReturn(
    XL6_Output* out, const XL6_IrExport* exported, const char* returnType)
{
    if (exported->returnAbi == XL6_IR_EXPORT_RETURN_VOID)
        xl6OutputPuts("    return;\n", out);
    else
        xl6OutputPrintf(out, "    return (%s){0};\n", returnType);
}

/* The export transaction owns its result until the final success commit.
 * Release is selected from the verified return TypeId, just like IR RELEASE. */
static int xl6IrCgenEmitExportResultRollback(
    XL6_Output* out, const XL6_IrModule* module, const XL6_IrExport* exported)
{
    const XL6_IrType* type = xl6IrTypeGet(&module->types, exported->returnType);
    if (exported->returnAbi != XL6_IR_EXPORT_RETURN_OWNED) return 1;
    xl6OutputPuts(
        "    { xl6_foreign_error_scope cleanup = {0}; xl6_foreign_error_result discarded;\n"
        "      if (!xl6ForeignCallBegin(&cleanup)) abort();\n", out);
    if (type != NULL && type->cAbi == XL6_IR_C_ABI_PRODUCT &&
        xl6IrCgenProductDropNeeded(module, exported->returnType))
        xl6OutputPrintf(out, "      xlir_product_drop_t%u(&pending_result);\n",
            (unsigned int)exported->returnType);
    else if (!xl6IrCgenEmitDetachedRelease(out,
                 xl6IrCgenType(module, exported->returnType),
                 xl6IrCgenStorageReleaseFunction(type), "pending_result", NULL))
        return 0;
    xl6OutputPuts(
        "      discarded = xl6ForeignCallEnd(&cleanup); xrtErrorFree(discarded.error); }\n", out);
    return 1;
}

static int xl6IrCgenEmitExport(
    XL6_Output* out, const XL6_IrModule* module, const XL6_IrExport* exported, XL6_DiagList* diags)
{
    const XL6_IrCallTarget* target = exported != NULL ? xl6IrCallTargetGet(module, exported->symbol) : NULL;
    const char* returnType;
    const char* errorResultType;
    int mayRaise;
    int materialize;
    int hasCallback = 0;
    size_t i;
    if ( exported == NULL || exported->cName == NULL || target == NULL ||
         target->kind != XL6_IR_CALL_TARGET_INTERNAL ||
         exported->executionAbi != XL6_IR_EXPORT_EXECUTION_RESIDENT_CODE_SCOPE_V1 )
        return xl6IrCgenDiag(diags, module, exported != NULL ? exported->declaration : XL6_IR_NODE_INVALID,
                             "XLIR C emitter export linkage ABI is unresolved");
    returnType = exported->returnAbi == XL6_IR_EXPORT_RETURN_VOID
        ? "void" : xl6IrCgenExportValueType(
            module, exported->returnType, exported->returnValueAbi);
    if ( returnType == NULL )
        return xl6IrCgenDiag(diags, module, exported->declaration,
                             "XLIR C emitter does not support export return ABI");
    mayRaise = exported->bodyErrorAbi == XL6_IR_ERROR_ABI_MAY_RAISE;
    materialize = exported->resultBridgeAbi == XL6_IR_EXPORT_RESULT_C_OWNED_COPY;
    errorResultType = mayRaise ? xl6IrCgenErrorResultType(module, target->returnType) : NULL;
    if ( mayRaise && errorResultType == NULL )
        return xl6IrCgenDiag(diags, module, exported->declaration,
                             "XLIR C emitter export error-result adapter ABI is unresolved");
    xl6OutputPrintf(out, "XLIR_C_API %s %s(", returnType, exported->cName);
    if ( exported->parameterCount == 0 ) xl6OutputPuts("void", out);
    for ( i = 0; i < exported->parameterCount; ++i ) {
        if ( i > 0 ) xl6OutputPuts(", ", out);
        if ( !xl6IrCgenEmitExportParameter(out, module, exported, i, 1, diags) ) return 0;
        if ( xl6IrExportValueAbiIsCallback(exported->parameterValueAbis[i]) )
            hasCallback = 1;
    }
    xl6OutputPuts(")\n{\n", out);
    xl6OutputPuts(
        "    xl6_foreign_error_scope entry = {0}; xl6_foreign_error_result outcome;\n"
        "    XL6_CodeScope execution = {0}; XL6_CodeLease* code = NULL;\n", out);
    if (exported->returnAbi != XL6_IR_EXPORT_RETURN_VOID)
        xl6OutputPrintf(out, "    %s pending_result = {0};\n",
            xl6IrCgenType(module, exported->returnType));
    if (materialize)
        xl6OutputPrintf(out, "    %s body_result = {0};\n", xl6IrCgenType(module, target->returnType));
    for ( i = 0; i < exported->parameterCount; ++i )
        if ( xl6IrExportValueAbiIsCallback(exported->parameterValueAbis[i]) )
            xl6OutputPrintf(out, "    xlir_closure* xlir_export_arg_%lu = NULL;\n", (unsigned long)i);
    xl6OutputPuts(
        "    if (!xl6ForeignCallBegin(&entry)) {\n"
        "        outcome = (xl6_foreign_error_result){false, xrtTakeError()};\n"
        "        xl6ErrorScopePropagateFailure(outcome);\n", out);
    xl6IrCgenEmitExportFailureReturn(out, exported, returnType);
    xl6OutputPuts(
        "    }\n"
        "    if (xl6CodeLeaseAcquireScoped(xlir_code_owner.size ? &xlir_code_owner : NULL, &code) != XL6_CODE_LEASE_ACQUIRED) {\n"
        "        xl6ErrorScopeCaptureFailure(&entry); goto xlir_export_finish;\n"
        "    }\n"
        "    xl6CodeScopeEnter(&execution, code); xl6CodeLeaseRelease(code);\n"
        "    if (!xlir_module_init()) { xl6ErrorScopeCaptureFailure(&entry); goto xlir_export_finish; }\n", out);
    for ( i = 0; i < exported->parameterCount; ++i )
        if ( xl6IrExportValueAbiIsCallback(exported->parameterValueAbis[i]) )
            xl6OutputPrintf(out,
                "    xlir_export_arg_%lu = xlir_export_callback_wrap_t%u(xlir_input_%lu);\n"
                "    if (xlir_input_%lu != NULL && xlir_export_arg_%lu == NULL) goto xlir_export_bridge_failed;\n",
                (unsigned long)i, (unsigned int)exported->parameterTypes[i], (unsigned long)i,
                (unsigned long)i, (unsigned long)i);
    if ( mayRaise ) xl6OutputPrintf(out, "    %s result = xlir_fn_%u(", errorResultType,
                                    (unsigned int)target->symbol);
    else if ( exported->returnAbi != XL6_IR_EXPORT_RETURN_VOID ) {
        xl6OutputPuts(materialize ? "    body_result = " : "    pending_result = ", out);
        xl6OutputPrintf(out, "xlir_fn_%u(", (unsigned int)target->symbol);
    } else xl6OutputPrintf(out, "    xlir_fn_%u(", (unsigned int)target->symbol);
    for ( i = 0; i < exported->parameterCount; ++i ) {
        if ( i > 0 ) xl6OutputPuts(", ", out);
        if ( xl6IrExportValueAbiIsCallback(exported->parameterValueAbis[i]) ) {
            xl6OutputPrintf(out, "xlir_export_arg_%lu", (unsigned long)i);
            continue;
        }
        if ( exported->parameterValueAbis[i] !=
                 XL6_IR_EXPORT_VALUE_TYPE_REPRESENTATION )
            xl6OutputPrintf(out, "(%s)",
                xl6IrCgenType(module, exported->parameterTypes[i]));
        xl6OutputPrintf(out, "xlir_input_%lu", (unsigned long)i);
    }
    xl6OutputPuts(");\n", out);
    if (mayRaise) {
        xl6OutputPuts("    if (!result.ok) xl6ErrorScopeCaptureFailure(&entry);\n", out);
        if (materialize)
            xl6OutputPuts("    else {\n    body_result = result.value;\n", out);
        else if (exported->returnAbi != XL6_IR_EXPORT_RETURN_VOID)
            xl6OutputPuts("    else pending_result = result.value;\n", out);
    }
    if (materialize) {
        if (!xl6IrCgenEmitExportResultMaterialize(out, module, target->returnType,
                exported->returnType, "body_result", "pending_result"))
            return xl6IrCgenDiag(diags, module, exported->declaration, "XLIR export result bridge is unresolved");
        if (mayRaise) xl6OutputPuts("    }\n", out);
    }
    xl6OutputPuts("xlir_export_finish:\n", out);
    if ( hasCallback ) xl6IrCgenEmitExportCallbackCleanup(out, exported);
    if (materialize && !xl6IrCgenEmitExportBodyResultDrop(out, module, target->returnType))
        return xl6IrCgenDiag(diags, module, exported->declaration, "XLIR export body result has no release ABI");
    xl6OutputPuts(
        "    outcome = xl6ForeignCallEnd(&entry);\n"
        "    if (!outcome.ok) {\n", out);
    if (!xl6IrCgenEmitExportResultRollback(out, module, exported))
        return xl6IrCgenDiag(diags, module, exported->declaration,
            "XLIR export pending result has no verified release ABI");
    xl6OutputPuts("        xl6CodeScopeLeave(&execution);\n"
        "        xl6ErrorScopePropagateFailure(outcome);\n", out);
    xl6IrCgenEmitExportFailureReturn(out, exported, returnType);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    xl6CodeScopeLeave(&execution);\n", out);
    if ( exported->returnAbi != XL6_IR_EXPORT_RETURN_VOID ) {
        xl6OutputPuts("    return ", out);
        if ( exported->returnValueAbi != XL6_IR_EXPORT_VALUE_TYPE_REPRESENTATION )
            xl6OutputPrintf(out, "(%s)", returnType);
        xl6OutputPuts("pending_result;\n", out);
    }
    else xl6OutputPuts("    return;\n", out);
    if ( hasCallback ) {
        xl6OutputPuts("xlir_export_bridge_failed:\n", out);
        xl6OutputPuts("    if (xrtGetError() == NULL) xrtSetErrorTake(xlir_error_create(\"MemoryError\", \"C callback export bridge allocation failed\"));\n", out);
        xl6OutputPuts("    xl6ErrorScopeCaptureFailure(&entry);\n    goto xlir_export_finish;\n", out);
    }
    xl6OutputPuts("}\n\n", out);
    return !xl6OutputHasFailed(out);
}

static int xl6IrCgenEmitClosureInvokePrototype(
    XL6_Output* out, const XL6_IrModule* module,
    const XL6_IrClosureTarget* closure, XL6_DiagList* diags)
{
    const XL6_IrType* signature = closure != NULL
        ? xl6IrTypeGet(&module->types, closure->signatureType) : NULL;
    const char* resultType = NULL;
    size_t i;
    if ( closure == NULL || signature == NULL ||
         signature->kind != XL6_IR_TYPE_FUNCTION ||
         signature->cAbi != XL6_IR_C_ABI_CLOSURE_PTR ||
         (signature->paramCount != 0 &&
          (signature->parameterModes == NULL || signature->parameterAbis == NULL)) )
        return xl6IrCgenDiag(diags, module,
            closure != NULL ? closure->declaration : XL6_IR_NODE_INVALID,
            "XLIR C emitter closure thunk prototype ABI is unresolved");
    if ( signature->callableResultType != XL6_IR_TYPE_INVALID ) {
        resultType = xl6IrCgenType(module, signature->callableResultType);
        if ( resultType == NULL )
            return xl6IrCgenDiag(diags, module, closure->declaration,
                "XLIR C emitter closure thunk prototype result ABI is unresolved");
    }
    xl6OutputPrintf(out, "static bool xlir_closure_invoke_%u(void* environment",
                    (unsigned int)closure->targetSymbol);
    if ( resultType != NULL ) xl6OutputPrintf(out, ", %s* result", resultType);
    for ( i = 0; i < signature->paramCount; ++i ) {
        const char* type = xl6IrCgenType(module, signature->params[i]);
        if ( type == NULL )
            return xl6IrCgenDiag(diags, module, closure->declaration,
                "XLIR C emitter closure thunk prototype parameter ABI is unresolved");
        xl6OutputPrintf(out, ", %s%s a%lu", type,
            xl6IrCgenParameterAbiIsIndirect(signature->parameterAbis[i])
                ? "*" : "", (unsigned long)i);
    }
    xl6OutputPuts(");\n", out);
    return !xl6OutputHasFailed(out);
}

/* A raw callback entry is emitted only from the verifier-owned closure ABI.
 * It has no userdata parameter, so only a capture-free closure can publish one.
 * The explicit ABI distinguishes no-raise from a foreign-scope error bridge;
 * this emitter never infers effects by inspecting the callback body. */
static int xl6IrCgenEmitRawCallbackEntry(
    XL6_Output* out, const XL6_IrModule* module,
    const XL6_IrClosureTarget* closure, XL6_DiagList* diags)
{
    const XL6_IrType* signature = closure != NULL
        ? xl6IrTypeGet(&module->types, closure->signatureType) : NULL;
    const char* resultType;
    size_t parameter;
    if ( closure == NULL ||
         !xl6IrClosureRawCallbackAbiHasEntry(closure->rawCallbackAbi) ||
         closure->rawExecutionAbi != XL6_IR_CLOSURE_RAW_EXECUTION_RESIDENT_BORROWED_ACTIVITY_V1 ||
         closure->captureCount != 0 ||
         !xl6IrRawCCallbackSignatureSupported(
             module, closure->signatureType) || signature == NULL )
        return xl6IrCgenDiag(diags, module,
            closure != NULL ? closure->declaration : XL6_IR_NODE_INVALID,
            "XLIR C emitter raw C callback entry ABI is unresolved");
    resultType = signature->callableResultType == XL6_IR_TYPE_INVALID
        ? "void" : xl6IrCgenType(module, signature->callableResultType);
    if ( resultType == NULL )
        return xl6IrCgenDiag(diags, module, closure->declaration,
            "XLIR C emitter raw C callback result ABI is unresolved");
    xl6OutputPrintf(out, "static %s xlir_raw_callback_entry_%u(",
        resultType, (unsigned int)closure->targetSymbol);
    if ( signature->paramCount == 0 ) xl6OutputPuts("void", out);
    for ( parameter = 0; parameter < signature->paramCount; ++parameter ) {
        const char* parameterType = xl6IrCgenRawCallbackParameterType(
            module, signature->params[parameter]);
        if ( parameterType == NULL )
            return xl6IrCgenDiag(diags, module, closure->declaration,
                "XLIR C emitter raw C callback parameter ABI is unresolved");
        if ( parameter != 0 ) xl6OutputPuts(", ", out);
        xl6OutputPrintf(out, "%s a%lu", parameterType,
            (unsigned long)parameter);
    }
    xl6OutputPuts(")\n{\n", out);
    xl6OutputPuts("    XL6_CodeActivityV1 execution = {0};\n"
        "    xl6CodeActivityEnterV1(&execution, xlir_closure_owner);\n", out);
    if ( signature->callableResultType != XL6_IR_TYPE_INVALID )
        xl6OutputPrintf(out, "    %s result;\n    if (!xlir_closure_invoke_%u(NULL, &result",
            resultType, (unsigned int)closure->targetSymbol);
    else
        xl6OutputPrintf(out, "    if (!xlir_closure_invoke_%u(NULL",
            (unsigned int)closure->targetSymbol);
    for ( parameter = 0; parameter < signature->paramCount; ++parameter ) {
        const XL6_IrType* item = xl6IrTypeGet(
            &module->types, signature->params[parameter]);
        if ( item != NULL && item->cAbi == XL6_IR_C_ABI_MANAGED_STRING )
            xl6OutputPrintf(out,
                ", (xlir_string){ a%lu, a%lu != NULL ? strlen(a%lu) : 0u }",
                (unsigned long)parameter, (unsigned long)parameter,
                (unsigned long)parameter);
        else
            xl6OutputPrintf(out, ", a%lu", (unsigned long)parameter);
    }
    if ( closure->rawCallbackAbi == XL6_IR_CLOSURE_RAW_CALLBACK_FOREIGN_SCOPE_THUNK ) {
        xl6OutputPuts(")) {\n        xl6ForeignCallbackFail();\n"
            "        xl6CodeActivityLeaveV1(&execution);\n", out);
        if ( signature->callableResultType != XL6_IR_TYPE_INVALID )
            xl6OutputPrintf(out, "        return (%s)0;\n", resultType);
        else
            xl6OutputPuts("        return;\n", out);
        xl6OutputPuts("    }\n", out);
    } else
        xl6OutputPuts(")) abort();\n", out);
    xl6OutputPuts("    xl6CodeActivityLeaveV1(&execution);\n", out);
    if ( signature->callableResultType != XL6_IR_TYPE_INVALID )
        xl6OutputPuts("    return result;\n", out);
    xl6OutputPuts("}\n\n", out);
    return !xl6OutputHasFailed(out);
}

static int xl6IrCgenEmitClosureInvoke(
    XL6_Output* out, const XL6_IrModule* module, const XL6_IrClosureTarget* closure, XL6_DiagList* diags)
{
    const XL6_IrCallTarget* target;
    const XL6_IrFunction* function;
    const XL6_IrType* signature;
    const char* resultType = NULL;
    const char* carrierType = NULL;
    size_t i;
    if ( closure == NULL ||
         (function = xl6IrCgenFindFunction(module, closure->targetSymbol)) == NULL ||
         (signature = xl6IrTypeGet(&module->types, closure->signatureType)) == NULL ||
         signature->kind != XL6_IR_TYPE_FUNCTION || signature->cAbi != XL6_IR_C_ABI_CLOSURE_PTR ||
         (signature->paramCount != 0 &&
          (signature->parameterModes == NULL || signature->parameterAbis == NULL)) ||
         function->parameterCount != signature->paramCount ||
         (function->returnType != signature->callableResultType &&
          !(signature->returnCount == 1 && xl6IrCgenClosureTypesCompatible(
              module, function->returnType, signature->callableResultType))) ||
         (function->errorAbi != XL6_IR_ERROR_ABI_NO_RAISE &&
          function->errorAbi != XL6_IR_ERROR_ABI_MAY_RAISE) )
        return xl6IrCgenDiag(diags, module, closure != NULL ? closure->declaration : XL6_IR_NODE_INVALID,
                             "XLIR C emitter closure target has no complete thunk ABI");
    target = xl6IrCallTargetGet(
        module, closure->kind == XL6_IR_CLOSURE_TARGET_NAMED_ADAPTER
            ? closure->adaptedTargetSymbol : closure->targetSymbol);
    if ( target == NULL ||
         (target->kind != XL6_IR_CALL_TARGET_INTERNAL &&
          target->kind != XL6_IR_CALL_TARGET_INTERNAL_ASYNC &&
          target->kind != XL6_IR_CALL_TARGET_INTERNAL_GENERATOR &&
          target->kind != XL6_IR_CALL_TARGET_EXTERN_C) ||
         (closure->kind == XL6_IR_CLOSURE_TARGET_ANONYMOUS && function->errorAbi != target->errorAbi) ||
         (closure->kind == XL6_IR_CLOSURE_TARGET_ANONYMOUS &&
          (function->kind != XL6_IR_FUNCTION_USER ||
           target->parameterCount != signature->paramCount ||
           (target->returnType != signature->callableResultType &&
            !(signature->returnCount == 1 && xl6IrCgenClosureTypesCompatible(
                module, target->returnType, signature->callableResultType))))) ||
         (closure->kind == XL6_IR_CLOSURE_TARGET_NAMED_ADAPTER &&
          (function->kind != XL6_IR_FUNCTION_NAMED_ADAPTER ||
           function->errorAbi != XL6_IR_ERROR_ABI_MAY_RAISE ||
           closure->adaptedTargetSymbol == 0 || closure->captureCount != 0)) )
        return xl6IrCgenDiag(diags, module, closure->declaration,
                             "XLIR C emitter closure adapter role is unresolved");
    if ( signature->callableResultType != XL6_IR_TYPE_INVALID ) {
        resultType = xl6IrCgenType(module, signature->callableResultType);
        if ( resultType == NULL ) return xl6IrCgenDiag(diags, module, closure->declaration,
                                                         "XLIR C emitter closure thunk result ABI is unresolved");
    }
    if ( function->errorAbi == XL6_IR_ERROR_ABI_MAY_RAISE &&
         (carrierType = xl6IrCgenErrorResultType(module, function->returnType)) == NULL )
        return xl6IrCgenDiag(diags, module, closure->declaration,
                             "XLIR C emitter closure thunk error carrier ABI is unresolved");
    xl6OutputPrintf(out, "static bool xlir_closure_invoke_%u(void* environment",
                    (unsigned int)closure->targetSymbol);
    if ( resultType != NULL ) xl6OutputPrintf(out, ", %s* result", resultType);
    for ( i = 0; i < signature->paramCount; ++i ) {
        const char* type = xl6IrCgenType(module, signature->params[i]);
        if ( type == NULL ) return xl6IrCgenDiag(diags, module, closure->declaration,
                                                  "XLIR C emitter closure thunk parameter ABI is unresolved");
        xl6OutputPrintf(out, ", %s%s a%lu", type,
            xl6IrCgenParameterAbiIsIndirect(signature->parameterAbis[i])
                ? "*" : "", (unsigned long)i);
    }
    xl6OutputPuts(")\n{\n", out);
    if ( function->errorAbi == XL6_IR_ERROR_ABI_MAY_RAISE && resultType != NULL )
        xl6OutputPrintf(out, "    %s call = xlir_fn_%u(environment",
                        carrierType, (unsigned int)closure->targetSymbol);
    else if ( function->errorAbi == XL6_IR_ERROR_ABI_MAY_RAISE )
        xl6OutputPrintf(out, "    return xlir_fn_%u(environment",
                        (unsigned int)closure->targetSymbol);
    else if ( resultType != NULL )
        xl6OutputPrintf(out, "    *result = xlir_fn_%u(environment",
                        (unsigned int)closure->targetSymbol);
    else
        xl6OutputPrintf(out, "    xlir_fn_%u(environment",
                        (unsigned int)closure->targetSymbol);
    for ( i = 0; i < signature->paramCount; ++i ) {
        xl6OutputPrintf(out, ", a%lu", (unsigned long)i);
    }
    if ( function->errorAbi == XL6_IR_ERROR_ABI_MAY_RAISE && resultType == NULL )
        xl6OutputPuts(").ok;\n", out);
    else
        xl6OutputPuts(");\n", out);
    if ( function->errorAbi == XL6_IR_ERROR_ABI_MAY_RAISE && resultType != NULL )
        xl6OutputPuts("    if (!call.ok) return false;\n    *result = call.value;\n", out);
    xl6OutputPuts("    return true;\n}\n\n", out);
    return !xl6OutputHasFailed(out);
}
