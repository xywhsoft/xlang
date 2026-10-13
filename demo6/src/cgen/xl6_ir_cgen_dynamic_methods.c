static int xl6IrCgenEmitExternPrototype(XL6_Output*, const XL6_IrModule*, const XL6_IrCallTarget*, XL6_DiagList*);

static int xl6IrCgenDynamicMethodNeeded(
    const XL6_IrModule* module, size_t methodIndex)
{
    const XL6_IrDynamicMethod* method;
    const XL6_IrCallTarget* target;
    size_t instructionIndex;
    if ( module == NULL || methodIndex >= module->dynamicMethodCount ) return 0;
    method = &module->dynamicMethods[methodIndex];
    target = xl6IrCallTargetGet(module, method->targetSymbol);
    if ( method->adapterAbi != XL6_IR_DYNAMIC_METHOD_ADAPTER_VERIFIED ||
         target == NULL || !xl6IrDynamicMethodCallTargetSupported(module, target) ||
         target->reachability == XL6_IR_REACHABILITY_UNREACHABLE ) return 0;
    for ( instructionIndex = 0;
          instructionIndex < module->instructionCount; ++instructionIndex ) {
        const XL6_IrInstruction* instruction =
            &module->instructions[instructionIndex];
        const XL6_IrDynamicCallSite* site;
        if ( !xl6IrCgenInstructionIsReachable(module, instructionIndex) ||
             instruction->kind != XL6_IR_INSTR_RUNTIME_CALL ||
             instruction->runtimeOp !=
                 XL6_IR_RUNTIME_XVALUE_DYNAMIC_METHOD_CALL ||
             instruction->dynamicCallSite >= module->dynamicCallSiteCount )
            continue;
        site = &module->dynamicCallSites[instruction->dynamicCallSite];
        if ( xl6IrDynamicMethodMatchesCallSite(site, method) ) return 1;
    }
    return 0;
}

static void xl6IrCgenEmitDynamicMethodError(
    XL6_Output* out, const char* message)
{
    xl6OutputPuts(
        "if (xrtGetError() == NULL) xrtSetErrorTake(xlir_error_create(\"TypeError\", ",
        out);
    xl6IrCgenEmitCStringLiteral(out, message);
    xl6OutputPuts(")); goto fail;", out);
}

/* A matcher only selects an overload. Reuse the resident, allocation-free
 * language predicates; never call a converting getter to probe a candidate. */
static int xl6IrCgenEmitDynamicArgumentAdmission(
    XL6_Output* out, const XL6_IrModule* module,
    const XL6_IrDynamicMethodParameter* item, size_t parameter)
{
    const XL6_IrType* type = xl6IrTypeGet(&module->types, item->type);
    if ( type == NULL ) return 0;
    if ( item->matchAbi == XL6_IR_DYNAMIC_METHOD_MATCH_NOMINAL ) {
        XL6_IrTypeId base = type->kind == XL6_IR_TYPE_NULLABLE
            ? type->params[0] : item->type;
        XL6_IrNominalIdentitySetId setId = XL6_IR_NOMINAL_IDENTITY_SET_INVALID;
        if ( xl6IrNominalIdentitySetGet(module, base, &setId) == NULL ) return 0;
        xl6OutputPrintf(out,
            "xl6ValueAdmitsNominalV1(bound%lu, &xlir_nominal_set_%u, %s)",
            (unsigned long)parameter, (unsigned int)setId,
            type->kind == XL6_IR_TYPE_NULLABLE ? "true" : "false");
    } else if ( item->matchAbi == XL6_IR_DYNAMIC_METHOD_MATCH_EXACT_IDENTITY ) {
        const XL6_IrType* base = type->kind == XL6_IR_TYPE_NULLABLE
            ? xl6IrTypeGet(&module->types, type->params[0]) : type;
        if ( base == NULL || base->typeValueId == 0 ) return 0;
        if ( type->kind == XL6_IR_TYPE_NULLABLE )
            xl6OutputPrintf(out, "(xrtValueType(bound%lu) == XVALUE_NULL || ",
                (unsigned long)parameter);
        xl6OutputPrintf(out, "xrtValueTypeId(bound%lu) == 0x%016llXULL",
            (unsigned long)parameter, (unsigned long long)base->typeValueId);
        if ( type->kind == XL6_IR_TYPE_NULLABLE ) xl6OutputPuts(")", out);
    } else {
        if ( type->xvalueAdmissionAbi == XL6_VALUE_ADMIT_NONE ) return 0;
        xl6OutputPrintf(out, "xl6ValueAdmitsV1(bound%lu, %u, %s, 0x%016llXULL)",
            (unsigned long)parameter, (unsigned int)type->xvalueAdmissionAbi,
            type->xvalueAdmissionNullable ? "true" : "false",
            (unsigned long long)type->xvalueAdmissionTypeValueId);
    }
    return !xl6OutputHasFailed(out);
}

