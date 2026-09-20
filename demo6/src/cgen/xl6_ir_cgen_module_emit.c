int xl6IrCgenEmitCEx(XL6_Output* out, const XL6_IrModule* module,
                     XL6_DiagList* diags, const XL6_IrCgenOptions* options)
{
    XL6_IrCgenNeeds needs = {0};
    size_t i;
    if (out==NULL || !xl6IrCgenBuildNeeds(module,&needs,diags,options)) return 0;
    if ( xl6IrCgenNeedsXrtDeclarations(&needs) )
        xl6OutputPuts(
            "#define XLIR_XRT_DECLARATIONS 1\n"
            "#include <__xl_runtime/xruntime_decl.h>\n", out);
    xl6OutputPuts("#include <stdbool.h>\n#include <stddef.h>\n#include <stdint.h>\n#include <stdio.h>\n#include <stdlib.h>\n#include <wchar.h>\n", out);
    xl6IrCgenEmitPublicLinkage(out);
    xl6IrCgenEmitCodeOwnerBinding(out, module);
    if (module->abi.continuationAbi == XL6_IR_MODULE_CONTINUATION_GUARANTEED_WORKERS)
        xl6OutputPuts(
            "#define XLIR_RUNTIME_CONTINUATION 1\n"
            "#include <__xl_runtime/xl6_continuation_executor.h>\n"
            "#include <__xl_runtime/xl6_continuation_future.h>\n"
            "/* Deployment resource policy, not a language type/effect inference. */\n"
            "#ifndef XLIR_CONTINUATION_CAPACITY\n#define XLIR_CONTINUATION_CAPACITY 256\n#endif\n"
            "static XL6_ContinuationExecutor* xlir_continuation_executor;\n"
            "static int xlir_continuation_quiesce(void)\n"
            "{\n"
            "    xl6ContinuationExecutorClose(xlir_continuation_executor);\n"
            "    if (!xl6ContinuationExecutorDestroy(xlir_continuation_executor)) return 0;\n"
            "    xlir_continuation_executor = NULL; return 1;\n"
            "}\n", out);
    if ( !xl6IrCgenNeedsXrtDeclarations(&needs) &&
         (needs.needsXValueRuntime ||
         module->abi.entryKind == XL6_IR_MODULE_ENTRY_XLANG_MAIN ||
         (options != NULL && options->emitScriptEntry)) )
        xl6OutputPuts("typedef struct xvalue xvalue;\n", out);
    if ( !xl6IrCgenNeedsXrtDeclarations(&needs) &&
         (needs.needsErrorRuntime ||
         module->abi.entryKind == XL6_IR_MODULE_ENTRY_XLANG_MAIN ||
         (options != NULL && options->emitScriptEntry)) )
        xl6OutputPuts("typedef struct xerror xerror;\n", out);
    if ( !xl6IrCgenNeedsXrtDeclarations(&needs) )
        xl6OutputPuts("typedef struct xstrview { const char* Data; size_t Size; } xstrview;\n", out);
    xl6OutputPuts("typedef xstrview xlir_string_view;\n"
                   "typedef struct xlir_closure xlir_closure;\n"
                   "typedef struct xlir_generator xlir_generator;\n", out);
    if ( !xl6IrCgenEmitUnionTypes(out, module, diags) ) return 0;
    if ( !xl6IrCgenEmitProductTypes(out, module, diags) ) return 0;
    /* Adapters as well as function bodies consume the same product drop ABI. */
    for ( i = 0; i < module->types.count; ++i ) {
        XL6_IrTypeId type = (XL6_IrTypeId)(i + 1);
        if ( xl6IrCgenProductDropNeeded(module, type) &&
             !xl6IrCgenEmitProductDropPrototype(
                 out, module, type, diags) ) return 0;
    }
    if ( !xl6IrCgenEmitStructuralErrorResultTypes(out, module, diags) ) return 0;
    if ( !xl6IrCgenEmitRuntimePrelude(out, module, &needs, diags) ) return 0;
    if ( !xl6IrCgenEmitRuntimeSupport(out, module, &needs, diags) ) return 0;
    if ( !xl6IrCgenEmitXValueCoreRuntime(out, module, &needs, diags) ) return 0;
    if ( !xl6IrCgenEmitXValueContainerRuntime(out, module, &needs, diags) ) return 0;
    if ( !xl6IrCgenEmitXValueOperationRuntime(out, module, &needs, diags) ) return 0;
    if ( !xl6IrCgenEmitTemplateRuntime(out, &needs) ) return 0;
    if ( !xl6IrCgenEmitStdlibBridgeRuntime(out, &needs) ) return 0;
    if ( !xl6IrCgenEmitStringBridgeRuntime(out, &needs) ) return 0;
    if ( needs.needsFileBridgeRuntime )
        xl6CgenEmitFileRuntime(
            out, 1, xl6IrCgenNeedsXrtDeclarations(&needs));
    if ( needs.needsBufferBridgeRuntime )
        xl6CgenEmitBufferRuntime(
            out, needs.needsFileBridgeRuntime, 1,
            xl6IrCgenNeedsXrtDeclarations(&needs));
    if ( needs.needsHttp1Runtime )
        xl6CgenEmitHttp1Runtime(out, 1);
    if ( needs.needsWebSocketFrameRuntime ||
         needs.needsWebSocketHandshakeRuntime ||
         needs.needsWebSocketUpgradeRuntime ||
         needs.needsWebSocketStreamConfigRuntime ||
         needs.needsWebSocketStreamRuntime ||
         needs.needsWebSocketStreamTlsRuntime )
        xl6CgenEmitWebSocketProtocolRuntime(
            out,
            needs.needsWebSocketFrameRuntime,
            needs.needsWebSocketHandshakeRuntime,
            needs.needsWebSocketUpgradeRuntime,
            needs.needsWebSocketStreamConfigRuntime,
            needs.needsWebSocketStreamRuntime,
            needs.needsWebSocketStreamTlsRuntime,
            1);
    if ( !xl6IrCgenEmitNetTcpRuntime(out, module, &needs, diags) ) return 0;
    if ( !xl6IrCgenEmitNetTlsRuntime(out, module, &needs, diags) ) return 0;
    if ( !xl6IrCgenEmitNetDatagramRuntime(out, module, &needs, diags) ) return 0;
    if ( !xl6IrCgenEmitAsyncAndCallableRuntime(out, module, &needs, diags) ) return 0;
    if ( !xl6IrCgenEmitResourceRuntime(out, module, &needs, diags) ) return 0;
    if ( !xl6IrCgenEmitModuleDefinitions(out, module, &needs, diags) ) return 0;
    if ( !xl6IrCgenEmitModuleOwnershipTrace(out, module, &needs) ) return 0;
    if ( !xl6IrCgenEmitSyntheticScriptEntry(out, module,
            options != NULL && options->emitScriptEntry) ) return 0;
    return xl6IrCgenEmitHostBindings(out, module,
        options != NULL && options->emitScriptEntry,
        options != NULL && options->emitMainWrapper,
        needs.needsErrorRuntime,
        module->abi.networkServiceAbi != XL6_IR_MODULE_NETWORK_SERVICE_NONE,
        diags);
}

int xl6IrCgenEmitC(XL6_Output* out, const XL6_IrModule* module, XL6_DiagList* diags)
{
    return xl6IrCgenEmitCEx(out, module, diags, NULL);
}
