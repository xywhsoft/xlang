/* Physical adapters selected only by verified export value ABIs. */
/* One public shared-runtime declaration contract for generated C and headers. */
static void xl6IrCgenEmitForeignErrorDeclarations(XL6_Output* out)
{
    xl6OutputPuts(
        "#ifndef XL6_FOREIGN_ERROR_ABI\n#define XL6_FOREIGN_ERROR_ABI\n"
        "typedef struct xerror xerror;\n"
        "typedef struct xl6_foreign_error_scope { struct xl6_foreign_error_scope* previous; xerror* pending; xerror* previous_diagnostic; bool failed; } xl6_foreign_error_scope;\n"
        "typedef struct xl6_foreign_error_result { bool ok; xerror* error; } xl6_foreign_error_result;\n"
        "typedef xl6_foreign_error_scope xl6_error_scope;\n"
        "typedef xl6_foreign_error_result xl6_error_result;\n"
        "extern bool xl6ErrorScopeBegin(xl6_error_scope*);\n"
        "extern bool xl6ErrorScopeBeginCleanup(xl6_error_scope*, bool);\n"
        "extern xl6_error_result xl6ErrorScopeEnd(xl6_error_scope*);\n"
        "extern bool xl6ForeignCallBegin(xl6_foreign_error_scope*);\n"
        "extern xl6_foreign_error_result xl6ForeignCallEnd(xl6_foreign_error_scope*);\n"
        "extern void xl6ErrorScopeCaptureFailure(xl6_error_scope*);\n"
        "extern void xl6ErrorScopePropagateFailure(xl6_error_result);\n"
        "extern void xl6ForeignCallbackFail(void);\n#endif\n", out);
}

static int xl6IrCgenExportCallbackNeeded(
    const XL6_IrModule* module, XL6_IrTypeId type)
{
    size_t i, p;
    for ( i = 0; i < module->exportCount; ++i )
        for ( p = 0; p < module->exports[i].parameterCount; ++p )
            if ( xl6IrExportValueAbiIsCallback(module->exports[i].parameterValueAbis[p]) &&
                 module->exports[i].parameterTypes[p] == type ) return 1;
    return 0;
}

static int xl6IrCgenCallbackTypeNeeded(
    const XL6_IrModule* module, XL6_IrTypeId type, int publicHeader)
{
    size_t i, p;
    if ( xl6IrCgenExportCallbackNeeded(module, type) ) return 1;
    for ( i = 0; i < module->callTargetCount; ++i ) {
        const XL6_IrCallTarget* target = &module->callTargets[i];
        if ( target->reachability == XL6_IR_REACHABILITY_UNREACHABLE ||
             (publicHeader &&
              ((target->kind != XL6_IR_CALL_TARGET_EXTERN_C &&
                target->kind != XL6_IR_CALL_TARGET_NATIVE_FACTORY) ||
               target->runtimeBridge != XL6_IR_RUNTIME_BRIDGE_NONE)) )
            continue;
        for ( p = 0; p < target->parameterCount; ++p )
            if ( target->parameterPhysicalAbis[p] ==
                     XL6_IR_CALL_PARAMETER_PHYSICAL_RAW_C_CALLBACK &&
                 target->parameterTypes[p] == type ) return 1;
    }
    return 0;
}

static int xl6IrCgenEmitCallbackTypes(
    XL6_Output* out, const XL6_IrModule* module, int publicHeader,
    XL6_DiagList* diags)
{
    size_t t;
    for ( t = 1; t <= module->types.count; ++t ) {
        if ( xl6IrCgenCallbackTypeNeeded(
                 module, (XL6_IrTypeId)t, publicHeader) &&
             !xl6IrCgenEmitRawCallbackTypedef(
                out, module, (XL6_IrTypeId)t, diags) ) return 0;
    }
    return !xl6OutputHasFailed(out);
}

