int xl6IrCgenEmitCEx(XL6_Output* out, const XL6_IrModule* module,
                     XL6_DiagList* diags, const XL6_IrCgenOptions* options)
{
    XL6_IrCgenNeeds needs = {0};
    size_t i;
    if (out==NULL || !xl6IrCgenBuildNeeds(module,&needs,diags,options) || !xl6IrCgenVerifyEntryContract(module,diags,options)) return 0;
    if(options && options->emitMainWrapper && !options->entryContract->managed &&
        (module->classLayoutCount || module->exportCount || needs.needsClosureRuntime || needs.needsCellRuntime ||
         needs.needsModuleOwnershipViews || module->abi.readBoundaryAbi!=XL6_IR_MODULE_READ_BOUNDARY_NONE))
        return xl6IrCgenDiag(diags,module,XL6_IR_NODE_INVALID,"XLIR executable contract omits required graph lifetime");
    if ( xl6IrCgenNeedsXrtDeclarations(&needs) )
        xl6OutputPuts(
            "#define XLIR_XRT_DECLARATIONS 1\n"
            "#include <__xl_runtime/xruntime_decl.h>\n", out);
    xl6OutputPuts("#include <stdbool.h>\n#include <stddef.h>\n#include <stdint.h>\n#include <limits.h>\n#include <stdio.h>\n#include <stdlib.h>\n#include <string.h>\n#include <wchar.h>\n", out);
    if(needs.needsCOwnedRuntime)
        xl6OutputPuts("#include <__xl_runtime/xl6_c_owned.h>\n",out);
    if(needs.needsCOwnedPrintRuntime)
        xl6OutputPuts("#include <__xl_runtime/xl6_c_print.h>\n",out);
    if (needs.needsLogAdapterDeclarations)
        xl6OutputPuts("#include <__xl_runtime/xl6_log.h>\n", out);
    if (needs.needsTextAdapterDeclarations)
        xl6OutputPuts("#include <__xl_runtime/xl6_text_value.h>\n", out);
    /* SDK declarations follow the same resident component identity as linkage. */
    {
        int foreign=0,copy=0;
        for(size_t t=0;t<module->callTargetCount;++t) {
            XL6_ArtifactRuntimeId owner=xl6ArtifactRuntimeSymbolOwner(module->callTargets[t].cName);
            foreign |= owner==XL6_ARTIFACT_FOREIGN_RESOURCE;
            copy |= owner==XL6_ARTIFACT_VALUE_COPY_COLLECTIONS;
        }
        if(foreign)xl6OutputPuts("#include <__xl_runtime/xl6_foreign_resource.h>\n",out);
        if(copy)xl6OutputPuts("#include <__xl_runtime/xl6_value_copy.h>\n",out);
    }
    xl6IrCgenEmitPublicLinkage(out);
    xl6IrCgenEmitCodeOwnerBinding(out, module);
    if(module->abi.executionOwnershipAbi==XL6_IR_MODULE_EXECUTION_OWNERSHIP_RESIDENT_SOURCE_V1)
        xl6OutputPuts("#include <__xl_runtime/xl6_execution_module_abi.h>\n"
            "#include <__xl_runtime/xl6_execution_language.h>\n"
            "static XL6_ExecutionSourceV1* xlir_execution_source;\n",out);
    if (module->abi.continuationAbi == XL6_IR_MODULE_CONTINUATION_FIXED_WORKERS_V1) {
        xl6OutputPuts(
            "#define XLIR_RUNTIME_CONTINUATION 1\n"
            "#include <__xl_runtime/xl6_continuation_executor.h>\n"
            "#include <__xl_runtime/xl6_runtime_services.h>\n"
            "#include <__xl_runtime/xl6_continuation_future.h>\n"
            "/* Deployment resource policy, not a language type/effect inference. */\n"
            "#ifndef XLIR_CONTINUATION_CAPACITY\n#define XLIR_CONTINUATION_CAPACITY 256\n#endif\n"
            "#ifndef XLIR_CONTINUATION_WORKERS\n#define XLIR_CONTINUATION_WORKERS 0\n#endif\n"
            "static XL6_ContinuationExecutor* xlir_continuation_executor;\n",out);
        if(module->abi.deadlineOwnershipAbi==XL6_IR_MODULE_DEADLINE_OWNERSHIP_RESIDENT_BINDINGS_JOIN_V1)
            xl6OutputPuts("#include <__xl_runtime/xl6_deadline_module_abi.h>\n"
                "#ifndef XLIR_DEADLINE_CAPACITY\n#define XLIR_DEADLINE_CAPACITY XLIR_CONTINUATION_CAPACITY\n#endif\n"
                "static XL6_DeadlineServiceV1* xlir_deadline_source;\n",out);
        xl6OutputPuts(
            "static int xlir_continuation_quiesce(bool wait)\n"
            "{\n",out);
        if(module->abi.deadlineOwnershipAbi==XL6_IR_MODULE_DEADLINE_OWNERSHIP_RESIDENT_BINDINGS_JOIN_V1)
            xl6OutputPuts("    xl6DeadlineServiceCloseV1(xlir_deadline_source);\n",out);
        xl6OutputPuts(
            "    xl6ContinuationExecutorClose(xlir_continuation_executor);\n"
            "    if (!(wait ? xl6ContinuationExecutorDestroyCompleted(xlir_continuation_executor)\n"
            "               : xl6ContinuationExecutorDestroy(xlir_continuation_executor))) return 0;\n"
            "    xlir_continuation_executor = NULL;\n",out);
        if(module->abi.deadlineOwnershipAbi==XL6_IR_MODULE_DEADLINE_OWNERSHIP_RESIDENT_BINDINGS_JOIN_V1)
            xl6OutputPuts("    if (!(wait ? xl6DeadlineServiceDestroyCompletedV1(xlir_deadline_source)\n"
                "               : xl6DeadlineServiceDestroyV1(xlir_deadline_source))) return 0;\n"
                "    xlir_deadline_source = NULL;\n",out);
        xl6OutputPuts("    return 1;\n}\n",out);
    }
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
                   "/* Language string storage has exact UTF-8 bytes and Data[Size] as a NUL sentinel. */\n"
                   "typedef xstrview xlir_string;\n"
                   "/* Canonical C string literals have a compile-time byte extent, including embedded NUL. */\n"
                   "#define xlir_literal_view(literal) ((xlir_string_view){ (literal), sizeof(literal) - 1u })\n"
                   "typedef struct xlir_closure xlir_closure;\n"
                   "typedef struct xlir_generator xlir_generator;\n", out);
    if ( options != NULL && options->debugMode ) {
        xl6OutputPuts("#include <string.h>\n", out);
        if ( !xl6IrCgenEmitDebugRuntime(out) ) return 0;
    }
    if ( !xl6IrCgenEmitUnionTypes(out, module, NULL, diags, NULL, NULL) ) return 0;
    if ( !xl6IrCgenEmitProductTypes(out, module, NULL, diags) ) return 0;
    /* Adapters as well as function bodies consume the same product drop ABI. */
    for ( i = 0; i < module->types.count; ++i ) {
        XL6_IrTypeId type = (XL6_IrTypeId)(i + 1);
        if ( xl6IrCgenProductDropNeeded(module, type) &&
             !xl6IrCgenEmitProductDropPrototype(
                 out, module, type, diags) ) return 0;
    }
    if ( !xl6IrCgenEmitStructuralErrorResultTypes(out, module, diags,needs.needsCOwnedRuntime) ) return 0;
    if ( !xl6IrCgenEmitRuntimePrelude(out, module, &needs, diags) ) return 0;
    if ( !xl6IrCgenEmitRuntimeSupport(out, module, &needs, diags) ) return 0;
    if ( needs.needsXlangRuntime ) xl6EmitXlangRuntimeComponent(out);
    if ( !xl6IrCgenEmitXValueCoreRuntime(out, module, &needs, diags) ) return 0;
    if ( !xl6IrCgenEmitXValueContainerRuntime(out, module, &needs, diags) ) return 0;
    if ( !xl6IrCgenEmitXValueOperationRuntime(out, module, &needs, diags) ) return 0;
    if ( !xl6IrCgenEmitStdlibBridgeRuntime(out, module, &needs, diags) ) return 0;
    if ( !xl6IrCgenEmitStringBridgeRuntime(out, &needs) ) return 0;
    if ( needs.needsFileBridgeRuntime )
        xl6CgenEmitFileRuntime(
            out, xl6IrCgenNeedsXrtDeclarations(&needs));
    if ( needs.needsBufferBridgeRuntime )
        xl6CgenEmitBufferRuntime(
            out, needs.needsFileBridgeRuntime,
            xl6IrCgenNeedsXrtDeclarations(&needs));
    if ( needs.needsHttp1Runtime )
        xl6CgenEmitHttp1Runtime(out);
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
            needs.needsWebSocketStreamTlsRuntime);
    if ( !xl6IrCgenEmitNetTcpRuntime(out, module, &needs, diags) ) return 0;
    if ( !xl6IrCgenEmitNetTlsRuntime(out, module, &needs, diags) ) return 0;
    if ( !xl6IrCgenEmitNetDatagramRuntime(out, module, &needs, diags) ) return 0;
    if ( !xl6IrCgenEmitAsyncAndCallableRuntime(out, module, &needs, diags) ) return 0;
    if ( !xl6IrCgenEmitResourceRuntime(out, module, &needs, diags) ) return 0;
    /* Dynamic adapters consume physical bridge definitions, not normalized
     * language prototypes. Emit them after every selected bridge family. */
    if ( needs.needsDynamicMethodRuntime &&
         !xl6IrCgenEmitDynamicMethodRuntime(out, module, diags) ) return 0;
    if ( !xl6IrCgenEmitModuleDefinitions(
            out, module, &needs, options, diags) ) return 0;
    if ( !xl6IrCgenEmitModuleOwnershipTrace(out, module, &needs) ) return 0;
    if ( !xl6IrCgenEmitSyntheticScriptEntry(out, module,
            options != NULL && options->emitScriptEntry) ) return 0;
    return xl6IrCgenEmitHostBindings(out, module,
        options != NULL && options->emitScriptEntry,
        options != NULL && options->emitMainWrapper,
        needs.needsErrorRuntime,
        module->abi.networkServiceAbi != XL6_IR_MODULE_NETWORK_SERVICE_NONE,
        options!=NULL ? options->entryContract : NULL,
        diags);
}

int xl6IrCgenEmitC(XL6_Output* out, const XL6_IrModule* module, XL6_DiagList* diags)
{
    return xl6IrCgenEmitCEx(out, module, diags, NULL);
}
