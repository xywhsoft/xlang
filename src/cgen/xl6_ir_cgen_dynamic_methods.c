static int xl6IrCgenEmitBoundMethodRuntime(XL6_Output* out, const XL6_IrModule* module, XL6_DiagList* diags)
{
    if (module->abi.boundMethodStorageAbi != XL6_IR_BOUND_METHOD_STORAGE_RESIDENT_CLOSURE_LAYOUT_V1)
        return xl6IrCgenDiag(diags, module, XL6_IR_NODE_INVALID, "XLIR bound method storage ABI is unresolved");
    xl6OutputPuts(
        "typedef XL6_DynamicOwnedDispatchV1 xlir_dynamic_method_dispatch;\n"
        "typedef struct xlir_bound_method {\n"
        "    xvalue* receiver;\n"
        "    xlir_dynamic_method_dispatch invoke;\n"
        "} xlir_bound_method;\n"
        "static const XL6_ClosureCaptureSlot xlir_bound_method_slots[] = {\n"
        "    {XL6_CLOSURE_CAPTURE_XVALUE, offsetof(xlir_bound_method, receiver), sizeof(xvalue*)},\n"
        "    {XL6_CLOSURE_CAPTURE_TRIVIAL, offsetof(xlir_bound_method, invoke), sizeof(xlir_dynamic_method_dispatch)}\n"
        "};\n"
        "static const XL6_ClosureEnvironmentV1 xlir_bound_method_layout = {\n"
        "    sizeof(XL6_ClosureEnvironmentV1), sizeof(xlir_bound_method), 2, xlir_bound_method_slots\n"
        "};\n"
        "static bool xlir_bound_method_entry(void* data, const xrtcallframe* frame, xrtcallresult* callResult)\n"
        "{\n"
        "    XL6_Closure* closure = (XL6_Closure*)data;\n"
        "    xlir_bound_method* bound = closure != NULL ? (xlir_bound_method*)closure->environment : NULL;\n"
        "    xvalue** owned = NULL; bool result = false; size_t total = 0, retained = 0, i;\n"
        "    if (bound == NULL || bound->receiver == NULL || bound->invoke == NULL || callResult == NULL ||\n"
        "        frame == NULL || (frame->ArgumentCount && frame->Arguments == NULL) ||\n"
        "        (frame->KeywordCount && (frame->KeywordNames == NULL || frame->KeywordValues == NULL)) ||\n"
        "        frame->ArgumentCount > SIZE_MAX - frame->KeywordCount) {\n"
        "        xrtSetErrorTake(xlir_error_create(\"TypeError\", \"bound method frame is invalid\")); goto fail;\n"
        "    }\n"
        "    total = frame->ArgumentCount + frame->KeywordCount;\n"
        "    if (total > SIZE_MAX / sizeof(*owned)) {\n"
        "        xrtSetErrorTake(xlir_error_create(\"RangeError\", \"bound method frame is too large\")); goto fail;\n"
        "    }\n"
        "    if (total != 0) {\n"
        "        owned = (xvalue**)xrtMalloc(total * sizeof(*owned));\n"
        "        if (owned == NULL) { xrtSetErrorTake(xlir_error_create(\"MemoryError\", \"bound method argument allocation failed\")); goto fail; }\n"
        "        for (i = 0; i < total; ++i) {\n"
        "            const xvalue* argument = i < frame->ArgumentCount ? frame->Arguments[i] : frame->KeywordValues[i - frame->ArgumentCount];\n"
        "            if (argument == NULL) { xrtSetErrorTake(xlir_error_create(\"TypeError\", \"bound method argument is null\")); goto fail; }\n"
        "            owned[i] = xrtValueRetain(argument);\n"
        "            if (owned[i] == NULL) goto fail;\n"
        "            ++retained;\n"
        "        }\n"
        "    }\n"
        "    result = bound->invoke(bound->receiver, frame->ArgumentCount, owned, frame->KeywordCount,\n"
        "        frame->KeywordNames, owned != NULL ? owned + frame->ArgumentCount : NULL, callResult);\n"
        "    retained = 0; /* The verified dispatcher consumes every argument on both outcomes. */\n"
        "    xrtFree(owned); owned = NULL;\n"
        "    if (!result) goto fail;\n"
        "    return true;\n"
        "fail: {\n"
        "    xerror* pendingError;\n"
        "    xl6ForeignCallbackFail(); pendingError = xrtTakeError();\n"
        "    for (i = 0; i < retained; ++i) xrtValueRelease(owned[i]);\n"
        "    xrtFree(owned);\n"
        "    if (callResult != NULL) xrtCallResultUnit(callResult);\n"
        "    xrtSetErrorTake(pendingError); return false;\n"
        "}\n"
        "}\n"
        "static xvalue* xlir_bound_method_create(xvalue* receiver, xlir_dynamic_method_dispatch invoke, uint64_t signature)\n"
        "{\n"
        "    xlir_bound_method* bound = NULL; XL6_Closure* closure = NULL; xvalue* result = NULL;\n"
        "    if (receiver == NULL || invoke == NULL) {\n"
        "        xrtSetErrorTake(xlir_error_create(\"TypeError\", \"bound method receiver or dispatch is missing\")); return NULL;\n"
        "    }\n"
        "    bound = (xlir_bound_method*)xrtMalloc(sizeof(*bound));\n"
        "    if (bound == NULL) goto fail;\n"
        "    *bound = (xlir_bound_method){0};\n"
        "    bound->receiver = xrtValueRetain(receiver); bound->invoke = invoke;\n"
        "    if (bound->receiver == NULL) goto fail;\n"
        "    closure = xlir_closure_new((void (*)(void))xlir_bound_method_entry, NULL, bound, &xlir_bound_method_layout, NULL, NULL, 0);\n"
        "    if (closure == NULL) goto fail;\n"
        "    bound = NULL; /* Resident closure now owns the immutable receiver slot. */\n"
        "    result = xl6ClosureBoxErasedV1(xlir_closure_owner, closure, xlir_bound_method_entry);\n"
        "    if (result == NULL) goto fail;\n"
        "    if (signature && !xrtValueTypeIdBind(result, signature)) goto fail;\n"
        "    xlir_closure_release(closure);\n"
        "    return result;\n"
        "fail: {\n"
        "    xl6_error_scope cleanup = {0};\n"
        "    if (!xl6ErrorScopeBeginCleanup(&cleanup, true)) abort();\n"
        "    xrtValueRelease(result); if (xrtGetError()) xl6ErrorScopeCaptureFailure(&cleanup);\n"
        "    xlir_closure_release(closure);\n"
        "    if (xrtGetError()) xl6ErrorScopeCaptureFailure(&cleanup);\n"
        "    if (bound != NULL) xlir_closure_env_drop(&xlir_bound_method_layout, bound);\n"
        "    if (xrtGetError()) xl6ErrorScopeCaptureFailure(&cleanup);\n"
        "    xl6ErrorScopePropagateFailure(xl6ErrorScopeEnd(&cleanup)); return NULL;\n"
        "}\n"
        "}\n", out);
    return !xl6OutputHasFailed(out);
}

