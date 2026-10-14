/* Execute only the verified destruction program. Clear each slot before a
 * destructor can re-enter, and preserve the first failure across all slots. */
static int xl6IrCgenEmitCallableBridgeCleanup(
    XL6_Output* out, const XL6_IrModule* module,
    const XL6_IrCallableBridge* bridge, int reverse, const char* failed, const char* outcome)
{
    size_t i;
    if (!bridge || bridge->cleanupAbi != XL6_IR_CALLBACK_CLEANUP_TRANSACTIONAL ||
        bridge->errorAbi != XL6_IR_CALLBACK_ERROR_PRESERVE) return 0;
    xl6OutputPrintf(out,
        "    { xl6_error_scope parameter_cleanup = {0}; xl6_error_result parameter_outcome;\n"
        "      if (!xl6ErrorScopeBeginCleanup(&parameter_cleanup, %s)) abort();\n", failed);
    if (reverse) for (i = bridge->parameterCount; i > 0; --i) {
        xl6OutputPrintf(out,
            "      { xvalue* value = arguments[%lu]; arguments[%lu] = NULL; xrtValueRelease(value); }\n"
            "      if (xrtGetError() != NULL) xl6ErrorScopeCaptureFailure(&parameter_cleanup);\n",
            (unsigned long)(i - 1), (unsigned long)(i - 1));
    }
    for (i = bridge->parameterCount; i > 0; --i) {
        const XL6_IrCallableBridgeParameter* slot =
            &module->callableBridgeParameters[bridge->firstParameter + i - 1];
        XL6_IrAsyncFrameDestroyAbi abi = reverse ? slot->inputDestroyAbi : slot->decodedDestroyAbi;
        const char* destroy;
        if (abi == XL6_IR_ASYNC_FRAME_DESTROY_NONE) continue;
        destroy = xl6IrCgenCallbackDestroy(abi);
        if (!destroy || !xl6IrCgenType(module, slot->type)) return 0;
        xl6OutputPrintf(out,
            "      { %s value = a%lu; a%lu = (%s){0}; %s(value); }\n"
            "      if (xrtGetError() != NULL) xl6ErrorScopeCaptureFailure(&parameter_cleanup);\n",
            xl6IrCgenType(module, slot->type), (unsigned long)(i - 1), (unsigned long)(i - 1),
            xl6IrCgenType(module, slot->type), destroy);
    }
    /* A missing bytes parameter owns its literal materialization even when
     * the target only borrows it. It is separate from the decoded ABI slot. */
    if(!reverse)for(i=0;i<bridge->parameterCount;++i) {
        const XL6_IrCallableBridgeParameter* slot=&module->callableBridgeParameters[bridge->firstParameter+i];
        if(slot->decodeAbi==XL6_IR_CALLABLE_BRIDGE_BYTES_BORROW || slot->decodeAbi==XL6_IR_CALLABLE_BRIDGE_BYTES_RETAIN)
            xl6OutputPrintf(out,"      { xvalue* value = default_bytes%lu; default_bytes%lu = NULL; xrtValueRelease(value); }\n"
                "      if(xrtGetError())xl6ErrorScopeCaptureFailure(&parameter_cleanup);\n",(unsigned long)i,(unsigned long)i);
    }
    xl6OutputPrintf(out,
        "      parameter_outcome = xl6ErrorScopeEnd(&parameter_cleanup);\n"
        "      xrtSetErrorTake(parameter_outcome.error); %s = parameter_outcome.ok; }\n", outcome);
    return !xl6OutputHasFailed(out);
}

static int xl6IrCgenEmitRuntimeCallableEntryPrototype(
    XL6_Output* out, const XL6_IrModule* module, XL6_IrTypeId type,
    XL6_DiagList* diags)
{
    if ( xl6IrCallableBridgeGet(module, type) == NULL )
        return xl6IrCgenDiag(diags, module, XL6_IR_NODE_INVALID,
            "XLIR C emitter runtime callable adapter prototype is unresolved");
    xl6OutputPrintf(out,
        "static bool xlir_xcall_entry_t%u(void*, const xrtcallframe*, xrtcallresult*);\n",
        (unsigned int)type);
    return !xl6OutputHasFailed(out);
}

static int xl6IrCgenEmitRuntimeCallableClosureEntryPrototype(
    XL6_Output* out, const XL6_IrModule* module, XL6_IrTypeId type,
    XL6_DiagList* diags)
{
    const XL6_IrType* signature = xl6IrTypeGet(&module->types, type);
    size_t i;
    const XL6_IrCallableBridge* bridge = xl6IrCallableBridgeGet(module, type);
    if ( bridge == NULL || !bridge->reverseSupported || signature == NULL ||
         bridge->reverseErrorBoundaryAbi != XL6_IR_CALLABLE_ERROR_BOUNDARY_ISOLATED_STATUS )
        return xl6IrCgenDiag(diags, module, XL6_IR_NODE_INVALID,
            "XLIR C emitter runtime callable closure prototype is unresolved");
    xl6OutputPrintf(out,
        "static bool xlir_xvalue_closure_entry_t%u(void*",
        (unsigned int)type);
    if ( signature->returnCount != 0 ) {
        const char* resultType = xl6IrCgenType(
            module, signature->callableResultType);
        if ( resultType == NULL )
            return xl6IrCgenDiag(diags, module, XL6_IR_NODE_INVALID,
                "XLIR C emitter runtime callable closure prototype result is unresolved");
        xl6OutputPrintf(out, ", %s*", resultType);
    }
    for ( i = 0; i < signature->paramCount; ++i ) {
        const char* parameterType = xl6IrCgenType(
            module, signature->params[i]);
        if ( parameterType == NULL )
            return xl6IrCgenDiag(diags, module, XL6_IR_NODE_INVALID,
                "XLIR C emitter runtime callable closure prototype parameter is unresolved");
        xl6OutputPrintf(out, ", %s", parameterType);
    }
    xl6OutputPuts(");\n", out);
    return !xl6OutputHasFailed(out);
}

