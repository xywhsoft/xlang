/* Adapt only a verified default program's native input/output carriers. Its
 * expression, capture reads, ownership and error CFG already live in XLIR. */
static int xl6IrCgenEmitClosureDefaultPrograms(XL6_Output* out,
    const XL6_IrModule* module, XL6_DiagList* diags)
{
    for (size_t c = 0; c < module->closureTargetCount; ++c) {
        const XL6_IrClosureTarget* closure = &module->closureTargets[c];
        if (closure->reachability == XL6_IR_REACHABILITY_UNREACHABLE) continue;
        const XL6_IrType* signature = xl6IrTypeGet(&module->types, closure->signatureType);
        for (size_t p = 0; p < closure->callParameterCount; ++p) {
            uint32 id = closure->callParameterDefaultFunctions[p];
            if (!id) continue;
            const XL6_IrFunction* program = &module->functions[id - 1];
            const char* result = xl6IrCgenType(module, program->returnType);
            const char* carrier = xl6IrCgenErrorResultType(module, program->returnType);
            if (!signature || !result || !carrier) return 0;
            xl6OutputPrintf(out,
                "static bool xlir_default_c%u_p%lu(void* environment, void* output, const void* const* prefix)\n{\n",
                (unsigned)closure->targetSymbol, (unsigned long)p);
            xl6OutputPuts("    (void)environment; (void)prefix;\n", out);
            xl6OutputPrintf(out, "    if (!output%s%s) { xrtSetErrorKind(XERR_TYPE); return false; }\n",
                p ? " || !prefix" : "", closure->captureCount ? " || !environment" : "");
            for (size_t input = 0; input < p; ++input)
                xl6OutputPrintf(out, "    if (!prefix[%lu]) { xrtSetErrorKind(XERR_TYPE); return false; }\n", (unsigned long)input);
            if (closure->captureCount) xl6OutputPrintf(out,
                "    const xlir_closure_env_%u* captured = (const xlir_closure_env_%u*)environment;\n",
                (unsigned)closure->targetSymbol, (unsigned)closure->targetSymbol);
            xl6OutputPrintf(out, "    %s value = xlir_fn_%u(", carrier, (unsigned)program->symbol);
            for (size_t input = 0; input < p; ++input) {
                const char* type = xl6IrCgenType(module, signature->params[input]);
                if (!type) return 0;
                xl6OutputPrintf(out, "%s*(%s const*)prefix[%lu]", input ? ", " : "", type, (unsigned long)input);
            }
            for (size_t input = 0; input < closure->captureCount; ++input)
                xl6OutputPrintf(out, "%scaptured->c%lu", p || input ? ", " : "", (unsigned long)input);
            xl6OutputPrintf(out,
                ");\n    if (!value.ok) { xl6ErrorScopePropagateFailure((xl6_error_result){false, xrtTakeError()}); return false; }\n"
                "    *(%s*)output = value.value; return true;\n}\n", result);
        }
    }
    (void)diags; return !xl6OutputHasFailed(out);
}