static int xl6IrCgenEmitDynamicMethodMatcher(
    XL6_Output* out, const XL6_IrModule* module, size_t methodIndex,
    XL6_DiagList* diags)
{
    const XL6_IrDynamicMethod* method =
        methodIndex < module->dynamicMethodCount
            ? &module->dynamicMethods[methodIndex] : NULL;
    size_t parameter;
    if ( method == NULL || method->adapterAbi !=
             XL6_IR_DYNAMIC_METHOD_ADAPTER_VERIFIED )
        return xl6IrCgenDiag(diags, module, XL6_IR_NODE_INVALID,
            "XLIR C emitter dynamic method matcher is unresolved");
    xl6OutputPrintf(out,
        "static bool xlir_dynamic_method_match_dm%lu(size_t positionalCount, xvalue** positionalValues, size_t keywordCount, const xlir_string_view* keywordNames, xvalue** keywordValues)\n"
        "{\n"
        "    size_t keywordIndex;\n",
        (unsigned long)methodIndex);
    for ( parameter = 0; parameter < method->fixedParameterCount; ++parameter ) {
        const XL6_IrDynamicMethodParameter* item =
            &module->dynamicMethodParameters[
                method->firstParameter + parameter];
        if ( item->matchAbi == XL6_IR_DYNAMIC_METHOD_MATCH_ANY_XVALUE )
            xl6OutputPrintf(out, "    bool has%lu = false;\n",
                (unsigned long)parameter);
        else
            xl6OutputPrintf(out,
                "    xvalue* bound%lu = NULL; bool has%lu = false;\n",
                (unsigned long)parameter, (unsigned long)parameter);
    }
    xl6OutputPuts(
        "    if ((positionalCount != 0 && positionalValues == NULL) || (keywordCount != 0 && (keywordNames == NULL || keywordValues == NULL)) || ",
        out);
    if ( method->variadicAbi == XL6_IR_CALL_VARIADIC_ARRAY )
        xl6OutputPuts("false", out);
    else
        xl6OutputPrintf(out, "positionalCount > %lu",
                        (unsigned long)method->fixedParameterCount);
    xl6OutputPuts(") return false;\n", out);
    for ( parameter = 0; parameter < method->fixedParameterCount; ++parameter ) {
        const XL6_IrDynamicMethodParameter* item =
            &module->dynamicMethodParameters[
                method->firstParameter + parameter];
        if ( item->matchAbi == XL6_IR_DYNAMIC_METHOD_MATCH_ANY_XVALUE )
            xl6OutputPrintf(out,
                "    if (positionalCount > %lu) { if (positionalValues[%lu] == NULL) return false; has%lu = true; }\n",
                (unsigned long)parameter, (unsigned long)parameter,
                (unsigned long)parameter);
        else
            xl6OutputPrintf(out,
                "    if (positionalCount > %lu) { if (positionalValues[%lu] == NULL) return false; bound%lu = positionalValues[%lu]; has%lu = true; }\n",
                (unsigned long)parameter, (unsigned long)parameter,
                (unsigned long)parameter, (unsigned long)parameter,
                (unsigned long)parameter);
    }
    xl6OutputPuts(
        "    for (keywordIndex = 0; keywordIndex < keywordCount; ++keywordIndex) {\n"
        "        bool matched = false;\n"
        "        if (keywordNames[keywordIndex].Data == NULL || keywordValues[keywordIndex] == NULL) return false;\n",
        out);
    for ( parameter = 0; parameter < method->fixedParameterCount; ++parameter ) {
        const XL6_IrDynamicMethodParameter* item =
            &module->dynamicMethodParameters[
                method->firstParameter + parameter];
        xl6OutputPuts(
            "        if (!matched && keywordNames[keywordIndex].Size == sizeof(", out);
        xl6IrCgenEmitCStringLiteral(out, item->name);
        xl6OutputPuts(") - 1 && memcmp(keywordNames[keywordIndex].Data, ", out);
        xl6IrCgenEmitCStringLiteral(out, item->name);
        xl6OutputPuts(", sizeof(", out);
        xl6IrCgenEmitCStringLiteral(out, item->name);
        if ( item->matchAbi == XL6_IR_DYNAMIC_METHOD_MATCH_ANY_XVALUE )
            xl6OutputPrintf(out,
                ") - 1) == 0) { if (has%lu) return false; has%lu = true; matched = true; }\n",
                (unsigned long)parameter, (unsigned long)parameter);
        else
            xl6OutputPrintf(out,
                ") - 1) == 0) { if (has%lu) return false; bound%lu = keywordValues[keywordIndex]; has%lu = true; matched = true; }\n",
                (unsigned long)parameter, (unsigned long)parameter,
                (unsigned long)parameter);
    }
    if ( method->variadicAbi != XL6_IR_CALL_VARIADIC_DICT )
        xl6OutputPuts("        if (!matched) return false;\n", out);
    xl6OutputPuts("    }\n", out);
    if ( method->variadicAbi == XL6_IR_CALL_VARIADIC_ARRAY )
        xl6OutputPrintf(out,
            "    for (size_t positionalIndex = %lu; positionalIndex < positionalCount; ++positionalIndex) if (positionalValues[positionalIndex] == NULL) return false;\n",
            (unsigned long)method->fixedParameterCount);
    for ( parameter = 0; parameter < method->fixedParameterCount; ++parameter ) {
        const XL6_IrDynamicMethodParameter* item =
            &module->dynamicMethodParameters[
                method->firstParameter + parameter];
        if ( item->defaultAbi == XL6_IR_DYNAMIC_METHOD_DEFAULT_NONE )
            xl6OutputPrintf(out,
                "    if (!has%lu) return false;\n",
                (unsigned long)parameter);
        if ( item->matchAbi != XL6_IR_DYNAMIC_METHOD_MATCH_ANY_XVALUE ) {
            xl6OutputPrintf(out, "    if (has%lu && !(", (unsigned long)parameter);
            if ( !xl6IrCgenEmitDynamicArgumentAdmission(out, module, item, parameter) )
                return xl6IrCgenDiag(diags, module, method->declaration,
                    "XLIR C emitter dynamic method match ABI is unresolved");
            xl6OutputPuts(")) return false;\n", out);
        }
    }
    xl6OutputPuts("    return true;\n}\n", out);
    return !xl6OutputHasFailed(out);
}

