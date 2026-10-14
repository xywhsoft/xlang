/* Execute closed bridge opcodes. Type IDs supply physical spelling/identity,
 * never an ownership or conversion decision in the renderer. */
static int xl6IrCgenEmitCallableBridgeEncode(
    XL6_Output* out, const XL6_IrModule* module, XL6_IrTypeId type,
    XL6_IrTypeId valueType, XL6_IrCallableBridgePresenceAbi presence,
    XL6_IrCallableBridgeValueAbi abi, XL6_IrCallableBridgeIntegerDomain domain,
    XL6_NativeScalarProfileV1 nativeProfile,
    const char* input, const char* boxed)
{
    const XL6_IrType* item = xl6IrTypeGet(&module->types, type);
    if (!item) return 0;
    if(presence!=XL6_IR_CALLABLE_PRESENCE_NONE) {
        char payload[96];
        snprintf(payload,sizeof(payload),presence==XL6_IR_CALLABLE_PRESENCE_TAGGED?"%s.value":"%s",input);
        if(presence==XL6_IR_CALLABLE_PRESENCE_XVALUE_NULL)xl6OutputPrintf(out,"    if(%s != NULL && xrtValueType(%s)!=XVALUE_NULL) {\n",input,input);
        else xl6OutputPrintf(out,"    if(%s%s) {\n",input,
            presence==XL6_IR_CALLABLE_PRESENCE_TAGGED?".hasValue":presence==XL6_IR_CALLABLE_PRESENCE_STRING_DATA?".Data != NULL":" != NULL");
        if(!xl6IrCgenEmitCallableBridgeEncode(out,module,valueType,valueType,XL6_IR_CALLABLE_PRESENCE_NONE,abi,domain,nativeProfile,payload,boxed))return 0;
        if(presence==XL6_IR_CALLABLE_PRESENCE_XVALUE_NULL) {
            xl6OutputPrintf(out,"    } else { %s = %s != NULL ? xrtValueRetain(xrtValueNull()) : NULL;\n",boxed,input);
            if(abi==XL6_IR_CALLABLE_BRIDGE_BYTES_MOVE)xl6OutputPrintf(out,"    if(%s != NULL)%s = NULL;\n",boxed,input);
            xl6OutputPuts("    }\n",out);
        } else xl6OutputPrintf(out,"    } else %s = xrtValueRetain(xrtValueNull());\n",boxed);
        return !xl6OutputHasFailed(out);
    }
    if (abi == XL6_IR_CALLABLE_BRIDGE_NATIVE_SCALAR) {
        xl6OutputPrintf(out, "    %s = xl6ValueBoxNativeScalarV1(&%s, %u, 0x%016llXULL);\n",
            boxed, input, (unsigned)nativeProfile, (unsigned long long)item->typeValueId);
        return !xl6OutputHasFailed(out);
    }
    if (domain.rule != XL6_VALUE_ADMIT_NONE) {
        xl6OutputPrintf(out, "    %s = xl6ValueBoxIntegerV1(&%s, %u, 0x%016llXULL, %s);\n",
            boxed, input, (unsigned)domain.rule, (unsigned long long)domain.identity,
            domain.nominal ? "true" : "false");
        return !xl6OutputHasFailed(out);
    }
    switch (abi) {
    case XL6_IR_CALLABLE_BRIDGE_XVALUE_MOVE:
        xl6OutputPrintf(out, "    %s = %s; %s = NULL;\n", boxed, input, input); break;
    case XL6_IR_CALLABLE_BRIDGE_XVALUE_RETAIN:
        xl6OutputPrintf(out, "    %s = %s != NULL ? xrtValueRetain(%s) : NULL;\n", boxed, input, input); break;
    case XL6_IR_CALLABLE_BRIDGE_BYTES_MOVE:
        xl6OutputPrintf(out,
            "    %s = %s != NULL && xrtValueType(%s) == XVALUE_BYTES ? %s : NULL; if (%s != NULL) %s = NULL;\n",
            boxed, input, input, input, boxed, input); break;
    case XL6_IR_CALLABLE_BRIDGE_BYTES_RETAIN:
        xl6OutputPrintf(out,
            "    %s = %s != NULL && xrtValueType(%s) == XVALUE_BYTES ? xrtValueRetain(%s) : NULL;\n",
            boxed, input, input, input); break;
    case XL6_IR_CALLABLE_BRIDGE_STRING_COPY:
        xl6OutputPrintf(out, "    %s = %s.Data != NULL ? xrtValueString(%s) : NULL;\n", boxed, input, input); break;
    case XL6_IR_CALLABLE_BRIDGE_ERROR_REF:
        xl6OutputPrintf(out, "    %s = xl6ErrorValueRefV1(%s, 0x%016llXULL);\n",
            boxed, input, (unsigned long long)item->typeValueId); break;
    case XL6_IR_CALLABLE_BRIDGE_CLOSURE_REF:
        xl6OutputPrintf(out, "    %s = %s != NULL ? xlir_closure_to_xvalue_scalar(%s, xlir_xcall_entry_t%u, 0x%016llXULL) : NULL;\n",
            boxed, input, input, (unsigned)type, (unsigned long long)item->typeValueId); break;
    case XL6_IR_CALLABLE_BRIDGE_BOOL:
        xl6OutputPrintf(out, "    %s = xrtValueRetain(xrtValueBool(%s));\n", boxed, input); break;
    case XL6_IR_CALLABLE_BRIDGE_CHAR:
        xl6OutputPrintf(out, "    %s = xrtValueChar(%s);\n", boxed, input); break;
    case XL6_IR_CALLABLE_BRIDGE_I64:
        xl6OutputPrintf(out, "    %s = xrtValueInt(%s);\n", boxed, input); break;
    case XL6_IR_CALLABLE_BRIDGE_U64:
        xl6OutputPrintf(out, "    %s = xrtValueUInt(%s);\n", boxed, input); break;
    case XL6_IR_CALLABLE_BRIDGE_F64:
    case XL6_IR_CALLABLE_BRIDGE_F32:
        xl6OutputPrintf(out, "    %s = xrtValueFloat(%s);\n", boxed, input); break;
    case XL6_IR_CALLABLE_BRIDGE_TIME:
        xl6OutputPrintf(out, "    %s = xrtValueTime(%s);\n", boxed, input); break;
    case XL6_IR_CALLABLE_BRIDGE_POINTER:
        xl6OutputPrintf(out, "    %s = xrtValuePointer(%s);\n", boxed, input); break;
    default: return 0;
    }
    return !xl6OutputHasFailed(out);
}