/* Emit one forward adapter per verified function TypeId.  All slot and
 * variadic-carrier choices come from the closure-to-xvalue XLIR contract. */
static int xl6IrCgenEmitRuntimeCallableEntry(
    XL6_Output* out, const XL6_IrModule* module, XL6_IrTypeId type,
    XL6_DiagList* diags)
{
    const XL6_IrType* signature = xl6IrTypeGet(&module->types, type);
    const XL6_IrCallableBridge* bridge = xl6IrCallableBridgeGet(module, type);
    const XL6_IrType* result = NULL;
    const char* resultType = NULL;
    size_t fixedCount;
    int arrayVariadic;
    int dictVariadic;
    int hasResult;
    size_t i;
    if ( bridge == NULL || signature == NULL ||
         bridge->forwardErrorBoundaryAbi != XL6_IR_CALLABLE_ERROR_BOUNDARY_NATIVE_CALLBACK )
        return xl6IrCgenDiag(diags, module, XL6_IR_NODE_INVALID,
                             "XLIR C emitter runtime callable signature is unresolved");
    hasResult = signature->returnCount != 0;
    if ( hasResult &&
         ((result = xl6IrTypeGet(
               &module->types, signature->callableResultType)) == NULL ||
          (resultType = xl6IrCgenType(
               module, signature->callableResultType)) == NULL) )
        return xl6IrCgenDiag(diags, module, XL6_IR_NODE_INVALID,
            "XLIR C emitter runtime callable result carrier is unresolved");
    arrayVariadic = bridge->erasureAbi == XL6_IR_CALLABLE_ERASURE_ARRAY_VARIADIC;
    dictVariadic = bridge->erasureAbi == XL6_IR_CALLABLE_ERASURE_DICT_VARIADIC;
    fixedCount = bridge->fixedParameterCount;
    xl6OutputPrintf(out,
        "static bool xlir_xcall_entry_t%u(void* environment, const xrtcallframe* frame, xrtcallresult* callResult)\n"
        "{\n"
        "    xlir_closure* closure = (xlir_closure*)environment; bool invoked = false; xrtcallresult bridge_results = {0};",
        (unsigned int)type);
    if ( hasResult ) {
        if ( result->cAbi == XL6_IR_C_ABI_PRODUCT || result->cAbi==XL6_IR_C_ABI_TAGGED_NULLABLE ||
             result->cAbi == XL6_IR_C_ABI_MANAGED_STRING )
            xl6OutputPrintf(out, " %s value = {0}; xvalue* boxed = NULL;",
                resultType);
        else
            xl6OutputPrintf(out, " %s value = (%s)0; xvalue* boxed = NULL;",
                resultType, resultType);
    }
    xl6OutputPuts("\n", out);
    for ( i = 0; i < signature->paramCount; ++i ) {
        const char* parameterType = xl6IrCgenType(module, signature->params[i]);
        if ( parameterType == NULL )
            return xl6IrCgenDiag(diags, module, XL6_IR_NODE_INVALID,
                                 "XLIR C emitter runtime callable parameter is unresolved");
        if ( i < fixedCount )
            xl6OutputPrintf(out,
                "    xvalue* f%lu = NULL; bool bound%lu = false; %s a%lu = (%s){0};\n",
                (unsigned long)i, (unsigned long)i, parameterType,
                (unsigned long)i, parameterType);
        else
            xl6OutputPrintf(out, "    %s a%lu = (%s){0};\n",
                parameterType, (unsigned long)i, parameterType);
        const XL6_IrCallableBridgeParameter* slot=&module->callableBridgeParameters[bridge->firstParameter+i];
        if(slot->decodeAbi==XL6_IR_CALLABLE_BRIDGE_BYTES_BORROW || slot->decodeAbi==XL6_IR_CALLABLE_BRIDGE_BYTES_RETAIN)
            xl6OutputPrintf(out,"    xvalue* default_bytes%lu = NULL;\n",(unsigned long)i);
    }
    if ( dictVariadic )
        xl6OutputPrintf(out,
            "    if (closure == NULL || frame == NULL || callResult == NULL || "
            "frame->ArgumentCount > %lu || "
            "(frame->ArgumentCount != 0 && frame->Arguments == NULL) || "
            "(frame->KeywordCount != 0 && (frame->KeywordNames == NULL || frame->KeywordValues == NULL)) || "
            "(frame->KeywordCount != 0 && %lu != 0 && (closure->call_parameter_count != %lu || closure->call_parameter_names == NULL))) {\n"
            "        xrtSetErrorTake(xlir_error_create(\"TypeError\", \"runtime dict-variadic callable frame does not match its verified signature\")); goto argument_fail;\n"
            "    }\n",
            (unsigned long)fixedCount, (unsigned long)fixedCount,
            (unsigned long)signature->paramCount);
    else if ( arrayVariadic && fixedCount == 0 )
        xl6OutputPrintf(out,
            "    if (closure == NULL || frame == NULL || callResult == NULL || "
            "(frame->ArgumentCount != 0 && frame->Arguments == NULL) || "
            "(frame->KeywordCount != 0 && (frame->KeywordNames == NULL || frame->KeywordValues == NULL)) || "
            "frame->KeywordCount != 0 || "
            "(frame->KeywordCount != 0 && (closure->call_parameter_count != %lu || closure->call_parameter_names == NULL))) {\n"
            "        xrtSetErrorTake(xlir_error_create(\"TypeError\", \"runtime array-variadic callable frame does not match its verified signature\")); goto argument_fail;\n"
            "    }\n",
            (unsigned long)signature->paramCount);
    else if ( arrayVariadic )
        xl6OutputPrintf(out,
            "    if (closure == NULL || frame == NULL || callResult == NULL || "
            "(frame->ArgumentCount != 0 && frame->Arguments == NULL) || "
            "(frame->KeywordCount != 0 && (frame->KeywordNames == NULL || frame->KeywordValues == NULL)) || "
            "(frame->ArgumentCount >= %lu ? frame->KeywordCount != 0 : frame->KeywordCount > %lu - frame->ArgumentCount) || "
            "(frame->KeywordCount != 0 && (closure->call_parameter_count != %lu || closure->call_parameter_names == NULL))) {\n"
            "        xrtSetErrorTake(xlir_error_create(\"TypeError\", \"runtime array-variadic callable frame does not match its verified signature\")); goto argument_fail;\n"
            "    }\n",
            (unsigned long)fixedCount, (unsigned long)fixedCount,
            (unsigned long)signature->paramCount);
    else
        xl6OutputPrintf(out,
            "    if (closure == NULL || frame == NULL || callResult == NULL || "
            "frame->ArgumentCount > %lu || frame->KeywordCount > %lu - frame->ArgumentCount || "
            "(frame->ArgumentCount != 0 && frame->Arguments == NULL) || "
            "(frame->KeywordCount != 0 && (frame->KeywordNames == NULL || frame->KeywordValues == NULL)) || "
            "(frame->KeywordCount != 0 && (closure->call_parameter_count != %lu || closure->call_parameter_names == NULL))) {\n"
            "        xrtSetErrorTake(xlir_error_create(\"TypeError\", \"runtime callable frame does not match its verified signature\")); goto argument_fail;\n"
            "    }\n",
            (unsigned long)signature->paramCount, (unsigned long)signature->paramCount,
            (unsigned long)signature->paramCount);
    for ( i = 0; i < fixedCount; ++i )
        xl6OutputPrintf(out,
            "    if (frame->ArgumentCount > %lu) { f%lu = frame->Arguments[%lu]; bound%lu = true; }\n",
            (unsigned long)i, (unsigned long)i, (unsigned long)i,
            (unsigned long)i);
    if ( dictVariadic ) {
        const size_t variadicIndex = signature->paramCount - 1;
        xl6OutputPrintf(out,
            "    a%lu = xrtValueObject();\n"
            "    if (a%lu == NULL) goto argument_fail;\n",
            (unsigned long)variadicIndex, (unsigned long)variadicIndex);
    }
    if ( fixedCount != 0 || dictVariadic ) {
        xl6OutputPuts(
            "    if (frame->KeywordCount != 0) {\n"
            "        const xlir_xcall_name_view* keywordNames = (const xlir_xcall_name_view*)frame->KeywordNames;\n"
            "        size_t keywordIndex;\n"
            "        for (keywordIndex = 0; keywordIndex < frame->KeywordCount; ++keywordIndex) {\n"
            "            bool known = false; bool matched = false;\n"
            "            if (keywordNames[keywordIndex].Data == NULL || frame->KeywordValues[keywordIndex] == NULL) {\n"
            "                xrtSetErrorTake(xlir_error_create(\"TypeError\", \"runtime callable keyword argument is null\")); goto argument_fail;\n"
            "            }\n", out);
        for ( i = 0; i < fixedCount; ++i )
            xl6OutputPrintf(out,
                "            if (!known && "
                "strlen(closure->call_parameter_names[%lu]) == keywordNames[keywordIndex].Size && "
                "memcmp(closure->call_parameter_names[%lu], keywordNames[keywordIndex].Data, keywordNames[keywordIndex].Size) == 0) "
                "{ known = true; if (!bound%lu && frame->ArgumentCount <= %lu) "
                "{ f%lu = frame->KeywordValues[keywordIndex]; bound%lu = true; matched = true; } }\n",
                (unsigned long)i, (unsigned long)i,
                (unsigned long)i, (unsigned long)i, (unsigned long)i,
                (unsigned long)i);
        if ( dictVariadic ) {
            const size_t variadicIndex = signature->paramCount - 1;
            xl6OutputPrintf(out,
                "            if (!matched && !known) { xlir_string_view key = {keywordNames[keywordIndex].Data, keywordNames[keywordIndex].Size}; xvalue* captured;\n"
                "                if (xrtValueObjectGet(a%lu, key) != NULL) { xrtSetErrorTake(xlir_error_create(\"TypeError\", \"runtime callable keyword is duplicated\")); goto argument_fail; }\n"
                "                captured = xrtValueRetain(frame->KeywordValues[keywordIndex]);\n"
                "                if (captured == NULL || !xrtValueObjectSetNew(a%lu, key, captured)) goto argument_fail;\n"
                "                matched = true;\n"
                "            }\n",
                (unsigned long)variadicIndex,
                (unsigned long)variadicIndex);
        }
        xl6OutputPuts(
            "            if (!matched) { xrtSetErrorTake(xlir_error_create(\"TypeError\", \"runtime callable keyword duplicates a bound parameter or has no variadic destination\")); goto argument_fail; }\n"
            "        }\n"
            "    }\n", out);
    }
    for ( i = 0; i < fixedCount; ++i ) {
        const XL6_IrType* parameter = xl6IrTypeGet(&module->types, signature->params[i]);
        const XL6_IrCallableBridgeParameter* slot =
            &module->callableBridgeParameters[bridge->firstParameter + i];
        xl6OutputPrintf(out,
            "    if (bound%lu && f%lu == NULL) { xrtSetErrorTake(xlir_error_create(\"TypeError\", \"runtime callable argument binding is incomplete\")); goto argument_fail; }\n",
            (unsigned long)i, (unsigned long)i);
        if(slot->presenceAbi!=XL6_IR_CALLABLE_PRESENCE_NONE) {
            XL6_IrCallableBridgeResult decode={.type=slot->type,.valueType=slot->valueType,.presenceAbi=slot->presenceAbi,
                .decodeAbi=slot->decodeAbi,.integerDomain=slot->integerDomain,.nativeScalarProfile=slot->nativeScalarProfile};
            char output[64],input[64]; snprintf(output,sizeof(output),"a%lu",(unsigned long)i); snprintf(input,sizeof(input),"f%lu",(unsigned long)i);
            xl6OutputPrintf(out,"    if(bound%lu) {\n",(unsigned long)i);
            if(!xl6IrCgenEmitCallableBridgeDecode(out,module,&decode,output,input))return 0;
            xl6OutputPrintf(out,"    } else if(closure->call_parameter_count==%lu && closure->call_parameter_defaults && closure->call_parameter_defaults[%lu].abi==%d) { a%lu=",
                (unsigned long)signature->paramCount,(unsigned long)i,(int)XL6_IR_DYNAMIC_METHOD_DEFAULT_XVALUE_NULL,(unsigned long)i);
            if(slot->presenceAbi==XL6_IR_CALLABLE_PRESENCE_XVALUE_NULL)xl6OutputPuts("xrtValueRetain(xrtValueNull()); }\n",out);
            else xl6OutputPrintf(out,"(%s){0}; }\n",xl6IrCgenType(module,slot->type));
            XL6_IrCallableBridgeValueAbi payloadAbi=slot->decodeAbi;
            const char* field=payloadAbi==XL6_IR_CALLABLE_BRIDGE_BOOL?"boolean_value":payloadAbi==XL6_IR_CALLABLE_BRIDGE_CHAR?"char_value":
                payloadAbi==XL6_IR_CALLABLE_BRIDGE_I64 || payloadAbi==XL6_IR_CALLABLE_BRIDGE_TIME?"i64_value":payloadAbi==XL6_IR_CALLABLE_BRIDGE_U64?"u64_value":payloadAbi==XL6_IR_CALLABLE_BRIDGE_F64?"f64_value":NULL;
            int defaultAbi=payloadAbi==XL6_IR_CALLABLE_BRIDGE_BOOL?XL6_IR_DYNAMIC_METHOD_DEFAULT_BOOL:payloadAbi==XL6_IR_CALLABLE_BRIDGE_CHAR?XL6_IR_DYNAMIC_METHOD_DEFAULT_CHAR:
                payloadAbi==XL6_IR_CALLABLE_BRIDGE_I64?XL6_IR_DYNAMIC_METHOD_DEFAULT_I64:payloadAbi==XL6_IR_CALLABLE_BRIDGE_U64?XL6_IR_DYNAMIC_METHOD_DEFAULT_U64:
                payloadAbi==XL6_IR_CALLABLE_BRIDGE_F64?XL6_IR_DYNAMIC_METHOD_DEFAULT_F64:payloadAbi==XL6_IR_CALLABLE_BRIDGE_TIME?XL6_IR_DYNAMIC_METHOD_DEFAULT_TIME:0;
            if(field && slot->presenceAbi==XL6_IR_CALLABLE_PRESENCE_TAGGED) {
                xl6OutputPrintf(out,"    else if(closure->call_parameter_count==%lu && closure->call_parameter_defaults && closure->call_parameter_defaults[%lu].abi==%d) { a%lu.value=closure->call_parameter_defaults[%lu].%s; a%lu.hasValue=true;\n",
                    (unsigned long)signature->paramCount,(unsigned long)i,defaultAbi,(unsigned long)i,(unsigned long)i,field,(unsigned long)i);
                if(slot->integerDomain.rule!=XL6_VALUE_ADMIT_NONE)xl6OutputPrintf(out,"    if(!xl6ValueIntegerSlotAdmitsV1(&a%lu.value,%u))goto argument_fail;\n",(unsigned long)i,(unsigned)slot->integerDomain.rule);
                xl6OutputPuts("    }\n",out);
            } else if(slot->presenceAbi==XL6_IR_CALLABLE_PRESENCE_STRING_DATA) {
                xl6OutputPrintf(out,"    else if(closure->call_parameter_count==%lu && closure->call_parameter_defaults && closure->call_parameter_defaults[%lu].abi==%d) { a%lu=xlir_string_from_utf8(closure->call_parameter_defaults[%lu].string_value,closure->call_parameter_defaults[%lu].string_size); if(!a%lu.Data)goto argument_fail; }\n",
                    (unsigned long)signature->paramCount,(unsigned long)i,(int)XL6_IR_DYNAMIC_METHOD_DEFAULT_STRING,(unsigned long)i,(unsigned long)i,(unsigned long)i,(unsigned long)i);
            } else if(slot->presenceAbi==XL6_IR_CALLABLE_PRESENCE_XVALUE_NULL &&
                      (slot->decodeAbi==XL6_IR_CALLABLE_BRIDGE_BYTES_RETAIN || slot->decodeAbi==XL6_IR_CALLABLE_BRIDGE_BYTES_BORROW)) {
                xl6OutputPrintf(out,"    else if(closure->call_parameter_count==%lu && closure->call_parameter_defaults && closure->call_parameter_defaults[%lu].abi==%d) {\n"
                    "      default_bytes%lu=xrtValueBytes((xbytesview){(const unsigned char*)closure->call_parameter_defaults[%lu].string_value,closure->call_parameter_defaults[%lu].string_size});\n"
                    "      if(!default_bytes%lu) { goto argument_fail; }\n      a%lu=%sdefault_bytes%lu%s;\n"
                    "      if(!a%lu) { goto argument_fail; } }\n",
                    (unsigned long)signature->paramCount,(unsigned long)i,(int)XL6_IR_DYNAMIC_METHOD_DEFAULT_BYTES,
                    (unsigned long)i,(unsigned long)i,(unsigned long)i,(unsigned long)i,(unsigned long)i,
                    slot->decodeAbi==XL6_IR_CALLABLE_BRIDGE_BYTES_RETAIN ? "xrtValueRetain(" : "",(unsigned long)i,
                    slot->decodeAbi==XL6_IR_CALLABLE_BRIDGE_BYTES_RETAIN ? ")" : "",(unsigned long)i);
            }
            xl6OutputPuts("    else { xrtSetErrorKind(XERR_TYPE); goto argument_fail; }\n",out);
        } else if ( slot->decodeAbi == XL6_IR_CALLABLE_BRIDGE_XVALUE_BORROW ||
             slot->decodeAbi == XL6_IR_CALLABLE_BRIDGE_XVALUE_RETAIN ) {
            xl6OutputPrintf(out,
                "    if (bound%lu) a%lu = f%lu;\n"
                "    else if (closure->call_parameter_count == %lu && closure->call_parameter_defaults != NULL && closure->call_parameter_defaults[%lu].abi == %d) a%lu = xrtValueNull();\n"
                "    else { xrtSetErrorTake(xlir_error_create(\"TypeError\", \"runtime callable argument binding is incomplete\")); goto argument_fail; }\n",
                (unsigned long)i, (unsigned long)i, (unsigned long)i,
                (unsigned long)signature->paramCount, (unsigned long)i,
                (int)XL6_IR_DYNAMIC_METHOD_DEFAULT_XVALUE_NULL,
                (unsigned long)i);
            if (slot->decodeAbi == XL6_IR_CALLABLE_BRIDGE_XVALUE_RETAIN)
                xl6OutputPrintf(out,
                    "    a%lu = xrtValueRetain(a%lu); if (a%lu == NULL) goto argument_fail;\n",
                    (unsigned long)i, (unsigned long)i, (unsigned long)i);
        } else if ( slot->decodeAbi == XL6_IR_CALLABLE_BRIDGE_BYTES_BORROW ||
                    slot->decodeAbi == XL6_IR_CALLABLE_BRIDGE_BYTES_RETAIN ) {
            xl6OutputPrintf(out,
                "    if (bound%lu && xrtValueType(f%lu) == XVALUE_BYTES) a%lu = f%lu;\n"
                "    else if (!bound%lu && closure->call_parameter_count == %lu && closure->call_parameter_defaults && closure->call_parameter_defaults[%lu].abi == %d) {\n"
                "      default_bytes%lu = xrtValueBytes((xbytesview){(const unsigned char*)closure->call_parameter_defaults[%lu].string_value,closure->call_parameter_defaults[%lu].string_size});\n"
                "      if(!default_bytes%lu) { goto argument_fail; }\n      a%lu = default_bytes%lu; }\n"
                "    else { xrtSetErrorTake(xlir_error_create(\"TypeError\", \"runtime callable bytes argument does not match its verified slot ABI\")); goto argument_fail; }\n",
                (unsigned long)i, (unsigned long)i,
                (unsigned long)i, (unsigned long)i,(unsigned long)i,(unsigned long)signature->paramCount,(unsigned long)i,
                (int)XL6_IR_DYNAMIC_METHOD_DEFAULT_BYTES,(unsigned long)i,(unsigned long)i,(unsigned long)i,
                (unsigned long)i,(unsigned long)i,(unsigned long)i);
            if (slot->decodeAbi == XL6_IR_CALLABLE_BRIDGE_BYTES_RETAIN)
                xl6OutputPrintf(out,
                    "    a%lu = xrtValueRetain(a%lu); if (a%lu == NULL) goto argument_fail;\n",
                    (unsigned long)i, (unsigned long)i, (unsigned long)i);
        } else if ( slot->decodeAbi == XL6_IR_CALLABLE_BRIDGE_ERROR_REF ) {
            xl6OutputPrintf(out,
                "    if (bound%lu) { if (!xlir_xvalue_cast_checked(f%lu, 0x%016llXULL, %d, &a%lu)) goto argument_fail; }\n"
                "    else { xrtSetErrorTake(xlir_error_create(\"TypeError\", \"runtime callable error argument binding is incomplete\")); goto argument_fail; }\n",
                (unsigned long)i, (unsigned long)i,
                (unsigned long long)parameter->typeValueId,
                (int)XL6_IR_CAST_XVALUE_TO_ERROR_CHECKED,
                (unsigned long)i);
        } else if ( slot->decodeAbi == XL6_IR_CALLABLE_BRIDGE_POINTER ) {
            xl6OutputPrintf(out,
                "    if (bound%lu) { if (!xrtValueGetPointer(f%lu, &a%lu)) goto argument_fail; }\n"
                "    else if (closure->call_parameter_count == %lu && closure->call_parameter_defaults != NULL && closure->call_parameter_defaults[%lu].abi == %d) a%lu = NULL;\n"
                "    else { xrtSetErrorTake(xlir_error_create(\"TypeError\", \"runtime callable pointer argument binding is incomplete\")); goto argument_fail; }\n",
                (unsigned long)i, (unsigned long)i, (unsigned long)i,
                (unsigned long)signature->paramCount, (unsigned long)i,
                (int)XL6_IR_DYNAMIC_METHOD_DEFAULT_XVALUE_NULL,
                (unsigned long)i);
        } else if ( slot->decodeAbi == XL6_IR_CALLABLE_BRIDGE_STRING_COPY ) {
            xl6OutputPrintf(out,
                "    if (bound%lu) { xlir_string_view text = {0}; if (!xrtValueGetString(f%lu, &text) || (a%lu = xlir_string_from_utf8(text.Data, text.Size)).Data == NULL) goto argument_fail; }\n"
                "    else if (closure->call_parameter_count == %lu && closure->call_parameter_defaults != NULL && closure->call_parameter_defaults[%lu].abi == %d && (a%lu = xlir_string_from_utf8(closure->call_parameter_defaults[%lu].string_value, closure->call_parameter_defaults[%lu].string_size)).Data != NULL) { }\n"
                "    else { xrtSetErrorTake(xlir_error_create(\"TypeError\", \"runtime callable argument binding is incomplete\")); goto argument_fail; }\n",
                (unsigned long)i, (unsigned long)i, (unsigned long)i,
                (unsigned long)signature->paramCount, (unsigned long)i,
                (int)XL6_IR_DYNAMIC_METHOD_DEFAULT_STRING,
                (unsigned long)i, (unsigned long)i, (unsigned long)i);
        } else if (slot->decodeAbi == XL6_IR_CALLABLE_BRIDGE_NATIVE_SCALAR) {
            xl6OutputPrintf(out,
                "    if (!bound%lu) { xrtSetErrorTake(xlir_error_create(\"TypeError\", \"runtime callable native scalar binding is incomplete\")); goto argument_fail; }\n"
                "    if (!xl6ValueReadNativeScalarV1(f%lu, %u, 0x%016llXULL, &a%lu)) goto argument_fail;\n",
                (unsigned long)i, (unsigned long)i, (unsigned)slot->nativeScalarProfile,
                (unsigned long long)parameter->typeValueId, (unsigned long)i);
        } else if (slot->integerDomain.rule != XL6_VALUE_ADMIT_NONE) {
            const int wide = slot->integerDomain.rule == XL6_VALUE_ADMIT_U64;
            xl6OutputPrintf(out,
                "    if (bound%lu) { if (!xl6ValueReadIntegerV1(f%lu, %u, 0x%016llXULL, %s, &a%lu)) goto argument_fail; }\n"
                "    else if (closure->call_parameter_count == %lu && closure->call_parameter_defaults != NULL && closure->call_parameter_defaults[%lu].abi == %d) {\n"
                "        a%lu = closure->call_parameter_defaults[%lu].%s;\n"
                "        if (!xl6ValueIntegerSlotAdmitsV1(&a%lu, %u)) { xrtSetErrorKind(XERR_TYPE); goto argument_fail; }\n"
                "    } else { xrtSetErrorTake(xlir_error_create(\"TypeError\", \"runtime callable integer argument binding is incomplete\")); goto argument_fail; }\n",
                (unsigned long)i, (unsigned long)i, (unsigned)slot->integerDomain.rule,
                (unsigned long long)slot->integerDomain.identity, slot->integerDomain.nominal ? "true" : "false", (unsigned long)i,
                (unsigned long)signature->paramCount, (unsigned long)i,
                wide ? (int)XL6_IR_DYNAMIC_METHOD_DEFAULT_U64 : (int)XL6_IR_DYNAMIC_METHOD_DEFAULT_I64,
                (unsigned long)i, (unsigned long)i, wide ? "u64_value" : "i64_value",
                (unsigned long)i, (unsigned)slot->integerDomain.rule);
        } else if (slot->decodeAbi == XL6_IR_CALLABLE_BRIDGE_F32) {
            /* No numeric default opcode currently certifies an F32 literal.
             * Decode only bound values, without a double* -> float* cast. */
            xl6OutputPrintf(out,
                "    if (!bound%lu) { xrtSetErrorTake(xlir_error_create(\"TypeError\", \"runtime callable f32 argument binding is incomplete\")); goto argument_fail; }\n"
                "    if (!xl6ValueReadF32V1(f%lu, 0x%016llXULL, &a%lu)) goto argument_fail;\n",
                (unsigned long)i, (unsigned long)i,
                (unsigned long long)parameter->typeValueId, (unsigned long)i);
        } else {
            const char* getter = NULL;
            const char* field = NULL;
            XL6_IrDynamicMethodDefaultAbi defaultAbi = XL6_IR_DYNAMIC_METHOD_DEFAULT_INVALID;
            switch (slot->decodeAbi) {
            case XL6_IR_CALLABLE_BRIDGE_BOOL:
                getter = "xrtValueGetBool"; defaultAbi = XL6_IR_DYNAMIC_METHOD_DEFAULT_BOOL; field = "boolean_value"; break;
            case XL6_IR_CALLABLE_BRIDGE_CHAR:
                getter = "xrtValueGetChar"; defaultAbi = XL6_IR_DYNAMIC_METHOD_DEFAULT_CHAR; field = "char_value"; break;
            case XL6_IR_CALLABLE_BRIDGE_I64:
                getter = "xrtValueGetInt"; defaultAbi = XL6_IR_DYNAMIC_METHOD_DEFAULT_I64; field = "i64_value"; break;
            case XL6_IR_CALLABLE_BRIDGE_U64:
                getter = "xrtValueGetUInt"; defaultAbi = XL6_IR_DYNAMIC_METHOD_DEFAULT_U64; field = "u64_value"; break;
            case XL6_IR_CALLABLE_BRIDGE_F64:
                getter = "xrtValueGetFloat"; defaultAbi = XL6_IR_DYNAMIC_METHOD_DEFAULT_F64; field = "f64_value"; break;
            case XL6_IR_CALLABLE_BRIDGE_TIME:
                getter = "xrtValueGetTime"; defaultAbi = XL6_IR_DYNAMIC_METHOD_DEFAULT_TIME; field = "i64_value"; break;
            default: break;
            }
            if (!parameter || !getter || !field)
                return xl6IrCgenDiag(diags, module, XL6_IR_NODE_INVALID,
                    "XLIR C emitter runtime callable parameter has no closed scalar ABI");
            xl6OutputPrintf(out,
                "    if (bound%lu) { if (!%s(f%lu, &a%lu)) goto argument_fail; }\n"
                "    else if (closure->call_parameter_count == %lu && closure->call_parameter_defaults != NULL && closure->call_parameter_defaults[%lu].abi == %d) a%lu = closure->call_parameter_defaults[%lu].%s;\n"
                "    else { xrtSetErrorTake(xlir_error_create(\"TypeError\", \"runtime callable argument binding is incomplete\")); goto argument_fail; }\n",
                (unsigned long)i, getter, (unsigned long)i, (unsigned long)i,
                (unsigned long)signature->paramCount, (unsigned long)i,
                (int)defaultAbi, (unsigned long)i, (unsigned long)i, field);
        }
    }
    if ( arrayVariadic ) {
        const size_t variadicIndex = signature->paramCount - 1;
        const XL6_IrType* array = xl6IrTypeGet(&module->types,
            module->callableBridgeParameters[bridge->firstParameter + variadicIndex].type);
        if (!array || !array->typeValueId || !bridge->variadicElementIdentity ||
            bridge->variadicCarrierAbi != XL6_IR_COLLECTION_CARRIER_SEQUENCE_V1)
            return xl6IrCgenDiag(diags, module, XL6_IR_NODE_INVALID,
                "XLIR runtime callable variadic producer has no native Array contract");
        xl6OutputPrintf(out,
            "    a%lu = xl6ArrayCallFramePackV1(0x%016llXULL, 0x%016llXULL, frame, %lu);\n"
            "    if (a%lu == NULL) goto argument_fail;\n",
            (unsigned long)variadicIndex, (unsigned long long)array->typeValueId,
            (unsigned long long)bridge->variadicElementIdentity, (unsigned long)fixedCount,
            (unsigned long)variadicIndex);
    }
    for ( i = 0; i < bridge->parameterCount; ++i ) {
        const XL6_IrCallableBridgeParameter* slot =
            &module->callableBridgeParameters[bridge->firstParameter + i];
        if (slot->invokeAbi == XL6_IR_CALLBACK_INVOKE_TAKE)
            xl6OutputPrintf(out, "    %s invoked_a%lu = a%lu; a%lu = (%s){0};\n",
                xl6IrCgenType(module, slot->type), (unsigned long)i,
                (unsigned long)i, (unsigned long)i, xl6IrCgenType(module, slot->type));
    }
    if ( hasResult )
        xl6OutputPrintf(out, "    invoked = ((bool (*)(void*, %s*", resultType);
    else
        xl6OutputPuts("    invoked = ((bool (*)(void*", out);
    for ( i = 0; i < signature->paramCount; ++i ) {
        const char* parameterType = xl6IrCgenType(module, signature->params[i]);
        xl6OutputPrintf(out, ", %s", parameterType);
    }
    xl6OutputPuts("))closure->invoke)(closure->environment", out);
    if ( hasResult ) xl6OutputPuts(", &value", out);
    for ( i = 0; i < signature->paramCount; ++i )
        xl6OutputPrintf(out, ", %sa%lu",
            module->callableBridgeParameters[bridge->firstParameter + i].invokeAbi ==
                XL6_IR_CALLBACK_INVOKE_TAKE ? "invoked_" : "", (unsigned long)i);
    xl6OutputPuts(");\n", out);
    if (!xl6IrCgenEmitCallableBridgeCleanup(out, module, bridge, 0, "!invoked", "invoked")) return 0;
    /* Finish every language-owned cleanup transaction before publishing the
     * native callback diagnostic. Publishing first would let an inherited
     * cleanup outcome replace the callback view with the original throw. */
    xl6OutputPuts("    if (!invoked || xrtGetError() != NULL) goto result_fail;\n", out);
    for ( i = 0; i < bridge->resultCount; ++i ) {
        const XL6_IrCallableBridgeResult* slot = &module->callableBridgeResults[bridge->firstResult + i];
        char place[64];
        if (bridge->resultCount == 1) snprintf(place, sizeof(place), "value");
        else snprintf(place, sizeof(place), "value.e%lu", (unsigned long)slot->result);
        if (!xl6IrCgenEmitCallableBridgeEncode(out, module, slot->type,slot->valueType,slot->presenceAbi, slot->encodeAbi, slot->integerDomain, slot->nativeScalarProfile, place, "boxed"))
            return xl6IrCgenDiag(diags, module, XL6_IR_NODE_INVALID, "XLIR callable result encoding plan is unresolved");
        xl6OutputPrintf(out,
            "    if (boxed == NULL) goto result_fail;\n"
            "    if (!xrtCallResultSetTake(&bridge_results, %lu, &boxed)) goto result_fail;\n", (unsigned long)i);
    }
    if (!xl6IrCgenEmitCallableResultCleanup(out, module, bridge, "false", "invoked", NULL, 0, hasResult, 0)) return 0;
    xl6OutputPuts("    if (!invoked) goto result_fail;\n", out);
    if (!xl6IrCgenEmitCallableResultCleanup(out, module, bridge, "false", "invoked", "callResult", 0, 0, 0)) return 0;
    xl6OutputPuts(
        "    if (!invoked || !xrtCallResultMove(callResult, &bridge_results)) goto result_fail;\n"
        "    return true;\n"
        "result_fail:\n", out);
    if (!xl6IrCgenEmitCallableResultCleanup(out, module, bridge, "true", "invoked", "&bridge_results", 0, hasResult, hasResult)) return 0;
    xl6OutputPuts("    xl6ForeignCallbackFail();\n    return false;\n", out);
    xl6OutputPuts("conversion_fail:\nargument_fail: {\n", out);
    if (!xl6IrCgenEmitCallableBridgeCleanup(out, module, bridge, 0, "true", "invoked")) return 0;
    xl6OutputPuts("    xl6ForeignCallbackFail();\n    return false;\n}\n}\n", out);
    return !xl6OutputHasFailed(out);
}