/* All operands are projected before native entry, in parameter order. A failed
 * projection never enters C and leaves the language owners for adapter rollback.
 * Physical field names and text representation come only from CallTarget. */
static int xl6IrCgenEmitDynamicNativeWitness(
    XL6_Output* out, const XL6_IrModule* module, const XL6_IrCallTarget* target,
    XL6_DiagList* diags)
{
    xl6OutputPuts("    { xl6_foreign_error_scope nativeScope = {0}; xl6_foreign_error_result nativeOutcome;\n", out);
    for ( size_t p = 0; p < target->parameterCount; ++p )
        if ( target->parameterPhysicalAbis[p] == XL6_IR_CALL_PARAMETER_PHYSICAL_NATIVE_CLASS_HANDLE_BORROW )
            xl6OutputPrintf(out, "      void* native%lu = NULL;\n", (unsigned long)p);
    xl6OutputPuts("      if (!xl6ForeignCallBegin(&nativeScope)) goto fail;\n", out);
    for ( size_t p = 0; p < target->parameterCount; ++p ) {
        const XL6_IrClassField* field;
        if ( target->parameterPhysicalAbis[p] != XL6_IR_CALL_PARAMETER_PHYSICAL_NATIVE_CLASS_HANDLE_BORROW ) continue;
        field = xl6IrCgenNativeClassPhysicalField(module, target, p);
        if ( field == NULL ) return xl6IrCgenDiag(diags, module, target->declaration,
            "XLIR dynamic native operand has no exact physical field");
        xl6OutputPuts("      { xvalue* field = xrtValueObjectGet(", out);
        if ( p == 0 ) xl6OutputPuts("receiver", out);
        else xl6OutputPrintf(out, "a%lu", (unsigned long)(p - 1));
        xl6OutputPuts(", xlir_literal_view(", out);
        xl6IrCgenEmitCStringLiteral(out, field->name);
        xl6OutputPrintf(out,
            ")); if (field == NULL || !%s(field, &native%lu%s)) { if (!nativeScope.failed && xrtGetError() == NULL) xrtSetErrorTake(xlir_error_create(\"TypeError\", \"dynamic native argument has no valid declared field\")); xl6ErrorScopeCaptureFailure(&nativeScope); goto native_done; } }\n",
            field->nativeDestroySymbol != 0 ? "xrtValueGetHandle" : "xrtValueGetPointer",
            (unsigned long)p, field->nativeDestroySymbol != 0 ? ", NULL, NULL" : "");
    }
    xl6OutputPuts("      ", out);
    if ( target->errorPhysicalAbi == XL6_IR_CALL_ERROR_PHYSICAL_RESULT_CARRIER )
        xl6OutputPuts("callResult = ", out);
    else if ( target->returnType != XL6_IR_TYPE_INVALID ) xl6OutputPuts("callResult.value = ", out);
    if ( target->resultConversionAbi == XL6_IR_CALL_RESULT_CONVERSION_BORROWED_STRING_COPY )
        xl6OutputPuts("xlir_string_from_cstr(", out);
    xl6OutputPrintf(out, "%s(", target->cName);
    for ( size_t p = 0; p < target->parameterCount; ++p ) {
        if ( p ) xl6OutputPuts(", ", out);
        if ( target->parameterPhysicalAbis[p] == XL6_IR_CALL_PARAMETER_PHYSICAL_NATIVE_CLASS_HANDLE_BORROW )
            xl6OutputPrintf(out, "native%lu", (unsigned long)p);
        else if ( p == 0 ) xl6OutputPuts("receiver", out);
        else xl6OutputPrintf(out, "a%lu%s", (unsigned long)(p - 1), xl6IrCgenDirectPlaceSuffix(module, target, p));
    }
    xl6OutputPuts(")", out);
    if ( target->resultConversionAbi == XL6_IR_CALL_RESULT_CONVERSION_BORROWED_STRING_COPY ) xl6OutputPuts(")", out);
    xl6OutputPuts(";\n", out);
    /* OWNED inputs are consumed only by actual native entry. Nulling the local
     * witnesses prevents the later failure path from releasing them again. */
    for ( size_t p = 1; p < target->parameterCount; ++p )
        if ( target->parameterAbis[p] == XL6_IR_PARAMETER_ABI_OWNED )
            xl6OutputPrintf(out, "      a%lu = (%s){0};\n", (unsigned long)(p - 1), xl6IrCgenType(module, target->parameterTypes[p]));
    if ( target->errorPhysicalAbi == XL6_IR_CALL_ERROR_PHYSICAL_RESULT_CARRIER )
        xl6OutputPuts("      if (!callResult.ok) { xl6_error_result failure = {false, xrtTakeError()}; xl6ErrorScopePropagateFailure(failure); }\n", out);
    xl6OutputPuts("native_done:\n      if (nativeScope.failed || xrtGetError() != NULL) {\n"
        "        xl6ErrorScopeCaptureFailure(&nativeScope);\n", out);
    if ( target->returnType != XL6_IR_TYPE_INVALID &&
         !xl6IrCgenEmitOwnedPlaceRelease(out, module, target->returnType, "callResult.value", diags) ) return 0;
    xl6OutputPuts("      }\n      nativeOutcome = xl6ForeignCallEnd(&nativeScope); callResult.ok = nativeOutcome.ok;\n"
        "      if (!nativeOutcome.ok) { xl6ErrorScopePropagateFailure(nativeOutcome); goto fail; }\n    }\n", out);
    return !xl6OutputHasFailed(out);
}