static int xl6IrCgenEmitCallableBridgeDecode(
    XL6_Output* out, const XL6_IrModule* module,
    const XL6_IrCallableBridgeResult* slot, const char* output, const char* boxed)
{
    const XL6_IrType* item = xl6IrTypeGet(&module->types, slot->type);
    const char* getter = NULL;
    if (!item) return 0;
    if(slot->presenceAbi!=XL6_IR_CALLABLE_PRESENCE_NONE) {
        XL6_IrCallableBridgeResult child=*slot;
        child.type=slot->valueType; child.presenceAbi=XL6_IR_CALLABLE_PRESENCE_NONE;
        char payload[96]; snprintf(payload,sizeof(payload),slot->presenceAbi==XL6_IR_CALLABLE_PRESENCE_TAGGED?"%s.value":"%s",output);
        if(slot->presenceAbi==XL6_IR_CALLABLE_PRESENCE_XVALUE_NULL)xl6OutputPrintf(out,"    if(%s != NULL && xrtValueType(%s)==XVALUE_NULL) %s = xrtValueRetain(xrtValueNull());\n    else {\n",boxed,boxed,output);
        else xl6OutputPrintf(out,"    if(%s != NULL && xrtValueType(%s)==XVALUE_NULL) %s = (%s){0};\n    else {\n",boxed,boxed,output,xl6IrCgenType(module,slot->type));
        if(!xl6IrCgenEmitCallableBridgeDecode(out,module,&child,payload,boxed))return 0;
        if(slot->presenceAbi==XL6_IR_CALLABLE_PRESENCE_TAGGED)xl6OutputPrintf(out,"    %s.hasValue=true;\n",output);
        xl6OutputPuts("    }\n",out); return !xl6OutputHasFailed(out);
    }
    if (slot->decodeAbi == XL6_IR_CALLABLE_BRIDGE_NATIVE_SCALAR) {
        xl6OutputPrintf(out, "    if (!xl6ValueReadNativeScalarV1(%s, %u, 0x%016llXULL, &%s)) goto conversion_fail;\n",
            boxed, (unsigned)slot->nativeScalarProfile, (unsigned long long)item->typeValueId, output);
        return !xl6OutputHasFailed(out);
    }
    if (slot->integerDomain.rule != XL6_VALUE_ADMIT_NONE) {
        xl6OutputPrintf(out, "    if (!xl6ValueReadIntegerV1(%s, %u, 0x%016llXULL, %s, &%s)) goto conversion_fail;\n",
            boxed, (unsigned)slot->integerDomain.rule, (unsigned long long)slot->integerDomain.identity,
            slot->integerDomain.nominal ? "true" : "false", output);
        return !xl6OutputHasFailed(out);
    }
    switch (slot->decodeAbi) {
    case XL6_IR_CALLABLE_BRIDGE_XVALUE_BORROW:
        xl6OutputPrintf(out,"    %s = %s; if(%s == NULL)goto conversion_fail;\n",output,boxed,output); break;
    case XL6_IR_CALLABLE_BRIDGE_XVALUE_RETAIN:
        xl6OutputPrintf(out, "    %s = xrtValueRetain(%s); if (%s == NULL) goto conversion_fail;\n", output, boxed, output); break;
    case XL6_IR_CALLABLE_BRIDGE_BYTES_RETAIN:
        xl6OutputPrintf(out,
            "    if (%s == NULL || xrtValueType(%s) != XVALUE_BYTES) { xrtSetErrorKind(XERR_TYPE); goto conversion_fail; } if ((%s = xrtValueRetain(%s)) == NULL) goto conversion_fail;\n",
            boxed, boxed, output, boxed); break;
    case XL6_IR_CALLABLE_BRIDGE_BYTES_BORROW:
        xl6OutputPrintf(out,"    if(%s == NULL || xrtValueType(%s)!=XVALUE_BYTES) { xrtSetErrorKind(XERR_TYPE); goto conversion_fail; } %s = %s;\n",boxed,boxed,output,boxed); break;
    case XL6_IR_CALLABLE_BRIDGE_STRING_COPY:
        xl6OutputPrintf(out, "    { xlir_string_view text = {0}; if (!xrtValueGetString(%s, &text) || (%s = xlir_string_from_utf8(text.Data, text.Size)).Data == NULL) goto conversion_fail; }\n", boxed, output); break;
    case XL6_IR_CALLABLE_BRIDGE_ERROR_REF:
        xl6OutputPrintf(out, "    if (!xlir_xvalue_cast_checked(%s, 0x%016llXULL, %d, &%s)) goto conversion_fail;\n",
            boxed, (unsigned long long)item->typeValueId, (int)XL6_IR_CAST_XVALUE_TO_ERROR_CHECKED, output); break;
    case XL6_IR_CALLABLE_BRIDGE_CLOSURE_COPY:
        xl6OutputPrintf(out, "    %s = xlir_xvalue_project_closure_try_copy(%s, (void (*)(void))xlir_xvalue_closure_entry_t%u);\n"
            "    if (%s == NULL) goto conversion_fail;\n", output, boxed, (unsigned)slot->type, output); break;
    case XL6_IR_CALLABLE_BRIDGE_BOOL: getter = "xrtValueGetBool"; break;
    case XL6_IR_CALLABLE_BRIDGE_CHAR: getter = "xrtValueGetChar"; break;
    case XL6_IR_CALLABLE_BRIDGE_I64: getter = "xrtValueGetInt"; break;
    case XL6_IR_CALLABLE_BRIDGE_U64: getter = "xrtValueGetUInt"; break;
    case XL6_IR_CALLABLE_BRIDGE_F64: getter = "xrtValueGetFloat"; break;
    case XL6_IR_CALLABLE_BRIDGE_TIME: getter = "xrtValueGetTime"; break;
    case XL6_IR_CALLABLE_BRIDGE_F32:
        xl6OutputPrintf(out, "    if (!xl6ValueReadF32V1(%s, 0x%016llXULL, &%s)) goto conversion_fail;\n",
            boxed, (unsigned long long)item->typeValueId, output); break;
    case XL6_IR_CALLABLE_BRIDGE_POINTER: getter = "xrtValueGetPointer"; break;
    default: return 0;
    }
    if (getter) xl6OutputPrintf(out, "    if (!%s(%s, &%s)) goto conversion_fail;\n", getter, boxed, output);
    return !xl6OutputHasFailed(out);
}