/* Both immediate invocation and retained method values share the same
 * verified overload dispatcher. No source-level binding is repeated here. */
static int xl6IrCgenEmitDynamicMethodDispatcher(
    XL6_Output* out, const XL6_IrModule* module, size_t siteIndex,
    XL6_IrDynamicCallKind dispatchKind)
{
    const XL6_IrDynamicCallSite* site = &module->dynamicCallSites[siteIndex];
    XL6_IrDynamicCollectionOp fallback=dispatchKind==site->kind?site->fallbackCollectionOp:
        dispatchKind==XL6_IR_DYNAMIC_CALL_METHOD?site->boundCollectionOp:XL6_IR_DYNAMIC_COLLECTION_INVALID;
    const char* fallbackOperation=xl6IrCgenDynamicCollectionOpToken(fallback);
    size_t methodIndex;
    int vector=dispatchKind==XL6_IR_DYNAMIC_CALL_METHOD;
    if(vector)xl6OutputPrintf(out,
        "static bool xlir_dynamic_method_vector_%s_d%lu(xvalue* receiver, size_t positionalCount, xvalue** positionalValues, size_t keywordCount, const xlir_string_view* keywordNames, xvalue** keywordValues, xrtcallresult* resultVector)\n"
        "{\n    xvalue* result = NULL; xvalue* typeValue = NULL; int64_t runtimeType = 0; int selected = -1; size_t argumentIndex; bool vectorSuccess = false; xrtcallresult prepared = {0};\n"
        "    xl6_foreign_error_scope invocationScope; xl6_foreign_error_result invocationOutcome;\n"
        "    if (!xl6ForeignCallBegin(&invocationScope)) {\n"
        "        xerror* error = xrtTakeError();\n"
        "        if(positionalValues)for(argumentIndex=0;argumentIndex<positionalCount;++argumentIndex)xrtValueRelease(positionalValues[argumentIndex]);\n"
        "        if(keywordValues)for(argumentIndex=0;argumentIndex<keywordCount;++argumentIndex)xrtValueRelease(keywordValues[argumentIndex]);\n"
        "        xl6ErrorScopePropagateFailure((xl6_error_result){false,error}); return false;\n    }\n",
        dispatchKind == site->kind ? "call" : "bound",(unsigned long)siteIndex);
    else xl6OutputPrintf(out,
        "static xvalue* xlir_dynamic_method_%s_d%lu(xvalue* receiver, size_t positionalCount, xvalue** positionalValues, size_t keywordCount, const xlir_string_view* keywordNames, xvalue** keywordValues)\n"
        "{\n"
        "    xvalue* result = NULL; xvalue* typeValue = NULL; int64_t runtimeType = 0; int selected = -1; size_t argumentIndex;\n"
        "    xl6_foreign_error_scope invocationScope; xl6_foreign_error_result invocationOutcome;\n"
        "    if (!xl6ForeignCallBegin(&invocationScope)) {\n"
        "        xerror* error = xrtTakeError();\n"
        "        if (positionalValues != NULL) for (argumentIndex = 0; argumentIndex < positionalCount; ++argumentIndex) xrtValueRelease(positionalValues[argumentIndex]);\n"
        "        if (keywordValues != NULL) for (argumentIndex = 0; argumentIndex < keywordCount; ++argumentIndex) xrtValueRelease(keywordValues[argumentIndex]);\n"
        "        xrtSetErrorTake(error); return NULL;\n"
        "    }\n",
        dispatchKind == site->kind ? "call" : "bound", (unsigned long)siteIndex);
    if (dispatchKind == XL6_IR_DYNAMIC_CALL_MEMBER_GET)
        xl6OutputPuts(
            "    if (receiver == NULL || xrtValueType(receiver) == 0) { result = xrtValueRetain(xrtValueNull()); goto cleanup; }\n"
            "    if (xrtValueType(receiver) == 12) typeValue = xrtValueObjectGet(receiver, xrtStrView(\"$xlir.class.runtime-type\"));\n"
            "    if (typeValue != NULL) (void)xrtValueGetInt(typeValue, &runtimeType);\n", out);
    else if (dispatchKind == XL6_IR_DYNAMIC_CALL_MEMBER_SET)
        xl6OutputPuts(
            "    if (positionalCount != 1 || positionalValues == NULL || keywordCount != 0 || receiver == NULL) { xrtSetErrorTake(xlir_error_create(\"TypeError\", \"dynamic setter requires a record receiver and one value\")); goto cleanup; }\n"
            "    if (xrtValueType(receiver) == 12) typeValue = xrtValueObjectGet(receiver, xrtStrView(\"$xlir.class.runtime-type\"));\n"
            "    if (typeValue != NULL && !xrtValueGetInt(typeValue, &runtimeType)) goto cleanup;\n",
            out);
    else if ( fallbackOperation != NULL ) {
        if(fallback>=XL6_IR_DYNAMIC_COLLECTION_TRY_POP && fallback<=XL6_IR_DYNAMIC_COLLECTION_TRY_PEEK_AT)
            xl6OutputPrintf(out,
                "    if(receiver == NULL || xrtValueType(receiver) != 12) {\n"
                "        if(keywordCount || (positionalCount && !positionalValues) || !xl6DynamicCollectionVectorArityV1(%s,positionalCount)) { xrtSetErrorKind(XERR_ARGUMENT); goto cleanup; }\n"
                "        vectorSuccess = xl6DynamicCollectionVectorTakeV1(receiver,%s,positionalCount?positionalValues[0]:NULL,NULL,&prepared);\n"
                "        for(argumentIndex=0;argumentIndex<positionalCount;++argumentIndex)positionalValues[argumentIndex]=NULL;\n"
                "        goto cleanup;\n    }\n"
                "    typeValue = xrtValueObjectGet(receiver,xrtStrView(\"$xlir.class.runtime-type\"));\n"
                "    if(!typeValue || !xrtValueGetInt(typeValue,&runtimeType)) { xrtSetErrorKind(XERR_TYPE); goto cleanup; }\n",
                fallbackOperation,fallbackOperation);
        else {
        xl6OutputPrintf(out,
            "    if (xrtValueType(receiver) != 12) {\n"
            "        if (keywordCount || (positionalCount && !positionalValues) || !xl6DynamicCollectionArityV1(%s, positionalCount)) { xrtSetErrorTake(xlir_error_create(\"ArgumentError\", \"invalid dynamic collection arguments\")); goto cleanup; }\n"
            "        result = xl6DynamicCollectionCallTakeV1(receiver, %s, positionalCount > 0 ? positionalValues[0] : NULL, positionalCount > 1 ? positionalValues[1] : NULL);\n"
            "        for (argumentIndex = 0; argumentIndex < positionalCount; ++argumentIndex) positionalValues[argumentIndex] = NULL;\n"
            "        if (result == NULL && xrtGetError() == NULL) xrtSetErrorTake(xlir_error_create(\"RuntimeError\", \"dynamic collection fallback failed\"));\n"
            "        if(result != NULL) vectorSuccess = xrtCallResultSetTake(&prepared,0,&result);\n"
            "        goto cleanup;\n"
            "    }\n"
            "    typeValue = xrtValueObjectGet(receiver, xrtStrView(\"$xlir.class.runtime-type\"));\n"
            "    if (typeValue == NULL || !xrtValueGetInt(typeValue, &runtimeType)) { if (xrtGetError() == NULL) xrtSetErrorTake(xlir_error_create(\"TypeError\", \"dynamic method receiver has no runtime class tag\")); goto cleanup; }\n",
            fallbackOperation,fallbackOperation);
        }
    }
    else
        xl6OutputPuts(
            "    typeValue = xrtValueObjectGet(receiver, xrtStrView(\"$xlir.class.runtime-type\"));\n"
            "    if (typeValue == NULL || !xrtValueGetInt(typeValue, &runtimeType)) { if (xrtGetError() == NULL) xrtSetErrorTake(xlir_error_create(\"TypeError\", \"dynamic method receiver has no runtime class tag\")); goto cleanup; }\n",
            out);
    if(dispatchKind==XL6_IR_DYNAMIC_CALL_MEMBER_GET && site->boundCollectionOp!=XL6_IR_DYNAMIC_COLLECTION_INVALID) {
        xl6OutputPrintf(out,
            "    if(xrtValueType(receiver) != 12) { int nativeStatus = xl6DynamicStackMethodProbeV1(receiver);\n"
            "        if(nativeStatus < 0)goto cleanup;\n"
            "        if(nativeStatus > 0) { result = xlir_bound_method_create(receiver,xlir_dynamic_method_vector_bound_d%lu, 0); goto cleanup; }\n    }\n",
            (unsigned long)siteIndex);
    }
    if (dispatchKind == XL6_IR_DYNAMIC_CALL_MEMBER_GET) {
        for (methodIndex = 0; methodIndex < module->dynamicMethodCount; ++methodIndex) {
            const XL6_IrDynamicMethod* method = &module->dynamicMethods[methodIndex];
            if (method->kind != dispatchKind || method->adapterAbi == XL6_IR_DYNAMIC_METHOD_ADAPTER_VERIFIED ||
                method->name == NULL || strcmp(method->name, site->memberName) != 0) continue;
            xl6OutputPrintf(out,
                "    if (runtimeType == %u) { if (xrtGetError() == NULL) xrtSetErrorTake(xlir_error_create(\"TypeError\", \"dynamic getter has no verified adapter ABI\")); goto cleanup; }\n",
                (unsigned int)method->runtimeTypeTag);
        }
    }
    xl6OutputPuts("    switch (runtimeType) {\n", out);
    for ( methodIndex = 0; methodIndex < module->dynamicMethodCount;
          ++methodIndex ) {
        const XL6_IrDynamicMethod* method =
            &module->dynamicMethods[methodIndex];
        size_t prior;
        size_t candidate;
        if ( method->kind != dispatchKind || !xl6IrCgenDynamicMethodNeeded(module, methodIndex) ||
             method->name == NULL || site->memberName == NULL ||
             strcmp(method->name, site->memberName) != 0 ) continue;
        for ( prior = 0; prior < methodIndex; ++prior )
            if ( xl6IrCgenDynamicMethodNeeded(module, prior) &&
                 module->dynamicMethods[prior].kind == dispatchKind &&
                 module->dynamicMethods[prior].name != NULL &&
                 strcmp(module->dynamicMethods[prior].name,
                        site->memberName) == 0 &&
                 module->dynamicMethods[prior].runtimeTypeTag ==
                     method->runtimeTypeTag ) break;
        if ( prior != methodIndex ) continue;
        xl6OutputPrintf(out,
            "    case %u:\n", (unsigned int)method->runtimeTypeTag);
        for ( candidate = methodIndex;
              candidate < module->dynamicMethodCount; ++candidate ) {
            const XL6_IrDynamicMethod* overload =
                &module->dynamicMethods[candidate];
            if ( overload->kind != dispatchKind || !xl6IrCgenDynamicMethodNeeded(module, candidate) ||
                 overload->runtimeTypeTag != method->runtimeTypeTag ||
                 overload->name == NULL ||
                 strcmp(overload->name, site->memberName) != 0 ) continue;
            xl6OutputPrintf(out,
                "        if (xlir_dynamic_method_match_dm%lu(positionalCount, positionalValues, keywordCount, keywordNames, keywordValues)) { if (selected != -1) { selected = -2; break; } selected = %lu; }\n",
                (unsigned long)candidate, (unsigned long)candidate);
        }
        xl6OutputPuts("        break;\n", out);
    }
    xl6OutputPuts(
        "    default: break;\n"
        "    }\n"
        "    if (selected == -2) { if (xrtGetError() == NULL) xrtSetErrorTake(xlir_error_create(\"TypeError\", \"dynamic method overload is ambiguous\")); goto cleanup; }\n"
        "    switch (selected) {\n",
        out);
    for ( methodIndex = 0; methodIndex < module->dynamicMethodCount;
          ++methodIndex ) {
        const XL6_IrDynamicMethod* method =
            &module->dynamicMethods[methodIndex];
        if ( method->kind != dispatchKind || !xl6IrCgenDynamicMethodNeeded(module, methodIndex) ||
             method->name == NULL || site->memberName == NULL ||
             strcmp(method->name, site->memberName) != 0 ) continue;
        if(vector)xl6OutputPrintf(out,
            "    case %lu: vectorSuccess = xlir_dynamic_method_dm%lu(receiver, positionalCount, positionalValues, keywordCount, keywordNames, keywordValues,&prepared); break;\n",
            (unsigned long)methodIndex,(unsigned long)methodIndex);
        else xl6OutputPrintf(out,
            "    case %lu: result = xlir_dynamic_method_dm%lu(receiver, positionalCount, positionalValues, keywordCount, keywordNames, keywordValues); break;\n",
            (unsigned long)methodIndex,(unsigned long)methodIndex);
    }
    if (dispatchKind == XL6_IR_DYNAMIC_CALL_MEMBER_GET) {
        xl6OutputPuts("    default: {\n", out);
        if(site->memberBindingAbi!=XL6_IR_DYNAMIC_MEMBER_BINDING_RETAIN_RECEIVER)
            xl6OutputPuts("        result = xl6ValueNameReadCopyV1(receiver, ",out);
        else xl6OutputPuts("        if (xrtValueType(receiver) != 12) { result = xl6ValueNameReadCopyV1(receiver, ",out);
        xl6IrCgenEmitCStringLiteral(out,site->memberName);
        xl6OutputPuts(", sizeof(",out); xl6IrCgenEmitCStringLiteral(out,site->memberName);
        xl6OutputPuts(")-1, true); break;",out);
        if(site->memberBindingAbi==XL6_IR_DYNAMIC_MEMBER_BINDING_RETAIN_RECEIVER) {
            xl6OutputPuts(" }\n        xvalue* field = xlir_xvalue_dynamic_field_find(receiver, xrtStrView(",out);
            xl6IrCgenEmitCStringLiteral(out,site->memberName);
            xl6OutputPuts("));\n        if (xrtGetError() != NULL) break;\n        if (field != NULL) { result = xrtValueRetain(field); break; }\n",out);
        } else xl6OutputPuts("\n",out);
        if (site->memberBindingAbi == XL6_IR_DYNAMIC_MEMBER_BINDING_RETAIN_RECEIVER) {
            /* Presence and runtime identities are fixed in the typed registry.
             * A present null field is not an absent field or a method group. */
            xl6OutputPuts("        switch (runtimeType) {\n", out);
            for (methodIndex = 0; methodIndex < module->dynamicMethodCount; ++methodIndex) {
                const XL6_IrDynamicMethod* method = &module->dynamicMethods[methodIndex];
                size_t prior;
                if (method->kind != XL6_IR_DYNAMIC_CALL_METHOD ||
                    !xl6IrDynamicMethodMatchesCallSite(site, method)) continue;
                for (prior = 0; prior < methodIndex; ++prior)
                    if (module->dynamicMethods[prior].kind == XL6_IR_DYNAMIC_CALL_METHOD &&
                        xl6IrDynamicMethodMatchesCallSite(site, &module->dynamicMethods[prior]) &&
                        module->dynamicMethods[prior].runtimeTypeTag == method->runtimeTypeTag) break;
                if (prior != methodIndex) continue;
                const XL6_IrType* signature=xl6IrTypeGet(&module->types,method->boundSignatureType);
                xl6OutputPrintf(out, "        case %u: result = xlir_bound_method_create(receiver, xlir_dynamic_method_vector_bound_d%lu, 0x%016llXULL); break;\n",
                    (unsigned int)method->runtimeTypeTag, (unsigned long)siteIndex,
                    (unsigned long long)(signature ? signature->typeValueId : 0));
            }
            xl6OutputPuts("        default: result = xrtValueRetain(xrtValueNull()); break;\n        }\n", out);
            xl6OutputPuts("        break;\n",out);
        }
        xl6OutputPuts("    }\n", out);
    } else if (dispatchKind == XL6_IR_DYNAMIC_CALL_MEMBER_SET) {
        xl6OutputPuts("    default: {\n", out);
        /* Property presence is registry data, even when its adapter is unsupported
         * or its getter is unreachable. Neither case authorizes a raw field write. */
        for (methodIndex = 0; methodIndex < module->dynamicMethodCount; ++methodIndex) {
            const XL6_IrDynamicMethod* method = &module->dynamicMethods[methodIndex];
            if ((method->kind != XL6_IR_DYNAMIC_CALL_MEMBER_GET && method->kind != XL6_IR_DYNAMIC_CALL_MEMBER_SET) ||
                method->name == NULL || strcmp(method->name, site->memberName) != 0) continue;
            xl6OutputPrintf(out,
                "        if (runtimeType == %u) { xrtSetErrorTake(xlir_error_create(\"TypeError\", \"dynamic property is read-only or has no matching verified setter\")); goto cleanup; }\n",
                (unsigned int)method->runtimeTypeTag);
        }
        xl6OutputPuts("        bool stored = xl6ValueNameSetTakeV1(receiver, ", out);
        xl6IrCgenEmitCStringLiteral(out, site->memberName);
        xl6OutputPuts(", sizeof(",out); xl6IrCgenEmitCStringLiteral(out,site->memberName);
        xl6OutputPuts(
            ")-1, positionalValues[0]);\n"
            "        positionalValues[0] = NULL;\n"
            "        if (stored) result = xrtValueRetain(xrtValueNull());\n"
            "        else if (xrtGetError() == NULL) xrtSetErrorTake(xlir_error_create(\"RuntimeError\", \"dynamic field assignment failed\"));\n"
            "        break;\n"
            "    }\n", out);
    } else
        xl6OutputPuts("    default: if (xrtGetError() == NULL) xrtSetErrorTake(xlir_error_create(\"TypeError\", \"dynamic method call has no matching verified overload\")); break;\n", out);
    if(vector) {
        xl6OutputPuts("    }\ncleanup:\n"
            "    if(!vectorSuccess || xrtGetError())xl6ErrorScopeCaptureFailure(&invocationScope);\n"
            "    if(positionalValues)for(argumentIndex=0;argumentIndex<positionalCount;++argumentIndex) { xrtValueRelease(positionalValues[argumentIndex]); if(xrtGetError())xl6ErrorScopeCaptureFailure(&invocationScope); }\n"
            "    if(keywordValues)for(argumentIndex=0;argumentIndex<keywordCount;++argumentIndex) { xrtValueRelease(keywordValues[argumentIndex]); if(xrtGetError())xl6ErrorScopeCaptureFailure(&invocationScope); }\n"
            "    xrtValueRelease(result);\n"
            "    if(invocationScope.failed || xrtGetError()) { xl6ErrorScopeCaptureFailure(&invocationScope); xrtCallResultUnit(&prepared); }\n"
            "    else if(!resultVector || !xrtCallResultMove(resultVector,&prepared)) { xl6ErrorScopeCaptureFailure(&invocationScope); xrtCallResultUnit(&prepared); }\n"
            "    invocationOutcome = xl6ForeignCallEnd(&invocationScope);\n"
            "    if(!invocationOutcome.ok) { if(resultVector)xrtCallResultClear(resultVector); xl6ErrorScopePropagateFailure(invocationOutcome); return false; }\n"
            "    return true;\n}\n",out);
        return !xl6OutputHasFailed(out);
    }
    xl6OutputPuts(
        "    }\n"
        "cleanup: {\n"
        "    if (result == NULL || xrtGetError() != NULL) xl6ErrorScopeCaptureFailure(&invocationScope);\n"
        "    if (positionalValues != NULL) for (argumentIndex = 0; argumentIndex < positionalCount; ++argumentIndex) xrtValueRelease(positionalValues[argumentIndex]);\n"
        "    if (keywordValues != NULL) for (argumentIndex = 0; argumentIndex < keywordCount; ++argumentIndex) xrtValueRelease(keywordValues[argumentIndex]);\n"
        "    if (invocationScope.failed || xrtGetError() != NULL) { xl6ErrorScopeCaptureFailure(&invocationScope); xrtValueRelease(result); result = NULL; }\n"
        "    invocationOutcome = xl6ForeignCallEnd(&invocationScope);\n"
        "    if (!invocationOutcome.ok) xl6ErrorScopePropagateFailure(invocationOutcome);\n"
        "    return result;\n"
        "    }\n"
        "}\n",
        out);
    return !xl6OutputHasFailed(out);
}