/* Emit one source-free adapter for a VERIFIED dynamic method row.  Binding,
 * defaults, unboxing, ownership transfer and result boxing are derived only
 * from Lowered XLIR metadata; this function never consults AST nodes. */
static int xl6IrCgenEmitDynamicMethodAdapter(
    XL6_Output* out, const XL6_IrModule* module, size_t methodIndex,
    XL6_DiagList* diags)
{
    const XL6_IrDynamicMethod* method =
        methodIndex < module->dynamicMethodCount
            ? &module->dynamicMethods[methodIndex] : NULL;
    const XL6_IrCallTarget* target = method != NULL
        ? xl6IrCallTargetGet(module, method->targetSymbol) : NULL;
    const XL6_IrType* resultType = method != NULL &&
            method->returnType != XL6_IR_TYPE_INVALID
        ? xl6IrTypeGet(&module->types, method->returnType) : NULL;
    const char* resultCType = method != NULL &&
            method->returnType != XL6_IR_TYPE_INVALID
        ? xl6IrCgenType(module, method->returnType) : NULL;
    const char* carrierCType = target != NULL &&
            (target->kind == XL6_IR_CALL_TARGET_EXTERN_C || target->errorAbi == XL6_IR_ERROR_ABI_MAY_RAISE)
        ? xl6IrCgenErrorResultType(module, method->returnType) : NULL;
    size_t parameter;
    if ( method == NULL || target == NULL ||
         method->adapterAbi != XL6_IR_DYNAMIC_METHOD_ADAPTER_VERIFIED ||
         !xl6IrDynamicMethodCallTargetSupported(module, target) ||
         (target->errorAbi != XL6_IR_ERROR_ABI_NO_RAISE &&
          (target->errorAbi != XL6_IR_ERROR_ABI_MAY_RAISE ||
           carrierCType == NULL)) ||
         (method->returnType != XL6_IR_TYPE_INVALID &&
          (resultType == NULL || resultCType == NULL)) )
        return xl6IrCgenDiag(diags, module,
            method != NULL ? method->declaration : XL6_IR_NODE_INVALID,
            "XLIR C emitter dynamic method adapter is unresolved");

    xl6OutputPrintf(out,
        "static xvalue* xlir_dynamic_method_dm%lu(xvalue* receiver, size_t positionalCount, xvalue** positionalValues, size_t keywordCount, const xlir_string_view* keywordNames, xvalue** keywordValues)\n"
        "{\n"
        "    xvalue* boxedResult = NULL; xvalue* variadic = NULL; size_t keywordIndex;\n",
        (unsigned long)methodIndex);
    if ( resultType != NULL )
        xl6OutputPrintf(out, "    %s returned = (%s){0};\n",
                        resultCType, resultCType);
    if ( carrierCType != NULL )
        xl6OutputPrintf(out, "    %s callResult = {0};\n", carrierCType);
    for ( parameter = 0; parameter < method->parameterCount; ++parameter ) {
        const XL6_IrDynamicMethodParameter* item =
            &module->dynamicMethodParameters[
                method->firstParameter + parameter];
        const XL6_IrType* type = xl6IrTypeGet(&module->types, item->type);
        const char* cType = xl6IrCgenType(module, item->type);
        if ( type == NULL || cType == NULL )
            return xl6IrCgenDiag(diags, module, method->declaration,
                "XLIR C emitter dynamic method parameter type is unresolved");
        if ( parameter < method->fixedParameterCount )
            xl6OutputPrintf(out,
                "    xvalue* bound%lu = NULL; bool has%lu = false;\n",
                (unsigned long)parameter, (unsigned long)parameter);
        xl6OutputPrintf(out, "    %s a%lu = (%s){0};\n",
                        cType, (unsigned long)parameter, cType);
        if ( type->cAbi == XL6_IR_C_ABI_MANAGED_STRING )
            xl6OutputPrintf(out,
                "    xlir_string_view text%lu = {0};\n",
                (unsigned long)parameter);
    }
    xl6OutputPrintf(out,
        "    if (receiver == NULL || (positionalCount != 0 && positionalValues == NULL) || "
        "(keywordCount != 0 && (keywordNames == NULL || keywordValues == NULL)) || ");
    if ( method->variadicAbi == XL6_IR_CALL_VARIADIC_ARRAY )
        xl6OutputPuts("false", out);
    else
        xl6OutputPrintf(out, "positionalCount > %lu",
                        (unsigned long)method->fixedParameterCount);
    xl6OutputPuts(") { ", out);
    xl6IrCgenEmitDynamicMethodError(
        out, "dynamic method frame does not match its verified signature");
    xl6OutputPuts(" }\n", out);
    for ( parameter = 0; parameter < method->fixedParameterCount; ++parameter ) {
        xl6OutputPrintf(out,
            "    if (positionalCount > %lu) { if (positionalValues[%lu] == NULL) { ",
            (unsigned long)parameter, (unsigned long)parameter);
        xl6IrCgenEmitDynamicMethodError(
            out, "dynamic method positional argument is null");
        xl6OutputPrintf(out,
            " } bound%lu = positionalValues[%lu]; has%lu = true; }\n",
            (unsigned long)parameter, (unsigned long)parameter,
            (unsigned long)parameter);
    }
    if ( method->variadicAbi == XL6_IR_CALL_VARIADIC_DICT )
        xl6OutputPuts(
            "    variadic = xrtValueObject(); if (variadic == NULL) goto fail;\n",
            out);
    xl6OutputPuts(
        "    for (keywordIndex = 0; keywordIndex < keywordCount; ++keywordIndex) {\n"
        "        bool matched = false;\n"
        "        if (keywordNames[keywordIndex].Data == NULL || keywordValues[keywordIndex] == NULL) { ",
        out);
    xl6IrCgenEmitDynamicMethodError(
        out, "dynamic method keyword argument is null");
    xl6OutputPuts(" }\n", out);
    for ( parameter = 0; parameter < method->fixedParameterCount; ++parameter ) {
        const XL6_IrDynamicMethodParameter* item =
            &module->dynamicMethodParameters[
                method->firstParameter + parameter];
        xl6OutputPuts(
            "        if (!matched && keywordNames[keywordIndex].Size == sizeof(", out);
        xl6IrCgenEmitCStringLiteral(out, item->name);
        xl6OutputPuts(") - 1 && memcmp(keywordNames[keywordIndex].Data, ", out);
        xl6IrCgenEmitCStringLiteral(out, item->name);
        xl6OutputPuts(", sizeof(", out);
        xl6IrCgenEmitCStringLiteral(out, item->name);
        xl6OutputPrintf(out,
            ") - 1) == 0) { if (has%lu) { ",
            (unsigned long)parameter);
        xl6IrCgenEmitDynamicMethodError(
            out, "dynamic method parameter is bound more than once");
        xl6OutputPrintf(out,
            " } bound%lu = keywordValues[keywordIndex]; has%lu = true; matched = true; }\n",
            (unsigned long)parameter, (unsigned long)parameter);
    }
    if ( method->variadicAbi == XL6_IR_CALL_VARIADIC_DICT ) {
        xl6OutputPuts(
            "        if (!matched) { xvalue* captured = xrtValueRetain(keywordValues[keywordIndex]);\n"
            "            if (captured == NULL || !xrtValueObjectSetNew(variadic, keywordNames[keywordIndex], captured)) { ",
            out);
        xl6IrCgenEmitDynamicMethodError(
            out, "dynamic method keyword capture failed");
        xl6OutputPuts(" } }\n", out);
    } else {
        xl6OutputPuts("        if (!matched) { ", out);
        xl6IrCgenEmitDynamicMethodError(
            out, "dynamic method keyword does not name an unbound parameter");
        xl6OutputPuts(" }\n", out);
    }
    xl6OutputPuts("    }\n", out);
    if ( method->variadicAbi == XL6_IR_CALL_VARIADIC_ARRAY ) {
        const XL6_IrType* array=xl6IrTypeGet(&module->types,
            module->dynamicMethodParameters[method->firstParameter+method->fixedParameterCount].type);
        if (!array || method->variadicCarrierAbi!=XL6_IR_COLLECTION_CARRIER_SEQUENCE_V1 ||
            !method->variadicElementIdentity)
            return xl6IrCgenDiag(diags,module,method->declaration,"Dynamic variadic method has no native Array producer contract");
        xl6OutputPrintf(out,
            "    { xrtcallframe frame = {0}; frame.Arguments = positionalValues; frame.ArgumentCount = positionalCount;\n"
            "      variadic = xl6ArrayCallFramePackV1(0x%016llXULL, 0x%016llXULL, &frame, %lu); }\n"
            "    if (variadic == NULL) goto fail;\n",
            (unsigned long long)array->typeValueId,(unsigned long long)method->variadicElementIdentity,
            (unsigned long)method->fixedParameterCount);
    }

    for ( parameter = 0; parameter < method->fixedParameterCount; ++parameter ) {
        const XL6_IrDynamicMethodParameter* item =
            &module->dynamicMethodParameters[
                method->firstParameter + parameter];
        const XL6_IrType* type = xl6IrTypeGet(&module->types, item->type);
        const char* getter = type->cAbi == XL6_IR_C_ABI_BOOL
            ? "xrtValueGetBool"
            : type->cAbi == XL6_IR_C_ABI_CHAR_U32
                ? "xrtValueGetChar"
            : type->cAbi == XL6_IR_C_ABI_I64
                ? "xrtValueGetInt"
            : type->cAbi == XL6_IR_C_ABI_U64
                ? "xrtValueGetUInt"
            : type->cAbi == XL6_IR_C_ABI_F64
                ? "xrtValueGetFloat"
            : type->cAbi == XL6_IR_C_ABI_TIME_I64
                ? "xrtValueGetTime" : NULL;
        if ( item->defaultAbi == XL6_IR_DYNAMIC_METHOD_DEFAULT_NONE ) {
            xl6OutputPrintf(out, "    if (!has%lu) { ",
                            (unsigned long)parameter);
            xl6IrCgenEmitDynamicMethodError(
                out, "dynamic method required parameter is missing");
            xl6OutputPuts(" }\n", out);
        }
        if ( item->matchAbi != XL6_IR_DYNAMIC_METHOD_MATCH_ANY_XVALUE ) {
            xl6OutputPrintf(out, "    if (has%lu && !(", (unsigned long)parameter);
            if ( !xl6IrCgenEmitDynamicArgumentAdmission(out, module, item, parameter) )
                return xl6IrCgenDiag(diags, module, method->declaration,
                    "XLIR dynamic argument admission is unresolved");
            xl6OutputPuts(")) { ", out);
            xl6IrCgenEmitDynamicMethodError(out,
                "dynamic method argument is outside its declared value domain");
            xl6OutputPuts(" }\n", out);
        }
        if ( getter != NULL ) {
            xl6OutputPrintf(out,
                "    if (has%lu) { if (!%s(bound%lu, &a%lu)) { ",
                (unsigned long)parameter, getter, (unsigned long)parameter,
                (unsigned long)parameter);
            xl6IrCgenEmitDynamicMethodError(
                out, "dynamic method argument has the wrong scalar type");
            xl6OutputPuts(" } }", out);
            if ( item->defaultAbi != XL6_IR_DYNAMIC_METHOD_DEFAULT_NONE )
                xl6OutputPrintf(out, " else a%lu = %s;",
                    (unsigned long)parameter, item->defaultLiteral);
            xl6OutputPuts("\n", out);
        } else if ( type->cAbi == XL6_IR_C_ABI_MANAGED_STRING ) {
            xl6OutputPrintf(out,
                "    if (has%lu) { if (!xrtValueGetString(bound%lu, &text%lu) || (a%lu = xlir_string_from_utf8(text%lu.Data, text%lu.Size)).Data == NULL) { ",
                (unsigned long)parameter, (unsigned long)parameter,
                (unsigned long)parameter, (unsigned long)parameter,
                (unsigned long)parameter, (unsigned long)parameter);
            xl6IrCgenEmitDynamicMethodError(
                out, "dynamic method argument has the wrong string type");
            xl6OutputPuts(" } }", out);
            if ( item->defaultAbi != XL6_IR_DYNAMIC_METHOD_DEFAULT_NONE )
                xl6OutputPrintf(out,
                    " else if ((a%lu = xlir_string_from_utf8(%s, sizeof(%s) - 1u)).Data == NULL) goto fail;",
                    (unsigned long)parameter, item->defaultLiteral, item->defaultLiteral);
            xl6OutputPuts("\n", out);
        } else if ( type->cAbi == XL6_IR_C_ABI_MANAGED_BYTES ) {
            xl6OutputPrintf(out,
                "    if (has%lu && xrtValueType(bound%lu) != XVALUE_NULL) { a%lu = xrtValueRetain(bound%lu); if (a%lu == NULL) goto fail; }\n",
                (unsigned long)parameter, (unsigned long)parameter,
                (unsigned long)parameter, (unsigned long)parameter,
                (unsigned long)parameter);
        } else if ( type->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR ) {
            xl6OutputPrintf(out,
                "    a%lu = xrtValueRetain(has%lu ? bound%lu : xrtValueNull()); if (a%lu == NULL) goto fail;\n",
                (unsigned long)parameter, (unsigned long)parameter,
                (unsigned long)parameter, (unsigned long)parameter);
        } else {
            return xl6IrCgenDiag(diags, module, method->declaration,
                "XLIR C emitter dynamic method projection ABI is unresolved");
        }
    }
    if ( method->variadicAbi != XL6_IR_CALL_VARIADIC_NONE ) {
        parameter = method->fixedParameterCount;
        xl6OutputPrintf(out,
            "    a%lu = variadic; variadic = NULL;\n",
            (unsigned long)parameter);
    }
    if ( target->kind == XL6_IR_CALL_TARGET_EXTERN_C ) {
        if ( carrierCType == NULL || !xl6IrCgenEmitDynamicNativeWitness(out, module, target, diags) ) return 0;
    } else {
        xl6OutputPuts("    ", out);
        if ( carrierCType != NULL ) xl6OutputPuts("callResult = ", out);
        else if ( resultType != NULL ) xl6OutputPuts("returned = ", out);
        xl6OutputPrintf(out, "xlir_fn_%u(receiver", (unsigned int)method->targetSymbol);
        for ( parameter = 0; parameter < method->parameterCount; ++parameter )
            xl6OutputPrintf(out, ", a%lu", (unsigned long)parameter);
        xl6OutputPuts(");\n", out);
    }
    if (carrierCType != NULL)
        xl6OutputPuts("    if (!callResult.ok) { xl6_error_result failure = {false, xrtTakeError()}; xl6ErrorScopePropagateFailure(failure); }\n", out);
    for ( parameter = 0; parameter < method->parameterCount; ++parameter ) {
        const XL6_IrDynamicMethodParameter* item =
            &module->dynamicMethodParameters[
                method->firstParameter + parameter];
        const XL6_IrType* type = xl6IrTypeGet(&module->types, item->type);
        if ( item->abi != XL6_IR_PARAMETER_ABI_BORROW ) continue;
        if ( type->cAbi == XL6_IR_C_ABI_MANAGED_STRING )
            xl6OutputPrintf(out, "    xlir_string_drop(a%lu); a%lu = (xlir_string){0};\n",
                (unsigned long)parameter, (unsigned long)parameter);
        else if ( type->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR ||
                  type->cAbi == XL6_IR_C_ABI_MANAGED_BYTES )
            xl6OutputPrintf(out,
                "    xrtValueRelease(a%lu); a%lu = NULL;\n",
                (unsigned long)parameter, (unsigned long)parameter);
    }
    if ( carrierCType != NULL ) {
        xl6OutputPuts(
            "    if (!callResult.ok) { if (xrtGetError() == NULL) xrtSetErrorTake(xlir_error_create(\"RuntimeError\", \"dynamic method target returned an error without runtime state\")); return NULL; }\n",
            out);
        if ( resultType != NULL )
            xl6OutputPuts("    returned = callResult.value;\n", out);
    }
    if ( resultType == NULL )
        xl6OutputPuts(
            "    boxedResult = xrtValueRetain(xrtValueNull());\n", out);
    else if ( resultType->cAbi == XL6_IR_C_ABI_BOOL )
        xl6OutputPuts(
            "    boxedResult = xrtValueRetain(xrtValueBool(returned));\n", out);
    else if ( resultType->cAbi == XL6_IR_C_ABI_CHAR_U32 )
        xl6OutputPuts("    boxedResult = xrtValueChar(returned);\n", out);
    else if ( resultType->cAbi == XL6_IR_C_ABI_I64 )
        xl6OutputPuts("    boxedResult = xrtValueInt(returned);\n", out);
    else if ( resultType->cAbi == XL6_IR_C_ABI_U64 )
        xl6OutputPuts("    boxedResult = xrtValueUInt(returned);\n", out);
    else if ( resultType->cAbi == XL6_IR_C_ABI_F64 )
        xl6OutputPuts("    boxedResult = xrtValueFloat(returned);\n", out);
    else if ( resultType->cAbi == XL6_IR_C_ABI_TIME_I64 )
        xl6OutputPuts("    boxedResult = xrtValueTime(returned);\n", out);
    else if ( resultType->cAbi == XL6_IR_C_ABI_MANAGED_STRING ) {
        xl6OutputPuts(
            "    if (returned.Data != NULL) boxedResult = xrtValueString(returned);\n",
            out);
        if (resultType->kind == XL6_IR_TYPE_NULLABLE)
            xl6OutputPuts("    else boxedResult = xrtValueRetain(xrtValueNull());\n", out);
        xl6OutputPuts("    xlir_string_drop(returned);\n", out);
    }
    else if ( resultType->cAbi == XL6_IR_C_ABI_MANAGED_BYTES ) {
        xl6OutputPuts("    boxedResult = returned;\n", out);
        if ( resultType->kind == XL6_IR_TYPE_NULLABLE )
            xl6OutputPuts("    if (returned == NULL) boxedResult = xrtValueRetain(xrtValueNull());\n", out);
    }
    else if ( resultType->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR )
        xl6OutputPuts("    boxedResult = returned;\n", out);
    else
        return xl6IrCgenDiag(diags, module, method->declaration,
            "XLIR C emitter dynamic method result boxing ABI is unresolved");
    xl6OutputPuts(
        "    if (boxedResult == NULL && xrtGetError() == NULL) xrtSetErrorTake(xlir_error_create(\"RuntimeError\", \"dynamic method result boxing failed\"));\n"
        "    return boxedResult;\n"
        "fail:\n"
        "    { xl6_error_result failure = {false, xrtTakeError()}; xl6ErrorScopePropagateFailure(failure); }\n",
        out);
    for ( parameter = 0; parameter < method->parameterCount; ++parameter ) {
        const XL6_IrDynamicMethodParameter* item =
            &module->dynamicMethodParameters[
                method->firstParameter + parameter];
        const XL6_IrType* type = xl6IrTypeGet(&module->types, item->type);
        if ( type->cAbi == XL6_IR_C_ABI_MANAGED_STRING )
            xl6OutputPrintf(out, "    xlir_string_drop(a%lu);\n",
                            (unsigned long)parameter);
        else if ( type->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR ||
                  type->cAbi == XL6_IR_C_ABI_MANAGED_BYTES )
            xl6OutputPrintf(out, "    xrtValueRelease(a%lu);\n",
                            (unsigned long)parameter);
    }
    xl6OutputPuts(
        "    xrtValueRelease(variadic); return NULL;\n"
        "}\n",
        out);
    return !xl6OutputHasFailed(out);
}

