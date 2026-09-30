/* Assemble storage, closure, dispatcher, machine and export definitions. */
static int xl6IrCgenEmitModuleDefinitions(
    XL6_Output* out, const XL6_IrModule* module,
    const XL6_IrCgenNeeds* needs, const XL6_IrCgenOptions* options,
    XL6_DiagList* diags)
{
    size_t i;
    size_t parameter;
    if ( (needs->needsPersistenceFormats & XL6_IR_PERSIST_FORMAT_JSON) != 0 &&
         !xl6IrCgenEmitPersistenceHelpers(
             out, module, XL6_IR_PERSIST_FORMAT_JSON, diags) ) return 0;
    if ( (needs->needsPersistenceFormats & XL6_IR_PERSIST_FORMAT_XLON) != 0 &&
         !xl6IrCgenEmitPersistenceHelpers(
             out, module, XL6_IR_PERSIST_FORMAT_XLON, diags) ) return 0;
    for ( i = 0; i < module->types.count; ++i ) {
        XL6_IrTypeId type = (XL6_IrTypeId)(i + 1);
        if ( xl6IrCgenProductDropNeeded(module, type) &&
             !xl6IrCgenEmitProductDrop(
                 out, module, type, diags) ) return 0;
    }
    for ( i = 0; i < module->storageCount; ++i ) {
        const XL6_IrStorage* storage = &module->storages[i];
        if ( storage->kind == XL6_IR_STORAGE_GLOBAL ) {
            const XL6_IrGlobalExport* exported = xl6IrGlobalExportGet(module, storage->symbol);
            const char* type = xl6IrCgenType(module, storage->cellType != XL6_IR_TYPE_INVALID
                                                       ? storage->cellType : storage->type);
            if ( type == NULL ) return xl6IrCgenDiag(diags, module, storage->declaration, "XLIR C emitter does not support global storage ABI");
            if ( exported != NULL ) {
                xl6OutputPuts("XLIR_C_API ", out);
                if ( exported->mutability == XL6_IR_GLOBAL_CONST ) xl6OutputPuts("const ", out);
                xl6OutputPrintf(out, "%s ", type);
                xl6IrCgenPutStorageName(out, module, storage->symbol);
                xl6OutputPrintf(out, " = %s;\n", exported->initializer);
            } else {
                xl6OutputPrintf(out, "static %s s%u;\n", type, (unsigned int)storage->symbol);
            }
        } else if ( storage->kind == XL6_IR_STORAGE_EXTERN_GLOBAL ) {
            const XL6_IrExternGlobal* imported = xl6IrExternGlobalGet(module, storage->symbol);
            if ( !xl6IrCgenEmitExternGlobal(out, module, imported, diags) ) return 0;
        }
    }
    if ( module->storageCount != 0 ) xl6OutputPuts("\n", out);
    /* Keyword binding is value-specific metadata.  Emit it from the lowered
     * closure registry once, before any function that may construct the
     * corresponding closure; Cgen never recovers names from source nodes. */
    for ( i = 0; i < module->closureTargetCount; ++i ) {
        const XL6_IrClosureTarget* closure = &module->closureTargets[i];
        size_t parameter;
        if ( closure->reachability == XL6_IR_REACHABILITY_UNREACHABLE ||
             closure->callParameterCount == 0 ) continue;
        if ( closure->callParameterNames == NULL )
            return xl6IrCgenDiag(diags, module, closure->declaration,
                "XLIR C emitter closure call-frame parameter metadata is unresolved");
        if ( closure->callParameterDefaultAbis == NULL ||
             closure->callParameterDefaultLiterals == NULL )
            return xl6IrCgenDiag(diags, module, closure->declaration,
                "XLIR C emitter closure call-frame default metadata is unresolved");
        xl6OutputPrintf(out,
            "static const char* const xlir_closure_call_names_%u[%lu] = {",
            (unsigned int)closure->targetSymbol,
            (unsigned long)closure->callParameterCount);
        for ( parameter = 0; parameter < closure->callParameterCount; ++parameter ) {
            if ( closure->callParameterNames[parameter] == NULL )
                return xl6IrCgenDiag(diags, module, closure->declaration,
                    "XLIR C emitter closure call-frame parameter name is unresolved");
            if ( parameter != 0 ) xl6OutputPuts(", ", out);
            xl6IrCgenEmitCStringLiteral(out, closure->callParameterNames[parameter]);
        }
        xl6OutputPuts("};\n", out);
        xl6OutputPrintf(out,
            "static const xlir_call_default xlir_closure_call_defaults_%u[%lu] = {",
            (unsigned int)closure->targetSymbol,
            (unsigned long)closure->callParameterCount);
        for ( parameter = 0; parameter < closure->callParameterCount; ++parameter ) {
            XL6_IrDynamicMethodDefaultAbi abi =
                closure->callParameterDefaultAbis[parameter];
            const char* literal =
                closure->callParameterDefaultLiterals[parameter];
            if ( parameter != 0 ) xl6OutputPuts(", ", out);
            xl6OutputPrintf(out, "{%d, ", (int)abi);
            if ( abi == XL6_IR_DYNAMIC_METHOD_DEFAULT_BOOL )
                xl6OutputPuts(literal, out);
            else
                xl6OutputPuts("false", out);
            xl6OutputPuts(", ", out);
            if ( abi == XL6_IR_DYNAMIC_METHOD_DEFAULT_I64 ||
                 abi == XL6_IR_DYNAMIC_METHOD_DEFAULT_TIME )
                xl6OutputPuts(literal, out);
            else
                xl6OutputPuts("INT64_C(0)", out);
            xl6OutputPuts(", ", out);
            if ( abi == XL6_IR_DYNAMIC_METHOD_DEFAULT_U64 )
                xl6OutputPuts(literal, out);
            else
                xl6OutputPuts("UINT64_C(0)", out);
            xl6OutputPuts(", ", out);
            if ( abi == XL6_IR_DYNAMIC_METHOD_DEFAULT_F64 )
                xl6OutputPuts(literal, out);
            else
                xl6OutputPuts("0.0", out);
            xl6OutputPuts(", ", out);
            if ( abi == XL6_IR_DYNAMIC_METHOD_DEFAULT_STRING )
                xl6OutputPuts(literal, out);
            else
                xl6OutputPuts("NULL", out);
            xl6OutputPuts(", ", out);
            if ( abi == XL6_IR_DYNAMIC_METHOD_DEFAULT_STRING )
                xl6OutputPrintf(out, "sizeof(%s) - 1u", literal);
            else
                xl6OutputPuts("0u", out);
            xl6OutputPuts(", ", out);
            if ( abi == XL6_IR_DYNAMIC_METHOD_DEFAULT_CHAR )
                xl6OutputPuts(literal, out);
            else
                xl6OutputPuts("UINT32_C(0)", out);
            xl6OutputPuts("}", out);
        }
        xl6OutputPuts("};\n", out);
    }
    if ( module->closureTargetCount != 0 ) xl6OutputPuts("\n", out);
    for ( i = 0; i < module->closureTargetCount; ++i ) {
        const XL6_IrClosureTarget* closure = &module->closureTargets[i];
        size_t capture;
        if ( closure->reachability == XL6_IR_REACHABILITY_UNREACHABLE ||
             closure->captureCount == 0 ) continue;
        xl6OutputPrintf(out, "typedef struct xlir_closure_env_%u {\n",
                        (unsigned int)closure->targetSymbol);
        for ( capture = 0; capture < closure->captureCount; ++capture ) {
            const XL6_IrClosureCapture* item = &module->closureCaptures[closure->firstCapture + capture];
            const XL6_IrType* type = xl6IrTypeGet(&module->types, item->type);
            const char* cType = xl6IrCgenType(module,
                                               item->abi == XL6_IR_CLOSURE_CAPTURE_RETAIN_CELL
                                                   ? item->cellType : item->type);
            if ( cType == NULL || type == NULL ||
                 !((item->abi == XL6_IR_CLOSURE_CAPTURE_COPY_VALUE &&
                    type->ownershipAbi == XL6_IR_OWNERSHIP_ABI_TRIVIAL) ||
                   (item->abi == XL6_IR_CLOSURE_CAPTURE_COPY_STRING &&
                    type->cAbi == XL6_IR_C_ABI_MANAGED_STRING && type->kind != XL6_IR_TYPE_NULLABLE) ||
                   (item->abi == XL6_IR_CLOSURE_CAPTURE_COPY_NULLABLE_STRING &&
                    type->cAbi == XL6_IR_C_ABI_MANAGED_STRING && type->kind == XL6_IR_TYPE_NULLABLE) ||
                   (item->abi == XL6_IR_CLOSURE_CAPTURE_RETAIN_XVALUE &&
                    (type->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR ||
                     type->cAbi == XL6_IR_C_ABI_MANAGED_BYTES)) ||
                   (item->abi == XL6_IR_CLOSURE_CAPTURE_RETAIN_FUTURE &&
                    type->cAbi == XL6_IR_C_ABI_RUNTIME_FUTURE_PTR) ||
                    (item->abi == XL6_IR_CLOSURE_CAPTURE_RETAIN_CLOSURE &&
                     type->cAbi == XL6_IR_C_ABI_CLOSURE_PTR) ||
                    (item->abi == XL6_IR_CLOSURE_CAPTURE_RETAIN_CELL &&
                     item->cellType != XL6_IR_TYPE_INVALID)) )
                return xl6IrCgenDiag(diags, module, closure->declaration,
                                     "XLIR C emitter closure environment ownership ABI is unresolved");
            xl6OutputPrintf(out, "    %s c%lu;\n", cType, (unsigned long)capture);
        }
        xl6OutputPrintf(out, "} xlir_closure_env_%u;\n", (unsigned int)closure->targetSymbol);
        /* Verified physical slots, not producer-owned lifecycle callbacks.
         * Resident code owns traversal, rollback and post-commit release. */
        xl6OutputPrintf(out,
            "static const XL6_ClosureCaptureSlot xlir_closure_slots_%u[] = {\n",
            (unsigned int)closure->targetSymbol);
        for ( capture = 0; capture < closure->captureCount; ++capture ) {
            const XL6_IrClosureCapture* item = &module->closureCaptures[closure->firstCapture + capture];
            const char* kind;
            switch ( item->abi ) {
            case XL6_IR_CLOSURE_CAPTURE_RETAIN_XVALUE: kind = "XL6_CLOSURE_CAPTURE_XVALUE"; break;
            case XL6_IR_CLOSURE_CAPTURE_RETAIN_CLOSURE: kind = "XL6_CLOSURE_CAPTURE_CLOSURE"; break;
            case XL6_IR_CLOSURE_CAPTURE_RETAIN_CELL: kind = "XL6_CLOSURE_CAPTURE_CELL"; break;
            case XL6_IR_CLOSURE_CAPTURE_RETAIN_FUTURE: kind = "XL6_CLOSURE_CAPTURE_FUTURE"; break;
            case XL6_IR_CLOSURE_CAPTURE_COPY_VALUE: kind = "XL6_CLOSURE_CAPTURE_TRIVIAL"; break;
            case XL6_IR_CLOSURE_CAPTURE_COPY_STRING:
            case XL6_IR_CLOSURE_CAPTURE_COPY_NULLABLE_STRING: kind = "XL6_CLOSURE_CAPTURE_STRING"; break;
            default: return xl6IrCgenDiag(diags, module, closure->declaration,
                "XLIR C emitter closure layout ABI is unresolved");
            }
            xl6OutputPrintf(out,
                "    {%s, offsetof(xlir_closure_env_%u, c%lu), sizeof(((xlir_closure_env_%u*)0)->c%lu)},\n",
                kind, (unsigned int)closure->targetSymbol, (unsigned long)capture,
                (unsigned int)closure->targetSymbol, (unsigned long)capture);
        }
        xl6OutputPrintf(out, "};\nstatic const XL6_ClosureEnvironmentV1 xlir_closure_layout_%u = {\n"
            "    sizeof(XL6_ClosureEnvironmentV1), sizeof(xlir_closure_env_%u), %lu, xlir_closure_slots_%u\n};\n\n",
            (unsigned int)closure->targetSymbol, (unsigned int)closure->targetSymbol,
            (unsigned long)closure->captureCount, (unsigned int)closure->targetSymbol);
    }
    /* Direct async machine launchers may construct and invoke closures before
     * the thunk definitions emitted below.  Publish the exact verified thunk
     * signatures here; this is also the single declaration source used by
     * strict generated-C builds. */
    for ( i = 0; i < module->closureTargetCount; ++i ) {
        if ( module->closureTargets[i].reachability == XL6_IR_REACHABILITY_UNREACHABLE )
            continue;
        if ( !xl6IrCgenEmitClosureInvokePrototype(
                out, module, &module->closureTargets[i], diags) ) return 0;
    }
    if ( module->closureTargetCount != 0 ) xl6OutputPuts("\n", out);
    for ( i = 0; i < module->closureTargetCount; ++i ) {
        if ( module->closureTargets[i].reachability ==
                 XL6_IR_REACHABILITY_UNREACHABLE ||
             !xl6IrClosureRawCallbackAbiHasEntry(
                 module->closureTargets[i].rawCallbackAbi) ) continue;
        if ( !xl6IrCgenEmitRawCallbackEntry(
                out, module, &module->closureTargets[i], diags) ) return 0;
    }
    /* Async machine bodies may launch any reachable async target, including a
     * later declaration or themselves.  Publish every exact Future launcher
     * signature before emitting a launcher body so strict C preserves that
     * call graph without declaration-order assumptions. */
    for ( i = 0; i < module->callTargetCount; ++i ) {
        if ( module->callTargets[i].reachability ==
                 XL6_IR_REACHABILITY_UNREACHABLE ||
             (module->callTargets[i].kind != XL6_IR_CALL_TARGET_INTERNAL_ASYNC &&
              module->callTargets[i].kind != XL6_IR_CALL_TARGET_INTERNAL_GENERATOR) ) continue;
        if ( !xl6IrCgenEmitLauncherPrototype(
                out, module, &module->callTargets[i], diags) ) return 0;
    }
    if ( module->callTargetCount != 0 ) xl6OutputPuts("\n", out);
    for ( i = 0; i < module->functionCount; ++i ) {
        if ( module->functions[i].reachability == XL6_IR_REACHABILITY_UNREACHABLE ||
             xl6IrCgenFunctionIsCanonicalMachineProgram(
                 module, &module->functions[i]) ) continue;
        if ( !xl6IrCgenEmitPrototype(out, module, &module->functions[i], diags) ) return 0;
        if ( module->functions[i].kind != XL6_IR_FUNCTION_MODULE_INIT ) xl6OutputPuts(";\n", out);
    }
    xl6OutputPuts("\n", out);
    if (!xl6IrCgenEmitModuleFiniBoundary(out, module)) return 0;
    /* Continuation bodies are emitted by async/generator launchers before the
     * dispatcher definitions below.  Publish the same verified signature
     * once so protocol calls never depend on C declaration order. */
    if (!xl6IrCgenEmitVirtualDispatcherPrototypes(out, module, diags)) return 0;
    if ( !xl6IrCgenEmitProtocolDispatcherPrototypes(
            out, module, diags) ) return 0;
    if ( module->protocolDispatchSlotCount != 0 ) xl6OutputPuts("\n", out);
    if ( needs->needsXValueClassFinalizerRuntime &&
         !xl6IrCgenEmitClassDestroyHelpers(out, module, diags) ) return 0;
    /* Dispatchers may select an async witness.  Emit launchers before every
     * virtual/protocol dispatcher so strict C never relies on an implicit
     * declaration, and so the dispatcher calls the public Future ABI rather
     * than the worker result function. */
    for ( i = 0; i < module->callTargetCount; ++i ) {
        if ( module->callTargets[i].reachability != XL6_IR_REACHABILITY_UNREACHABLE &&
             module->callTargets[i].kind == XL6_IR_CALL_TARGET_INTERNAL_ASYNC &&
             (!xl6IrCgenEmitAsyncLauncher(out, module, &module->callTargets[i], diags) ||
              !xl6IrCgenEmitLaunchOutcomeBoundary(out, module, &module->callTargets[i], diags)) ) return 0;
    }
    for ( i = 0; i < module->callTargetCount; ++i ) {
        if ( module->callTargets[i].reachability !=
                 XL6_IR_REACHABILITY_UNREACHABLE &&
             module->callTargets[i].kind ==
                 XL6_IR_CALL_TARGET_INTERNAL_GENERATOR &&
             (!xl6IrCgenEmitGeneratorMachine(
                 out, module, &module->callTargets[i], diags) ||
              !xl6IrCgenEmitLaunchOutcomeBoundary(out, module, &module->callTargets[i], diags)) ) return 0;
    }
    if ( !xl6IrCgenEmitVirtualDispatchers(out, module, diags) ||
         !xl6IrCgenEmitProtocolDispatchers(out, module, diags) ) return 0;
    for ( i = 0; i < module->closureTargetCount; ++i ) {
        if ( module->closureTargets[i].reachability == XL6_IR_REACHABILITY_UNREACHABLE ) continue;
        if ( !xl6IrCgenEmitClosureInvoke(out, module, &module->closureTargets[i], diags) ) return 0;
    }
    for ( i = 0; i < module->functionCount; ++i ) {
        if ( module->functions[i].reachability == XL6_IR_REACHABILITY_UNREACHABLE ||
             xl6IrCgenFunctionIsCanonicalMachineProgram(
                 module, &module->functions[i]) ) continue;
        if ( !xl6IrCgenEmitFunction(
                out, module, &module->functions[i],
                module->abi.networkServiceAbi != XL6_IR_MODULE_NETWORK_SERVICE_NONE,
                options, diags) ) return 0;
    }
    if ( !xl6IrCgenEmitModuleEntry(out, module, diags) ) return 0;
    if ( !xl6IrCgenEmitExportCallbackAdapters(out, module, diags) ) return 0;
    for ( i = 0; i < module->exportCount; ++i ) {
        if ( !xl6IrCgenEmitExport(out, module, &module->exports[i], diags) ) return 0;
    }
    return !xl6OutputHasFailed(out);
}