/* Bit 1 selects primitive predicates, bit 2 exact identities. Emission and
 * the resident link plan share this reachable-row closure. */
static unsigned xl6IrCgenDynamicArgumentAdmissionNeeds(const XL6_IrModule* module)
{
    size_t methodIndex;
    unsigned needs = 0;
    for ( methodIndex = 0; methodIndex < module->dynamicMethodCount; ++methodIndex ) {
        const XL6_IrDynamicMethod* method = &module->dynamicMethods[methodIndex];
        if ( !xl6IrCgenDynamicMethodNeeded(module, methodIndex) ) continue;
        for ( size_t p = 0; p < method->fixedParameterCount; ++p ) {
            XL6_IrDynamicMethodMatchAbi abi =
                module->dynamicMethodParameters[method->firstParameter + p].matchAbi;
            if(abi==XL6_IR_DYNAMIC_METHOD_MATCH_SCALAR_CODEC)needs|=3u;
            else if ( abi == XL6_IR_DYNAMIC_METHOD_MATCH_EXACT_IDENTITY ) needs |= 2u;
            else if ( abi != XL6_IR_DYNAMIC_METHOD_MATCH_ANY_XVALUE &&
                      abi != XL6_IR_DYNAMIC_METHOD_MATCH_ZERO_PAYLOAD &&
                      abi != XL6_IR_DYNAMIC_METHOD_MATCH_NOMINAL ) needs |= 1u;
        }
    }
    return needs;
}