static int xl6IrCgenEmitCallableNativeResultsDrop(
    XL6_Output* out, const XL6_IrModule* module, const XL6_IrCallableBridge* bridge)
{
    size_t i;
    for (i = bridge->resultCount; i > 0; --i) {
        const XL6_IrCallableBridgeResult* slot = &module->callableBridgeResults[bridge->firstResult + i - 1];
        const char* destroy;
        char place[64];
        if (slot->destroyAbi == XL6_IR_ASYNC_FRAME_DESTROY_NONE) continue;
        destroy = xl6IrCgenCallbackDestroy(slot->destroyAbi); if (!destroy) return 0;
        if (bridge->resultCount == 1) snprintf(place, sizeof(place), "value");
        else snprintf(place, sizeof(place), "value.e%lu", (unsigned long)slot->result);
        if (!xl6IrCgenEmitDetachedRelease(out, xl6IrCgenType(module, slot->type), destroy, place, NULL)) return 0;
        xl6OutputPuts("    if (xrtGetError() != NULL) xl6ErrorScopeCaptureFailure(&result_cleanup);\n", out);
    }
    return !xl6OutputHasFailed(out);
}

/* Each boxed frame is detached with the public Move API. Replacing an existing
 * slot by the static null singleton allocates nothing, detaches before Drop,
 * and lets the transaction observe every destructor independently. */