/* Assemble storage, closure, dispatcher, machine and export definitions. */
static int xl6IrCgenEmitModuleDefinitions(
    XL6_Output* out, const XL6_IrModule* module,
    const XL6_IrCgenNeeds* needs, const XL6_IrCgenOptions* options,
    XL6_DiagList* diags)
{
    size_t i;
    size_t parameter;
    if (!xl6IrCgenEmitPersistenceProjectPlans(out, module, diags)) return 0;
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
             closure->callParameterDefaultLiterals == NULL ||
             closure->callParameterDefaultFunctions == NULL )
            return xl6IrCgenDiag(diags, module, closure->declaration,
                "XLIR C emitter closure call-frame default metadata is unresolved");
        for (parameter = 0; parameter < closure->callParameterCount; ++parameter)
            if (closure->callParameterDefaultFunctions[parameter]) xl6OutputPrintf(out,
                "static bool xlir_default_c%u_p%lu(void*, void*, const void* const*);\n",
                (unsigned)closure->targetSymbol, (unsigned long)parameter);
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
            if ( abi == XL6_IR_DYNAMIC_METHOD_DEFAULT_STRING || abi == XL6_IR_DYNAMIC_METHOD_DEFAULT_BYTES )
                xl6OutputPuts(literal, out);
            else
                xl6OutputPuts("NULL", out);
            xl6OutputPuts(", ", out);
            if ( abi == XL6_IR_DYNAMIC_METHOD_DEFAULT_STRING || abi == XL6_IR_DYNAMIC_METHOD_DEFAULT_BYTES )
                xl6OutputPrintf(out, "sizeof(%s) - 1u", literal);
            else
                xl6OutputPuts("0u", out);
            xl6OutputPuts(", ", out);
            if ( abi == XL6_IR_DYNAMIC_METHOD_DEFAULT_CHAR )
                xl6OutputPuts(literal, out);
            else
                xl6OutputPuts("UINT32_C(0)", out);
            xl6OutputPuts(", ", out);
            if (closure->callParameterDefaultFunctions[parameter])
                xl6OutputPrintf(out, "xlir_default_c%u_p%lu", (unsigned)closure->targetSymbol, (unsigned long)parameter);
            else xl6OutputPuts("NULL", out);
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
        int nativeLayout=xl6IrClosureEnvironmentAbi(module,closure)==XL6_IR_CLOSURE_STORAGE_RESIDENT_LAYOUT_V3;
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
                   (item->abi == XL6_IR_CLOSURE_CAPTURE_COPY_NATIVE_VALUE && xl6IrOwnedTransferPlanForType(type)!=NULL) ||
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
        if(closure->forwardCallMetadata && closure->callParameterCount)
            xl6OutputPrintf(out,"    xlir_call_default forwarded_defaults[%lu];\n",(unsigned long)closure->callParameterCount);
        xl6OutputPrintf(out, "} xlir_closure_env_%u;\n", (unsigned int)closure->targetSymbol);
        if(closure->forwardCallMetadata)
            for(size_t p=0;p<closure->callParameterCount;++p)
                xl6OutputPrintf(out,
                    "static bool xlir_forward_default_%u_%lu(void* environment, void* output, const void* const* prefix) {\n"
                    "    xlir_closure* source=((xlir_closure_env_%u*)environment)->c0; XL6_ClosureScope scope={0};\n"
                    "    if(!source || source->call_parameter_count!=%lu || !source->call_parameter_defaults || !source->call_parameter_defaults[%lu].program) {\n"
                    "        xrtSetErrorTake(xlir_error_create(\"TypeError\",\"adapted callable default is unavailable\")); return false; }\n"
                    "    xlir_closure_enter(&scope,source);\n"
                    "    bool ok=source->call_parameter_defaults[%lu].program(source->environment,output,prefix);\n"
                    "    xlir_closure_leave(&scope); return ok;\n}\n",
                    (unsigned)closure->targetSymbol,(unsigned long)p,(unsigned)closure->targetSymbol,
                    (unsigned long)closure->callParameterCount,(unsigned long)p,(unsigned long)p);
        /* Verified physical slots, not producer-owned lifecycle callbacks.
         * Resident code owns traversal, rollback and post-commit release. */
        xl6OutputPrintf(out,
            "static const %s xlir_closure_slots_%u[] = {\n",
            nativeLayout ? "XL6_CaptureSlotV2" : "XL6_ClosureCaptureSlot",(unsigned int)closure->targetSymbol);
        for ( capture = 0; capture < closure->captureCount; ++capture ) {
            const XL6_IrClosureCapture* item = &module->closureCaptures[closure->firstCapture + capture];
            const char* kind;
            switch ( item->abi ) {
            case XL6_IR_CLOSURE_CAPTURE_COPY_NATIVE_VALUE:kind="XL6_CAPTURE_NATIVE_LEAF_V2";break;
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
            if(nativeLayout && item->abi!=XL6_IR_CLOSURE_CAPTURE_COPY_NATIVE_VALUE){
                switch(item->abi){
                case XL6_IR_CLOSURE_CAPTURE_RETAIN_XVALUE:kind="XL6_CAPTURE_XVALUE";break;
                case XL6_IR_CLOSURE_CAPTURE_RETAIN_CLOSURE:kind="XL6_CAPTURE_CLOSURE";break;
                case XL6_IR_CLOSURE_CAPTURE_RETAIN_CELL:kind="XL6_CAPTURE_CELL";break;
                case XL6_IR_CLOSURE_CAPTURE_RETAIN_FUTURE:kind="XL6_CAPTURE_FUTURE";break;
                case XL6_IR_CLOSURE_CAPTURE_COPY_VALUE:kind="XL6_CAPTURE_TRIVIAL";break;
                default:kind="XL6_CAPTURE_STRING";break;
                }
            }
            xl6OutputPrintf(out,
                "    %s{%s, offsetof(xlir_closure_env_%u, c%lu), sizeof(((xlir_closure_env_%u*)0)->c%lu)}",
                nativeLayout ? "{" : "",kind, (unsigned int)closure->targetSymbol, (unsigned long)capture,
                (unsigned int)closure->targetSymbol, (unsigned long)capture);
            if(nativeLayout){
                const XL6_IrNativeValuePlan* native=item->abi==XL6_IR_CLOSURE_CAPTURE_COPY_NATIVE_VALUE ?
                    xl6IrOwnedTransferPlanForType(xl6IrTypeGet(&module->types,item->type)) : NULL;
                xl6OutputPrintf(out,", %s}",native ? native->captureProfileSymbol : "NULL");
            }
            xl6OutputPuts(",\n",out);
        }
        xl6OutputPrintf(out, "};\nstatic const %s xlir_closure_layout_%u = {\n"
            "    sizeof(%s), sizeof(xlir_closure_env_%u), %lu, xlir_closure_slots_%u\n};\n\n",
            nativeLayout ? "XL6_CaptureLayoutV2" : "XL6_ClosureEnvironmentV1",(unsigned int)closure->targetSymbol,
            nativeLayout ? "XL6_CaptureLayoutV2" : "XL6_ClosureEnvironmentV1",(unsigned int)closure->targetSymbol,
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
    if (!xl6IrCgenEmitClosureDefaultPrograms(out, module, diags)) return 0;
    /* Continuation bodies are emitted by async/generator launchers before the
     * dispatcher definitions below.  Publish the same verified signature
     * once so protocol calls never depend on C declaration order. */
    if (!xl6IrCgenEmitVirtualDispatcherPrototypes(out, module, diags)) return 0;
    if ( !xl6IrCgenEmitProtocolDispatcherPrototypes(
             out, module, diags) ) return 0;
    /* Runtime ABI adapters may call ordinary lowered functions. Emit their
     * definitions only after the common prototype phase, without specialized
     * persistence-only forward declarations or signature inference. */
    if ( (needs->needsPersistenceRestoreFormats & XL6_IR_PERSIST_FORMAT_JSON) != 0 &&
         !xl6IrCgenEmitPersistenceHelpers(
             out, module, XL6_IR_PERSIST_FORMAT_JSON,
             (needs->needsPersistenceReadFormats & XL6_IR_PERSIST_FORMAT_JSON) != 0, diags) ) return 0;
    if ( (needs->needsPersistenceRestoreFormats & XL6_IR_PERSIST_FORMAT_XLON) != 0 &&
         !xl6IrCgenEmitPersistenceHelpers(
             out, module, XL6_IR_PERSIST_FORMAT_XLON,
             (needs->needsPersistenceReadFormats & XL6_IR_PERSIST_FORMAT_XLON) != 0, diags) ) return 0;
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