static int xl6IrCgenEmitDynamicMethodRuntime(
    XL6_Output* out, const XL6_IrModule* module, XL6_DiagList* diags)
{
    /* These adapters are emitted before resource support. Their physical
     * function-pointer and vector ABI declarations must precede all uses. */
    xl6OutputPuts("#include <__xl_runtime/xl6_dynamic_product.h>\n",out);
    for(size_t i=0;i<module->dynamicCallSiteCount;++i) {
        const XL6_IrDynamicCallSite* site=&module->dynamicCallSites[i];
        if(site->boundCollectionOp!=XL6_IR_DYNAMIC_COLLECTION_INVALID ||
           (site->fallbackCollectionOp>=XL6_IR_DYNAMIC_COLLECTION_TRY_POP &&
            site->fallbackCollectionOp<=XL6_IR_DYNAMIC_COLLECTION_TRY_PEEK_AT)) {
            xl6OutputPuts("#include <__xl_runtime/xl6_dynamic_collection_vector.h>\n",out); break;
        }
    }
    size_t methodIndex;
    size_t siteIndex;
    int bindingRuntimeEmitted = 0;
    unsigned admissionNeeds = xl6IrCgenDynamicArgumentAdmissionNeeds(module);
    for ( methodIndex = 0; methodIndex < module->dynamicMethodCount; ++methodIndex ) {
        const XL6_IrCallTarget* target;
        if ( !xl6IrCgenDynamicMethodNeeded(module, methodIndex) ) continue;
        target = xl6IrCallTargetGet(module, module->dynamicMethods[methodIndex].targetSymbol);
        if ( target->kind == XL6_IR_CALL_TARGET_EXTERN_C ) {
            xl6OutputPuts("#ifndef XLIR_XRT_DECLARATIONS\ntypedef struct xvaluehandleops xvaluehandleops;\n#endif\n"
                "extern bool xrtValueGetHandle(const xvalue*, void**, const xvaluehandleops**, void**);\n"
                "extern bool xrtValueGetPointer(const xvalue*, void**);\n", out);
            break;
        }
    }
    if ( admissionNeeds & 1u )
        xl6OutputPuts("extern bool xl6ValueAdmitsV1(const xvalue*, unsigned int, bool, uint64_t);\n", out);
    if ( admissionNeeds & 2u )
        xl6OutputPuts("extern uint64_t xrtValueTypeId(const xvalue*);\n", out);
    xl6OutputPuts(
        "extern bool xrtValueGetBool(const xvalue*, bool*);\n"
        "extern bool xrtValueGetInt(const xvalue*, int64_t*);\n"
        "extern bool xrtValueGetUInt(const xvalue*, uint64_t*);\n"
        "extern bool xrtValueGetFloat(const xvalue*, double*);\n"
        "extern bool xrtValueGetTime(const xvalue*, int64_t*);\n"
        "extern bool xrtValueGetString(const xvalue*, xlir_string_view*);\n"
        "#ifndef XLIR_XRT_DECLARATIONS\nextern int xrtValueType(const xvalue*);\n#endif\n"
        "extern char* xrtStrDupN(const char*, size_t);\n"
        "extern xvalue* xrtValueTime(int64_t);\n",
        out);
    for ( methodIndex = 0; methodIndex < module->dynamicMethodCount;
          ++methodIndex ) {
        const XL6_IrFunction* function;
        const XL6_IrCallTarget* target;
        if ( !xl6IrCgenDynamicMethodNeeded(module, methodIndex) ) continue;
        target = xl6IrCallTargetGet(module, module->dynamicMethods[methodIndex].targetSymbol);
        const XL6_IrDynamicMethod* method = &module->dynamicMethods[methodIndex];
        for (size_t p = 0; p < method->parameterCount; ++p) {
            const XL6_IrDynamicMethodParameter* item = &module->dynamicMethodParameters[method->firstParameter + p];
            if (!item->defaultFunction) continue;
            if (!xl6IrCgenEmitPrototype(out, module, &module->functions[item->defaultFunction - 1], diags)) return 0;
            xl6OutputPuts(";\n", out);
        }
        if ( target->kind == XL6_IR_CALL_TARGET_EXTERN_C ) {
            if ( !xl6IrCgenEmitExternPrototype(out, module, target, diags) ) return 0;
            continue;
        }
        if (target->kind==XL6_IR_CALL_TARGET_INTERNAL_ASYNC) {
            if (!xl6IrCgenEmitLauncherPrototype(out,module,target,diags)) return 0;
            continue;
        }
        const XL6_IrNode* declaration=xl6IrNodeGet(module,target->declaration);
        /* Compiled-package entries have an exact external prototype emitted
         * before this adapter family, not a duplicate local function body. */
        if(declaration && declaration->compiledPackageLink)continue;
        function = xl6IrCgenFindFunction(
            module, module->dynamicMethods[methodIndex].targetSymbol);
        if ( function == NULL )return xl6IrCgenDiag(diags,module,target->declaration,
            "XLIR dynamic method has no function implementation or compiled package binding");
        if (!xl6IrCgenEmitPrototype(out, module, function, diags)) return 0;
        xl6OutputPuts(";\n", out);
    }
    for ( methodIndex = 0; methodIndex < module->dynamicMethodCount;
          ++methodIndex )
        if ( xl6IrCgenDynamicMethodNeeded(module, methodIndex) &&
             !xl6IrCgenEmitDynamicMethodMatcher(
                 out, module, methodIndex, diags) ) return 0;
    for ( methodIndex = 0; methodIndex < module->dynamicMethodCount;
          ++methodIndex )
        if ( xl6IrCgenDynamicMethodNeeded(module, methodIndex) &&
             !xl6IrCgenEmitDynamicMethodAdapter(
                 out, module, methodIndex, diags) ) return 0;
    for ( siteIndex = 0; siteIndex < module->dynamicCallSiteCount;
          ++siteIndex ) {
        const XL6_IrDynamicCallSite* site =
            &module->dynamicCallSites[siteIndex];
        size_t instructionIndex;
        int needed = 0;
        if ( site->kind != XL6_IR_DYNAMIC_CALL_METHOD && site->kind != XL6_IR_DYNAMIC_CALL_MEMBER_GET && site->kind != XL6_IR_DYNAMIC_CALL_MEMBER_SET ) continue;
        for ( instructionIndex = 0;
              instructionIndex < module->instructionCount; ++instructionIndex ) {
            const XL6_IrInstruction* instruction =
                &module->instructions[instructionIndex];
            if ( xl6IrCgenInstructionIsReachable(module, instructionIndex) &&
                 instruction->kind == XL6_IR_INSTR_RUNTIME_CALL &&
                 instruction->runtimeOp ==
                     XL6_IR_RUNTIME_XVALUE_DYNAMIC_METHOD_CALL &&
                 instruction->dynamicCallSite == siteIndex ) {
                needed = 1;
                break;
            }
        }
        if ( !needed ) continue;
        if (site->memberBindingAbi == XL6_IR_DYNAMIC_MEMBER_BINDING_RETAIN_RECEIVER) {
            if (!bindingRuntimeEmitted) {
                if (!xl6IrCgenEmitBoundMethodRuntime(out, module, diags)) return 0;
                bindingRuntimeEmitted = 1;
            }
            if (!xl6IrCgenEmitDynamicMethodDispatcher(out, module, siteIndex, XL6_IR_DYNAMIC_CALL_METHOD)) return 0;
        }
        if (!xl6IrCgenEmitDynamicMethodDispatcher(out, module, siteIndex, site->kind)) return 0;
    }
    return !xl6OutputHasFailed(out);
}