static void xl6IrCgenEmitCallableFrameDrain(XL6_Output* out, const char* frame)
{
    xl6OutputPrintf(out,
        "    { xrtcallresult owned_results = {0}; size_t remaining;\n"
        "      if (!xrtCallResultMove(&owned_results, %s)) abort();\n"
        "      remaining = xrtCallResultCount(&owned_results);\n"
        "      while (remaining != 0) {\n"
        "        if (!xrtCallResultSet(&owned_results, --remaining, xrtValueNull())) xl6ErrorScopeCaptureFailure(&result_cleanup);\n"
        "        if (xrtGetError() != NULL) xl6ErrorScopeCaptureFailure(&result_cleanup);\n"
        "      }\n"
        "      xrtCallResultUnit(&owned_results);\n"
        "      if (xrtGetError() != NULL) xl6ErrorScopeCaptureFailure(&result_cleanup); }\n", frame);
}

static int xl6IrCgenEmitCallableResultCleanup(
    XL6_Output* out, const XL6_IrModule* module, const XL6_IrCallableBridge* bridge,
    const char* failed, const char* outcome, const char* frame, int nativeFirst, int native, int boxed)
{
    if (bridge->resultCommitAbi != XL6_IR_CALLABLE_RESULT_COMMIT_AFTER_CLEANUP ||
        bridge->frameCleanupAbi != XL6_IR_CALLABLE_FRAME_CLEANUP_REPLACE_REVERSE ||
        bridge->cleanupAbi != XL6_IR_CALLBACK_CLEANUP_TRANSACTIONAL || bridge->errorAbi != XL6_IR_CALLBACK_ERROR_PRESERVE) return 0;
    xl6OutputPrintf(out,
        "    { xl6_error_scope result_cleanup = {0}; xl6_error_result result_outcome;\n"
        "      if (!xl6ErrorScopeBeginCleanup(&result_cleanup, %s)) abort();\n", failed);
    if (native && nativeFirst && !xl6IrCgenEmitCallableNativeResultsDrop(out, module, bridge)) return 0;
    if (boxed) xl6OutputPuts(
        "    { xvalue* released = boxed; boxed = NULL; xrtValueRelease(released); }\n"
        "    if (xrtGetError() != NULL) xl6ErrorScopeCaptureFailure(&result_cleanup);\n", out);
    if (frame) xl6IrCgenEmitCallableFrameDrain(out, frame);
    if (native && !nativeFirst && !xl6IrCgenEmitCallableNativeResultsDrop(out, module, bridge)) return 0;
    xl6OutputPrintf(out,
        "      result_outcome = xl6ErrorScopeEnd(&result_cleanup);\n"
        "      xrtSetErrorTake(result_outcome.error); %s = result_outcome.ok; }\n", outcome);
    return !xl6OutputHasFailed(out);
}