static int xl6IrCgenEmitExportParameter(
    XL6_Output* out, const XL6_IrModule* module, const XL6_IrExport* exported,
    size_t parameter, int implementation, XL6_DiagList* diags)
{
    if ( xl6IrExportValueAbiIsCallback(exported->parameterValueAbis[parameter]) )
        xl6OutputPrintf(out, "xlir_raw_callback_t%u ",
            (unsigned int)exported->parameterTypes[parameter]);
    else {
        const char* type = xl6IrCgenExportValueType(module,
            exported->parameterTypes[parameter], exported->parameterValueAbis[parameter]);
        if ( type == NULL ) return xl6IrCgenDiag(diags, module, exported->declaration,
            "XLIR C export parameter representation is unresolved");
        xl6OutputPrintf(out, "%s%s ", type,
            xl6IrCgenParameterAbiIsIndirect(exported->parameterAbis[parameter]) ? "*" : "");
    }
    if ( implementation ) xl6OutputPrintf(out, "xlir_input_%lu", (unsigned long)parameter);
    else xl6OutputPuts(exported->parameterNames[parameter], out);
    return !xl6OutputHasFailed(out);
}

static int xl6IrCgenEmitExportCallbackAdapters(
    XL6_Output* out, const XL6_IrModule* module, XL6_DiagList* diags)
{
    size_t t, p;
    for ( t = 1; t <= module->types.count; ++t ) {
        const XL6_IrType* signature;
        const XL6_IrType* resultType;
        const char* result;
        const char* release = NULL;
        if ( !xl6IrCgenExportCallbackNeeded(module, (XL6_IrTypeId)t) ) continue;
        if ( xl6IrExportCallbackSignatureType(module, (XL6_IrTypeId)t) == XL6_IR_TYPE_INVALID )
            return xl6IrCgenDiag(diags, module, XL6_IR_NODE_INVALID,
                "XLIR export callback adapter has no verified signature");
        signature = xl6IrTypeGet(&module->types,
            xl6IrExportCallbackSignatureType(module, (XL6_IrTypeId)t));
        result = signature->callableResultType == XL6_IR_TYPE_INVALID ? NULL :
            xl6IrCgenType(module, signature->callableResultType);
        resultType = xl6IrTypeGet(&module->types, signature->callableResultType);
        if ( resultType != NULL && resultType->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED )
            release = resultType->cAbi == XL6_IR_C_ABI_MANAGED_STRING ? "xrtFree" : "xrtValueRelease";
        if (release != NULL)
            xl6OutputPrintf(out,
                "static xl6_foreign_error_result xlir_export_callback_drop_t%lu(%s* owner)\n"
                "{\n"
                "    %s owned = *owner; xl6_foreign_error_scope cleanup = {0};\n"
                "    *owner = NULL;\n"
                "    if (!xl6ForeignCallBegin(&cleanup)) abort();\n"
                "    %s(owned);\n"
                "    return xl6ForeignCallEnd(&cleanup);\n"
                "}\n",
                (unsigned long)t, result, result, release);
        xl6OutputPrintf(out,
            "typedef struct xlir_export_callback_t%lu { xlir_raw_callback_t%lu callback; } xlir_export_callback_t%lu;\n"
            "static bool xlir_export_callback_invoke_t%lu(void* environment",
            (unsigned long)t, (unsigned long)t, (unsigned long)t, (unsigned long)t);
        if ( result != NULL ) xl6OutputPrintf(out, ", %s* result", result);
        for ( p = 0; p < signature->paramCount; ++p )
            xl6OutputPrintf(out, ", %s a%lu", xl6IrCgenType(module, signature->params[p]), (unsigned long)p);
        xl6OutputPrintf(out, ")\n{\n    xlir_raw_callback_t%lu callback = *(xlir_raw_callback_t%lu*)environment;\n",
            (unsigned long)t, (unsigned long)t);
        xl6OutputPuts(
            "    xl6_foreign_error_scope invocation = {0}; xl6_foreign_error_result outcome;\n"
            "    if (!xl6ForeignCallBegin(&invocation)) return false;\n    ", out);
        if ( result != NULL ) xl6OutputPrintf(out, "%s value = ", result);
        xl6OutputPuts("callback(", out);
        for ( p = 0; p < signature->paramCount; ++p ) {
            if ( p != 0 ) xl6OutputPuts(", ", out);
            xl6OutputPrintf(out, "a%lu", (unsigned long)p);
        }
        xl6OutputPuts(");\n", out);
        xl6OutputPuts("    outcome = xl6ForeignCallEnd(&invocation);\n", out);
        if ( release != NULL ) {
            xl6OutputPrintf(out,
                "    if (!outcome.ok) {\n"
                "        xl6_foreign_error_result discarded = xlir_export_callback_drop_t%lu(&value);\n"
                "        xrtErrorFree(discarded.error); xrtSetErrorTake(outcome.error); return false;\n"
                "    }\n"
                "    if (result != NULL) *result = value; else {\n"
                "        outcome = xlir_export_callback_drop_t%lu(&value);\n"
                "        if (!outcome.ok) { xrtSetErrorTake(outcome.error); return false; }\n"
                "    }\n", (unsigned long)t, (unsigned long)t);
        } else {
            xl6OutputPuts("    if (!outcome.ok) { xrtSetErrorTake(outcome.error); return false; }\n", out);
            if ( result != NULL ) xl6OutputPuts("    if (result != NULL) *result = value;\n", out);
        }
        xl6OutputPuts("    return true;\n}\n", out);
        /* The native callback is a borrowed code address, not an owning
         * context. Its adapter storage is a verified trivial field. */
        xl6OutputPrintf(out, "static const XL6_ClosureCaptureSlot xlir_export_callback_slot_t%lu = {\n"
            "    XL6_CLOSURE_CAPTURE_TRIVIAL, offsetof(xlir_export_callback_t%lu, callback), sizeof(xlir_raw_callback_t%lu)};\n",
            (unsigned long)t, (unsigned long)t, (unsigned long)t);
        xl6OutputPrintf(out, "static const XL6_ClosureEnvironmentV1 xlir_export_callback_layout_t%lu = {\n"
            "    sizeof(XL6_ClosureEnvironmentV1), sizeof(xlir_export_callback_t%lu), 1, &xlir_export_callback_slot_t%lu};\n",
            (unsigned long)t, (unsigned long)t, (unsigned long)t);
        xl6OutputPrintf(out,
            "static xlir_closure* xlir_export_callback_wrap_t%lu(xlir_raw_callback_t%lu callback)\n"
            "{\n"
            "    xlir_export_callback_t%lu* bridge; xlir_closure* closure;\n"
            "    if (callback == NULL) return NULL;\n"
            "    bridge = (xlir_export_callback_t%lu*)xrtMalloc(sizeof(*bridge));\n"
            "    if (bridge == NULL) return NULL;\n"
            "    bridge->callback = callback;\n"
            "    closure = xlir_closure_new((void (*)(void))xlir_export_callback_invoke_t%lu,\n"
            "        (void (*)(void))callback, bridge, &xlir_export_callback_layout_t%lu, NULL, NULL, %lu);\n"
            "    if (closure == NULL) xrtFree(bridge);\n"
            "    return closure;\n"
            "}\n\n",
            (unsigned long)t, (unsigned long)t, (unsigned long)t, (unsigned long)t,
            (unsigned long)t, (unsigned long)t, (unsigned long)signature->paramCount);
    }
    return !xl6OutputHasFailed(out);
}

static void xl6IrCgenEmitExportCallbackCleanup(XL6_Output* out, const XL6_IrExport* exported)
{
    size_t p = exported->parameterCount;
    while ( p != 0 ) {
        --p;
        if ( xl6IrExportValueAbiIsCallback(exported->parameterValueAbis[p]) )
            xl6OutputPrintf(out,
                "    { xlir_closure* owned = xlir_export_arg_%lu; xlir_export_arg_%lu = NULL; xlir_closure_release(owned); }\n",
                (unsigned long)p, (unsigned long)p);
    }
}