/* Reify one verified runtime callable signature back into XLIR's native
 * closure ABI.  This is the inverse adapter of xlir_xcall_entry_tN: VALUE
 * parameters are boxed, open-dynamic BORROW parameters are retained for the
 * consuming call frame, and an open-dynamic result transfers directly. */
static int xl6IrCgenEmitRuntimeCallableClosureEntry(
    XL6_Output* out, const XL6_IrModule* module, XL6_IrTypeId type,
    XL6_DiagList* diags)
{
    const XL6_IrType* signature = xl6IrTypeGet(&module->types, type);
    const XL6_IrCallableBridge* bridge = xl6IrCallableBridgeGet(module, type);
    const XL6_IrType* result = NULL;
    const char* resultType = NULL;
    int hasResult;
    size_t i;
    if ( bridge == NULL || !bridge->reverseSupported || signature == NULL )
        return xl6IrCgenDiag(diags, module, XL6_IR_NODE_INVALID,
            "XLIR C emitter runtime callable-to-closure signature is unresolved");
    hasResult = signature->returnCount != 0;
    if ( hasResult &&
         ((result = xl6IrTypeGet(
               &module->types, signature->callableResultType)) == NULL ||
          (resultType = xl6IrCgenType(
               module, signature->callableResultType)) == NULL) )
        return xl6IrCgenDiag(diags, module, XL6_IR_NODE_INVALID,
            "XLIR C emitter runtime callable-to-closure result carrier is unresolved");
    xl6OutputPrintf(out,
        "static bool xlir_xvalue_closure_entry_t%u(void* environment",
        (unsigned int)type);
    if ( hasResult ) xl6OutputPrintf(out, ", %s* output", resultType);
    for ( i = 0; i < signature->paramCount; ++i ) {
        const char* parameterType = xl6IrCgenType(module, signature->params[i]);
        if ( parameterType == NULL )
            return xl6IrCgenDiag(diags, module, XL6_IR_NODE_INVALID,
                "XLIR C emitter runtime callable-to-closure parameter is unresolved");
        xl6OutputPrintf(out, ", %s a%lu", parameterType, (unsigned long)i);
    }
    xl6OutputPuts(")\n{\n", out);
    if ( signature->paramCount != 0 )
        xl6OutputPrintf(out, "    xvalue* arguments[%lu] = {0};\n",
                        (unsigned long)signature->paramCount);
    xl6OutputPuts(
        "    xrtcallframe callFrame = {0}; xrtcallresult callResults = {0}; bool success = false;",
        out);
    if ( hasResult ) {
        if ( result->cAbi == XL6_IR_C_ABI_PRODUCT || result->cAbi==XL6_IR_C_ABI_TAGGED_NULLABLE ||
             result->cAbi == XL6_IR_C_ABI_MANAGED_STRING )
            xl6OutputPrintf(out, " xvalue* resultValue; %s value = {0};",
                resultType);
        else
            xl6OutputPrintf(out, " xvalue* resultValue; %s value = (%s)0;",
                resultType, resultType);
    }
    xl6OutputPuts("\n    if (environment == NULL", out);
    if ( hasResult ) xl6OutputPuts(" || output == NULL", out);
    xl6OutputPuts(
        ") { xrtSetErrorTake(xlir_error_create(\"TypeError\", \"runtime callable closure has no environment or result storage\")); goto argument_fail; }\n",
        out);
    for ( i = 0; i < bridge->parameterCount; ++i ) {
        const XL6_IrCallableBridgeParameter* slot =
            &module->callableBridgeParameters[bridge->firstParameter + i];
        char input[64], argument[64];
        snprintf(input, sizeof(input), "a%lu", (unsigned long)i);
        snprintf(argument, sizeof(argument), "arguments[%lu]", (unsigned long)i);
        if (!xl6IrCgenEmitCallableBridgeEncode(out, module, slot->type,slot->valueType,slot->presenceAbi, slot->encodeAbi, slot->integerDomain, slot->nativeScalarProfile, input, argument))
            return xl6IrCgenDiag(diags, module, XL6_IR_NODE_INVALID, "XLIR callable parameter encoding plan is unresolved");
        xl6OutputPrintf(out, "    if (%s == NULL) goto argument_fail;\n", argument);
    }
    xl6OutputPrintf(out,
        "    callFrame.ArgumentCount = %lu; callFrame.Arguments = %s;\n"
        "    { xl6_foreign_error_scope invocation = {0};\n"
        "      if (xl6ForeignCallBegin(&invocation)) {\n"
        "        success = xrtValueInvoke(*(xvalue* const*)environment, &callFrame, &callResults);\n"
        "        if (!success) xl6ErrorScopeCaptureFailure(&invocation);\n"
        "        { xl6_foreign_error_result outcome = xl6ForeignCallEnd(&invocation);\n"
        "          success = outcome.ok; xrtSetErrorTake(outcome.error); }\n"
        "      } }\n",
        (unsigned long)signature->paramCount,
        signature->paramCount != 0 ? "arguments" : "NULL");
    if (!xl6IrCgenEmitCallableBridgeCleanup(out, module, bridge, 1, "!success", "success")) return 0;
    xl6OutputPuts("    if (xrtGetError() != NULL) success = false;\n", out);
    xl6OutputPuts(
        "    if (!success) {\n"
        "        xerror* callError = xrtTakeError(); const xerror* cause = xrtErrorCause(callError);\n"
        "        xerror* projected = cause != NULL ? xrtErrorRef(cause) : NULL;\n"
        "        if (projected != NULL) { xrtErrorFree(callError); xrtSetErrorTake(projected); }\n"
        "        else if (callError != NULL) xrtSetErrorTake(callError);\n", out);
    if (!xl6IrCgenEmitCallableResultCleanup(out, module, bridge, "true", "success", "&callResults", 1, hasResult, 0)) return 0;
    xl6OutputPuts("        return false;\n    }\n", out);
    xl6OutputPrintf(out,
        "    if (xrtCallResultCount(&callResults) != %lu) { xrtSetErrorTake(xlir_error_create(\"TypeError\", \"runtime callable result count does not match its verified signature\")); goto conversion_fail; }\n",
        (unsigned long)bridge->resultCount);
    for ( i = 0; i < bridge->resultCount; ++i ) {
        const XL6_IrCallableBridgeResult* slot = &module->callableBridgeResults[bridge->firstResult + i];
        char place[64];
        if (bridge->resultCount == 1) snprintf(place, sizeof(place), "value");
        else snprintf(place, sizeof(place), "value.e%lu", (unsigned long)slot->result);
        xl6OutputPrintf(out,
            "    resultValue = xrtCallResultGet(&callResults, %lu); if (resultValue == NULL) goto conversion_fail;\n", (unsigned long)i);
        if (!xl6IrCgenEmitCallableBridgeDecode(out, module, slot, place, "resultValue"))
            return xl6IrCgenDiag(diags, module, XL6_IR_NODE_INVALID, "XLIR callable result decoding plan is unresolved");
    }
    if (!xl6IrCgenEmitCallableResultCleanup(out, module, bridge, "false", "success", "&callResults", 1, 0, 0)) return 0;
    xl6OutputPuts("    if (!success) goto conversion_fail;\n", out);
    if (hasResult) xl6OutputPuts("    *output = value;\n", out);
    xl6OutputPuts(
        "    return true;\n"
        "conversion_fail:\n"
        "    if (xrtGetError() == NULL) xrtSetErrorTake(xlir_error_create(\"TypeError\", \"runtime callable result does not match its verified slot ABI\"));\n", out);
    if (!xl6IrCgenEmitCallableResultCleanup(out, module, bridge, "true", "success", "&callResults", 1, hasResult, 0)) return 0;
    xl6OutputPuts("    return false;\n", out);
    xl6OutputPuts("argument_fail:\n", out);
    if (!xl6IrCgenEmitCallableBridgeCleanup(out, module, bridge, 1, "true", "success")) return 0;
    xl6OutputPuts("    return false;\n}\n", out);
    return !xl6OutputHasFailed(out);
}