static int xl6IrCgenEmitBoundMethodRuntime(XL6_Output* out, const XL6_IrModule* module, XL6_DiagList* diags)
{
    if (module->abi.boundMethodStorageAbi != XL6_IR_BOUND_METHOD_STORAGE_RESIDENT_CLOSURE_LAYOUT_V1)
        return xl6IrCgenDiag(diags, module, XL6_IR_NODE_INVALID, "XLIR bound method storage ABI is unresolved");
    xl6OutputPuts(
        "typedef xvalue* (*xlir_dynamic_method_dispatch)(xvalue*, size_t, xvalue**, size_t, const xlir_string_view*, xvalue**);\n"
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
        "    xvalue** owned = NULL; xvalue* result = NULL; size_t total = 0, retained = 0, i;\n"
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
        "        frame->KeywordNames, owned != NULL ? owned + frame->ArgumentCount : NULL);\n"
        "    retained = 0; /* The verified dispatcher consumes every argument on both outcomes. */\n"
        "    xrtFree(owned); owned = NULL;\n"
        "    if (result == NULL || !xrtCallResultSetTake(callResult, 0, &result)) goto fail;\n"
        "    return true;\n"
        "fail: {\n"
        "    xerror* pendingError;\n"
        "    xl6ForeignCallbackFail(); pendingError = xrtTakeError();\n"
        "    for (i = 0; i < retained; ++i) xrtValueRelease(owned[i]);\n"
        "    xrtFree(owned); xrtValueRelease(result);\n"
        "    if (callResult != NULL) xrtCallResultUnit(callResult);\n"
        "    xrtSetErrorTake(pendingError); return false;\n"
        "}\n"
        "}\n"
        "static xvalue* xlir_bound_method_create(xvalue* receiver, xlir_dynamic_method_dispatch invoke)\n"
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
        "    xlir_closure_release(closure);\n"
        "    return result;\n"
        "fail: {\n"
        "    xerror* pendingError = xrtTakeError();\n"
        "    if (pendingError == NULL) pendingError = xlir_error_create(\"MemoryError\", \"bound method allocation failed\");\n"
        "    xlir_closure_release(closure);\n"
        "    if (bound != NULL) xlir_closure_env_drop(&xlir_bound_method_layout, bound);\n"
        "    xrtSetErrorTake(pendingError); return NULL;\n"
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
    const char* fallbackOperation = dispatchKind == site->kind
        ? xl6IrCgenDynamicCollectionOpToken(site->fallbackCollectionOp) : NULL;
    size_t methodIndex;
    xl6OutputPrintf(out,
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
    else if ( fallbackOperation != NULL )
        xl6OutputPrintf(out,
            "    if (xrtValueType(receiver) != 12) {\n"
            "        if (keywordCount || (positionalCount && !positionalValues) || !xl6DynamicCollectionArityV1(%s, positionalCount)) { xrtSetErrorTake(xlir_error_create(\"ArgumentError\", \"invalid dynamic collection arguments\")); goto cleanup; }\n"
            "        result = xl6DynamicCollectionCallTakeV1(receiver, %s, positionalCount > 0 ? positionalValues[0] : NULL, positionalCount > 1 ? positionalValues[1] : NULL);\n"
            "        for (argumentIndex = 0; argumentIndex < positionalCount; ++argumentIndex) positionalValues[argumentIndex] = NULL;\n"
            "        if (result == NULL && xrtGetError() == NULL) xrtSetErrorTake(xlir_error_create(\"RuntimeError\", \"dynamic collection fallback failed\"));\n"
            "        goto cleanup;\n"
            "    }\n"
            "    typeValue = xrtValueObjectGet(receiver, xrtStrView(\"$xlir.class.runtime-type\"));\n"
            "    if (typeValue == NULL || !xrtValueGetInt(typeValue, &runtimeType)) { if (xrtGetError() == NULL) xrtSetErrorTake(xlir_error_create(\"TypeError\", \"dynamic method receiver has no runtime class tag\")); goto cleanup; }\n",
            fallbackOperation,fallbackOperation);
    else
        xl6OutputPuts(
            "    typeValue = xrtValueObjectGet(receiver, xrtStrView(\"$xlir.class.runtime-type\"));\n"
            "    if (typeValue == NULL || !xrtValueGetInt(typeValue, &runtimeType)) { if (xrtGetError() == NULL) xrtSetErrorTake(xlir_error_create(\"TypeError\", \"dynamic method receiver has no runtime class tag\")); goto cleanup; }\n",
            out);
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
        xl6OutputPrintf(out,
            "    case %lu: result = xlir_dynamic_method_dm%lu(receiver, positionalCount, positionalValues, keywordCount, keywordNames, keywordValues); break;\n",
            (unsigned long)methodIndex, (unsigned long)methodIndex);
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
                xl6OutputPrintf(out, "        case %u: result = xlir_bound_method_create(receiver, xlir_dynamic_method_bound_d%lu); break;\n",
                    (unsigned int)method->runtimeTypeTag, (unsigned long)siteIndex);
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
            if ( abi == XL6_IR_DYNAMIC_METHOD_MATCH_EXACT_IDENTITY ) needs |= 2u;
            else if ( abi != XL6_IR_DYNAMIC_METHOD_MATCH_ANY_XVALUE &&
                      abi != XL6_IR_DYNAMIC_METHOD_MATCH_NOMINAL ) needs |= 1u;
        }
    }
    return needs;
}

static int xl6IrCgenEmitDynamicMethodRuntime(
    XL6_Output* out, const XL6_IrModule* module, XL6_DiagList* diags)
{
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
        if ( target->kind == XL6_IR_CALL_TARGET_EXTERN_C ) {
            if ( !xl6IrCgenEmitExternPrototype(out, module, target, diags) ) return 0;
            continue;
        }
        function = xl6IrCgenFindFunction(
            module, module->dynamicMethods[methodIndex].targetSymbol);
        if ( function == NULL ||
             !xl6IrCgenEmitPrototype(out, module, function, diags) ) return 0;
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
