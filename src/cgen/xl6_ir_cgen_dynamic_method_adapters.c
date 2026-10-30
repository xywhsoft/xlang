static int xl6IrCgenEmitExternPrototype(XL6_Output*, const XL6_IrModule*, const XL6_IrCallTarget*, XL6_DiagList*);
static int xl6IrCgenEmitLauncherPrototype(XL6_Output*,const XL6_IrModule*,const XL6_IrCallTarget*,XL6_DiagList*);
static void xl6IrCgenEmitDispatchTargetCall(XL6_Output*,const XL6_IrCallTarget*,const XL6_IrClassField*,size_t);

static int xl6IrCgenDynamicMethodNeeded(
    const XL6_IrModule* module, size_t methodIndex)
{
    const XL6_IrDynamicMethod* method;
    const XL6_IrCallTarget* target;
    size_t siteIndex;
    size_t instructionIndex;
    if ( module == NULL || methodIndex >= module->dynamicMethodCount ) return 0;
    method = &module->dynamicMethods[methodIndex];
    if ( method->adapterAbi != XL6_IR_DYNAMIC_METHOD_ADAPTER_VERIFIED ) return 0;
    /* Most declared methods have no erased call site. Consult that canonical
     * IR set before repeatedly scanning the instruction arena or validating a
     * native target. This is only a necessary condition; an actual reachable
     * matching instruction and the complete target contract remain required. */
    for ( siteIndex = 0; siteIndex < module->dynamicCallSiteCount; ++siteIndex )
        if ( xl6IrDynamicMethodMatchesCallSite(
                 &module->dynamicCallSites[siteIndex], method) ) break;
    if ( siteIndex == module->dynamicCallSiteCount ) return 0;
    target = xl6IrCallTargetGet(module, method->targetSymbol);
    if ( target == NULL || target->reachability == XL6_IR_REACHABILITY_UNREACHABLE ||
         !xl6IrDynamicMethodCallTargetSupported(module, target) ) return 0;
    for ( instructionIndex = 0;
          instructionIndex < module->instructionCount; ++instructionIndex ) {
        const XL6_IrInstruction* instruction =
            &module->instructions[instructionIndex];
        const XL6_IrDynamicCallSite* site;
        if ( instruction->kind != XL6_IR_INSTR_RUNTIME_CALL ||
             instruction->runtimeOp !=
                 XL6_IR_RUNTIME_XVALUE_DYNAMIC_METHOD_CALL ||
             instruction->dynamicCallSite >= module->dynamicCallSiteCount )
            continue;
        site = &module->dynamicCallSites[instruction->dynamicCallSite];
        if ( xl6IrDynamicMethodMatchesCallSite(site, method) &&
             xl6IrCgenInstructionIsReachable(module, instructionIndex) ) return 1;
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
    if (item->matchAbi == XL6_IR_DYNAMIC_METHOD_MATCH_ZERO_PAYLOAD) {
        xl6OutputPrintf(out,
            "(xrtValueType(bound%lu) == XVALUE_NULL && (!xrtValueTypeId(bound%lu) || xrtValueTypeId(bound%lu) == 0x%016llXULL))",
            (unsigned long)parameter, (unsigned long)parameter, (unsigned long)parameter,
            (unsigned long long)type->typeValueId);
    } else if (item->matchAbi==XL6_IR_DYNAMIC_METHOD_MATCH_SCALAR_CODEC) {
        const XL6_IrCallableBridgeResult* codec=xl6IrDynamicMethodScalarCodecGet(module,item->scalarCodec);
        const XL6_IrType* value=codec ? xl6IrTypeGet(&module->types,codec->valueType) : NULL;
        if(!codec || !value || codec->type!=item->type)return 0;
        bool nullable=codec->presenceAbi!=XL6_IR_CALLABLE_PRESENCE_NONE;
        if(codec->decodeAbi==XL6_IR_CALLABLE_BRIDGE_NATIVE_SCALAR)
            xl6OutputPrintf(out,"xl6ValueAdmitsNativeScalarV1(bound%lu, %u, 0x%016llXULL, %s)",
                (unsigned long)parameter,(unsigned)codec->nativeScalarProfile,
                (unsigned long long)value->typeValueId,nullable ? "true":"false");
        else {
            if(nullable)xl6OutputPrintf(out,"(xrtValueType(bound%lu)==XVALUE_NULL || ",(unsigned long)parameter);
            if(codec->decodeAbi==XL6_IR_CALLABLE_BRIDGE_POINTER)
                xl6OutputPrintf(out,"(xrtValueType(bound%lu)==XVALUE_POINTER && (!xrtValueTypeId(bound%lu) || xrtValueTypeId(bound%lu)==0x%016llXULL))",
                    (unsigned long)parameter,(unsigned long)parameter,(unsigned long)parameter,(unsigned long long)value->typeValueId);
            else {
                unsigned rule=codec->integerDomain.rule;
                if(!rule) switch(codec->decodeAbi) {
                case XL6_IR_CALLABLE_BRIDGE_BOOL: rule=XL6_VALUE_ADMIT_BOOL;break;
                case XL6_IR_CALLABLE_BRIDGE_CHAR: rule=XL6_VALUE_ADMIT_CHAR;break;
                case XL6_IR_CALLABLE_BRIDGE_F32: rule=XL6_VALUE_ADMIT_F32;break;
                case XL6_IR_CALLABLE_BRIDGE_F64: rule=XL6_VALUE_ADMIT_F64;break;
                case XL6_IR_CALLABLE_BRIDGE_TIME: rule=XL6_VALUE_ADMIT_TIME;break;
                default:return 0;
                }
                if(codec->integerDomain.nominal)xl6OutputPrintf(out,"(xrtValueTypeId(bound%lu)==0x%016llXULL && ",
                    (unsigned long)parameter,(unsigned long long)codec->integerDomain.identity);
                xl6OutputPrintf(out,"xl6ValueAdmitsV1(bound%lu, %u, false, 0x%016llXULL)",
                    (unsigned long)parameter,rule,(unsigned long long)value->typeValueId);
                if(codec->integerDomain.nominal)xl6OutputPuts(")",out);
            }
            if(nullable)xl6OutputPuts(")",out);
        }
    } else if ( item->matchAbi == XL6_IR_DYNAMIC_METHOD_MATCH_CALLABLE ) {
        XL6_IrTypeId signature=xl6IrRuntimeCallableFieldSourceSignature(module,item->type);
        const XL6_IrType* base=xl6IrTypeGet(&module->types,signature);
        if (!base || !base->typeValueId) return 0;
        if (type->kind==XL6_IR_TYPE_NULLABLE) xl6OutputPrintf(out,"(xrtValueType(bound%lu) == XVALUE_NULL || ",(unsigned long)parameter);
        xl6OutputPrintf(out,
            "(xrtValueIsCallable(bound%lu) && xrtValueTypeId(bound%lu) == 0x%016llXULL)",
            (unsigned long)parameter, (unsigned long)parameter,
            (unsigned long long)base->typeValueId);
        if (type->kind==XL6_IR_TYPE_NULLABLE) xl6OutputPuts(")",out);
    } else if ( item->matchAbi == XL6_IR_DYNAMIC_METHOD_MATCH_FUTURE ) {
        const XL6_IrType* base=type->kind==XL6_IR_TYPE_NULLABLE ? xl6IrTypeGet(&module->types,type->params[0]) : type;
        if (!base || !base->typeValueId) return 0;
        if (type->kind==XL6_IR_TYPE_NULLABLE) xl6OutputPrintf(out,"(xrtValueType(bound%lu) == XVALUE_NULL || ",(unsigned long)parameter);
        xl6OutputPrintf(out,"(xrtValueIsFuture(bound%lu) && xrtValueTypeId(bound%lu) == 0x%016llXULL)",
            (unsigned long)parameter,(unsigned long)parameter,(unsigned long long)base->typeValueId);
        if (type->kind==XL6_IR_TYPE_NULLABLE) xl6OutputPuts(")",out);
    } else if ( item->matchAbi == XL6_IR_DYNAMIC_METHOD_MATCH_NOMINAL ) {
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
    XL6_IrNativeCallValuePlan valuePlan={0};
    if (!xl6IrNativeCallValuePlanBuild(module,target,&valuePlan)) return 0;
    bool encoded=xl6IrCgenNativeCallValueEncoded(&valuePlan);
    char input[40];
    if (valuePlan.inputParameter>1)snprintf(input,sizeof(input),"a%u",valuePlan.inputParameter-2);
    else snprintf(input,sizeof(input),"receiver");
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
    if (!xl6IrCgenNativeCallValueBegin(out,module,&valuePlan,input,"nativeScope")) return 0;
    xl6OutputPuts("      ", out);
    if (target->resultConversionAbi==XL6_IR_CALL_RESULT_CONVERSION_BORROWED_STRING_COPY)
        xl6OutputPuts("const char* borrowed_raw = ",out);
    else if ( target->errorPhysicalAbi == XL6_IR_CALL_ERROR_PHYSICAL_RESULT_CARRIER )
        xl6OutputPuts("callResult = ", out);
    else if ( target->returnType != XL6_IR_TYPE_INVALID && target->nativeResultAbi==XL6_IR_CALL_NATIVE_RESULT_NONE )
        xl6OutputPuts(target->resultConversionAbi==XL6_IR_CALL_RESULT_CONVERSION_OWNED_STRING_TAKE
            ? "callResult.value.Data = " : "callResult.value = ", out);
    xl6OutputPrintf(out, "%s(", target->cName);
    if(target->nativeServicesRequired) {
        if(!xl6IrCgenEmitNativeServices(out,target))return 0;
        if(target->nativeContextAbi || target->parameterCount)xl6OutputPuts(", ",out);
    }
    if(target->nativeContextAbi!=XL6_IR_CALL_NATIVE_CONTEXT_NONE) {
        if(!xl6IrCgenEmitNativeContext(out,module,target))return 0;
        if(target->parameterCount)xl6OutputPuts(", ",out);
    }
    for ( size_t p = 0; p < target->parameterCount; ++p ) {
        if ( p ) xl6OutputPuts(", ", out);
        if (encoded && valuePlan.inputParameter==p+1)
            xl6OutputPuts("&native_value_input",out);
        else if ( target->parameterPhysicalAbis[p] == XL6_IR_CALL_PARAMETER_PHYSICAL_NATIVE_CLASS_HANDLE_BORROW )
            xl6OutputPrintf(out, "native%lu", (unsigned long)p);
        else if(target->parameterPhysicalAbis[p]==XL6_IR_CALL_PARAMETER_PHYSICAL_GENERIC_VALUE_ADDRESS_BORROW)
            xl6OutputPrintf(out,"&a%lu",(unsigned long)(p-1));
        else if(target->parameterPhysicalAbis[p]==XL6_IR_CALL_PARAMETER_PHYSICAL_GENERIC_ZERO_PAYLOAD)
            xl6OutputPuts("NULL",out);
        else if ( p == 0 ) xl6OutputPuts("receiver", out);
        else xl6OutputPrintf(out, "a%lu%s", (unsigned long)(p - 1), xl6IrCgenDirectPlaceSuffix(module, target, p));
    }
    if(target->nativeResultAbi==XL6_IR_CALL_NATIVE_RESULT_VALUE_SLOT_V1) {
        if(target->parameterCount || target->nativeContextAbi || target->nativeServicesRequired)xl6OutputPuts(", ",out);
        xl6OutputPuts(encoded&&valuePlan.output?"&native_value_output":"&callResult.value",out);
    }
    xl6OutputPuts(")", out);
    xl6OutputPuts(";\n", out);
    if(target->resultConversionAbi==XL6_IR_CALL_RESULT_CONVERSION_BORROWED_STRING_COPY)
        xl6IrCgenEmitBorrowedStringAdmission(out,module,target,"callResult.value","borrowed_raw","nativeScope");
    if(target->resultConversionAbi==XL6_IR_CALL_RESULT_CONVERSION_OWNED_STRING_TAKE)
        xl6IrCgenEmitOwnedStringAdmission(out,module,target,"callResult.value","nativeScope.failed");
    /* OWNED inputs are consumed only by actual native entry. Nulling the local
     * witnesses prevents the later failure path from releasing them again. */
    for ( size_t p = 1; p < target->parameterCount; ++p )
        if ( target->parameterAbis[p] == XL6_IR_PARAMETER_ABI_OWNED )
            xl6OutputPrintf(out, "      a%lu = (%s){0};\n", (unsigned long)(p - 1), xl6IrCgenType(module, target->parameterTypes[p]));
    if (!xl6IrCgenNativeCallValueEnd(out,module,&valuePlan,"callResult.value","nativeScope")) return 0;
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
/* Physical boxing only. The verified return row already fixes every field. */
static int xl6IrCgenEmitDynamicReturnedBox(XL6_Output* out,const XL6_IrModule* module,
    XL6_IrTypeId id,size_t scalarCodec,const char* value)
{
    const XL6_IrCallableBridgeResult* codec=xl6IrDynamicMethodScalarCodecGet(module,scalarCodec);
    if(codec) return codec->type==id && xl6IrCgenEmitCallableBridgeEncode(out,module,
        codec->type,codec->valueType,codec->presenceAbi,codec->encodeAbi,codec->integerDomain,
        codec->nativeScalarProfile,value,"boxedResult");
    const XL6_IrType* type=xl6IrTypeGet(&module->types,id);
    if(!type)return 0;
    if(type->cAbi==XL6_IR_C_ABI_MANAGED_STRING) {
        xl6OutputPrintf(out,"    boxedResult = %s.Data != NULL ? xrtValueString(%s) : NULL;\n",value,value);
        if(type->kind==XL6_IR_TYPE_NULLABLE)xl6OutputPuts("    if(boxedResult == NULL && !xrtGetError()) boxedResult = xrtValueRetain(xrtValueNull());\n",out);
        xl6OutputPrintf(out,"    xlir_string_drop(%s); %s = (xlir_string){0};\n",value,value);
    } else if(type->cAbi==XL6_IR_C_ABI_RUNTIME_XVALUE_PTR || type->cAbi==XL6_IR_C_ABI_MANAGED_BYTES) {
        xl6OutputPrintf(out,"    boxedResult = %s; %s = NULL;\n",value,value);
        if(type->kind==XL6_IR_TYPE_NULLABLE)xl6OutputPuts("    if(boxedResult == NULL)boxedResult = xrtValueRetain(xrtValueNull());\n",out);
    } else if (type->cAbi==XL6_IR_C_ABI_CLOSURE_PTR) {
        XL6_IrTypeId signature=xl6IrRuntimeCallableFieldSourceSignature(module,id);
        const XL6_IrType* base=xl6IrTypeGet(&module->types,signature);
        if (!base) return 0;
        if (type->kind==XL6_IR_TYPE_NULLABLE)
            xl6OutputPrintf(out,
                "    boxedResult = %s != NULL ? xlir_closure_to_xvalue_scalar(%s, xlir_xcall_entry_t%u, 0x%016llXULL) : xrtValueRetain(xrtValueNull());\n",
                value,value,(unsigned)signature,(unsigned long long)base->typeValueId);
        else xl6OutputPrintf(out,
            "    boxedResult = xlir_closure_to_xvalue_scalar(%s, xlir_xcall_entry_t%u, 0x%016llXULL);\n",
            value,(unsigned)signature,(unsigned long long)base->typeValueId);
        xl6OutputPrintf(out,"    xlir_closure_release(%s); %s = NULL;\n",value,value);
    } else if (type->cAbi==XL6_IR_C_ABI_RUNTIME_FUTURE_PTR) {
        const XL6_IrType* base=type->kind==XL6_IR_TYPE_NULLABLE ? xl6IrTypeGet(&module->types,type->params[0]) : type;
        if (!base || !base->typeValueId) return 0;
        xl6OutputPrintf(out,"    boxedResult = (%s != NULL || %s) ? xl6PayloadBoxFutureV1(%s, 0x%016llXULL) : NULL;\n"
            "    xrtFutureDestroy(%s); %s = NULL;\n",value,type->kind==XL6_IR_TYPE_NULLABLE ? "true" : "false",
            value,(unsigned long long)base->typeValueId,value,value);
    } else return 0;
    return !xl6OutputHasFailed(out);
}

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
    int vector=method && method->kind==XL6_IR_DYNAMIC_CALL_METHOD;
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

    if(vector)xl6OutputPrintf(out,
        "static bool xlir_dynamic_method_dm%lu(xvalue* receiver, size_t positionalCount, xvalue** positionalValues, size_t keywordCount, const xlir_string_view* keywordNames, xvalue** keywordValues, xrtcallresult* resultVector)\n"
        "{\n    xvalue* boxedResult = NULL; xvalue* variadic = NULL; size_t keywordIndex;\n"
        "    xrtcallresult prepared = {0}; xl6_error_scope adapterScope = {0};\n",
        (unsigned long)methodIndex);
    else xl6OutputPrintf(out,
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
        if ( type == NULL || (cType == NULL && item->matchAbi != XL6_IR_DYNAMIC_METHOD_MATCH_ZERO_PAYLOAD) )
            return xl6IrCgenDiag(diags, module, method->declaration,
                "XLIR C emitter dynamic method parameter type is unresolved");
        if ( parameter < method->fixedParameterCount )
            xl6OutputPrintf(out,
                "    xvalue* bound%lu = NULL; bool has%lu = false;\n",
                (unsigned long)parameter, (unsigned long)parameter);
        if (item->matchAbi == XL6_IR_DYNAMIC_METHOD_MATCH_ZERO_PAYLOAD) continue;
        xl6OutputPrintf(out, "    %s a%lu = (%s){0};\n",
                        cType, (unsigned long)parameter, cType);
        if ( type->cAbi == XL6_IR_C_ABI_MANAGED_STRING )
            xl6OutputPrintf(out,
                "    xlir_string_view text%lu = {0};\n",
                (unsigned long)parameter);
    }
    if(vector)xl6OutputPuts("    if(!xl6ErrorScopeBegin(&adapterScope)) { xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()}); return false; }\n",out);
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
        if (item->defaultAbi == XL6_IR_DYNAMIC_METHOD_DEFAULT_PROGRAM) {
            const XL6_IrFunction* program = &module->functions[item->defaultFunction - 1];
            const char* carrier = xl6IrCgenErrorResultType(module, item->type);
            if (!carrier) return xl6IrCgenDiag(diags, module, method->declaration,
                "XLIR dynamic default program has no checked result carrier");
            xl6OutputPrintf(out, "    if (!has%lu) { %s defaultResult = xlir_fn_%u(receiver",
                (unsigned long)parameter, carrier, (unsigned)program->symbol);
            for (size_t prefix = 0; prefix < parameter; ++prefix)
                xl6OutputPrintf(out, ", a%lu", (unsigned long)prefix);
            xl6OutputPrintf(out,
                "); if (!defaultResult.ok) { xl6ErrorScopePropagateFailure((xl6_error_result){false, xrtTakeError()}); goto fail; } a%lu = defaultResult.value; } else {\n",
                (unsigned long)parameter);
        }
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
        if (item->matchAbi == XL6_IR_DYNAMIC_METHOD_MATCH_ZERO_PAYLOAD) {
            /* Admission consumed the complete logical input. The exact native
             * physical parameter supplies NULL; there is no local to decode. */
            if (item->defaultAbi != XL6_IR_DYNAMIC_METHOD_DEFAULT_NONE) return 0;
        } else if (item->matchAbi==XL6_IR_DYNAMIC_METHOD_MATCH_SCALAR_CODEC) {
            const XL6_IrCallableBridgeResult* codec=xl6IrDynamicMethodScalarCodecGet(module,item->scalarCodec);
            char place[48],source[48]; snprintf(place,sizeof(place),"a%lu",(unsigned long)parameter);
            snprintf(source,sizeof(source),"bound%lu",(unsigned long)parameter);
            if(!codec)return 0;
            xl6OutputPrintf(out,"    if(has%lu) {\n",(unsigned long)parameter);
            if(!xl6IrCgenEmitCallableBridgeDecode(out,module,codec,place,source))return 0;
            xl6OutputPuts("    }",out);
            if(item->defaultAbi!=XL6_IR_DYNAMIC_METHOD_DEFAULT_NONE && item->defaultAbi!=XL6_IR_DYNAMIC_METHOD_DEFAULT_PROGRAM) {
                if(item->defaultAbi==XL6_IR_DYNAMIC_METHOD_DEFAULT_XVALUE_NULL)
                    xl6OutputPrintf(out," else a%lu=(%s){0};",(unsigned long)parameter,xl6IrCgenType(module,item->type));
                else if(codec->presenceAbi==XL6_IR_CALLABLE_PRESENCE_TAGGED)
                    xl6OutputPrintf(out," else { a%lu.hasValue=true; a%lu.value=%s; }",(unsigned long)parameter,(unsigned long)parameter,item->defaultLiteral);
                else xl6OutputPrintf(out," else a%lu=%s;",(unsigned long)parameter,item->defaultLiteral);
            }
            xl6OutputPuts("\n",out);
        } else if ( type->cAbi == XL6_IR_C_ABI_MANAGED_STRING ) {
            xl6OutputPrintf(out,
                "    if (has%lu) { if (!xrtValueGetString(bound%lu, &text%lu) || (a%lu = xlir_string_from_utf8(text%lu.Data, text%lu.Size)).Data == NULL) { ",
                (unsigned long)parameter, (unsigned long)parameter,
                (unsigned long)parameter, (unsigned long)parameter,
                (unsigned long)parameter, (unsigned long)parameter);
            xl6IrCgenEmitDynamicMethodError(
                out, "dynamic method argument has the wrong string type");
            xl6OutputPuts(" } }", out);
            if ( item->defaultAbi != XL6_IR_DYNAMIC_METHOD_DEFAULT_NONE &&
                 item->defaultAbi != XL6_IR_DYNAMIC_METHOD_DEFAULT_PROGRAM )
                xl6OutputPrintf(out,
                    " else if ((a%lu = xlir_string_from_utf8(%s, sizeof(%s) - 1u)).Data == NULL) goto fail;",
                    (unsigned long)parameter, item->defaultLiteral, item->defaultLiteral);
            xl6OutputPuts("\n", out);
        } else if ( type->cAbi == XL6_IR_C_ABI_MANAGED_BYTES ) {
            xl6OutputPrintf(out,
                "    if (has%lu && xrtValueType(bound%lu) != XVALUE_NULL) { a%lu = xrtValueRetain(bound%lu); if (a%lu == NULL) goto fail; }",
                (unsigned long)parameter, (unsigned long)parameter,
                (unsigned long)parameter, (unsigned long)parameter,
                (unsigned long)parameter);
            if(item->defaultAbi==XL6_IR_DYNAMIC_METHOD_DEFAULT_BYTES)
                xl6OutputPrintf(out," else if (!has%lu) { a%lu = xrtValueBytes((xbytesview){(const unsigned char*)%s,sizeof(%s)-1u}); if(!a%lu)goto fail; }",
                    (unsigned long)parameter,(unsigned long)parameter,item->defaultLiteral,item->defaultLiteral,(unsigned long)parameter);
            xl6OutputPuts("\n",out);
        } else if ( type->cAbi == XL6_IR_C_ABI_CLOSURE_PTR ) {
            XL6_IrTypeId signature=xl6IrRuntimeCallableFieldSourceSignature(module,item->type);
            if (!signature) return 0;
            xl6OutputPrintf(out,
                "    if (has%lu && !xlir_xvalue_project_closure_try_copy_into(bound%lu, (void (*)(void))xlir_xvalue_closure_entry_t%u, %s, &a%lu)) goto fail;\n",
                (unsigned long)parameter,(unsigned long)parameter,(unsigned)signature,
                type->kind==XL6_IR_TYPE_NULLABLE ? "true" : "false",(unsigned long)parameter);
        } else if ( type->cAbi == XL6_IR_C_ABI_RUNTIME_FUTURE_PTR ) {
            const XL6_IrType* base=type->kind==XL6_IR_TYPE_NULLABLE ? xl6IrTypeGet(&module->types,type->params[0]) : type;
            if (!base || !base->typeValueId) return 0;
            xl6OutputPrintf(out,"    if (has%lu && !xl6PayloadReadFutureV1(bound%lu, 0x%016llXULL, %s, &a%lu)) goto fail;\n",
                (unsigned long)parameter,(unsigned long)parameter,(unsigned long long)base->typeValueId,
                type->kind==XL6_IR_TYPE_NULLABLE ? "true" : "false",(unsigned long)parameter);
        } else if ( type->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR ) {
            xl6OutputPrintf(out,
                "    a%lu = xrtValueRetain(has%lu ? bound%lu : xrtValueNull()); if (a%lu == NULL) goto fail;\n",
                (unsigned long)parameter, (unsigned long)parameter,
                (unsigned long)parameter, (unsigned long)parameter);
        } else {
            return xl6IrCgenDiag(diags, module, method->declaration,
                "XLIR C emitter dynamic method projection ABI is unresolved");
        }
        if (item->defaultAbi == XL6_IR_DYNAMIC_METHOD_DEFAULT_PROGRAM) xl6OutputPuts("    }\n", out);
    }
    if ( method->variadicAbi != XL6_IR_CALL_VARIADIC_NONE ) {
        parameter = method->fixedParameterCount;
        xl6OutputPrintf(out,
            "    a%lu = variadic; variadic = NULL;\n",
            (unsigned long)parameter);
    }
    if(vector)xl6OutputPuts("    if(adapterScope.failed || xrtGetError())goto fail;\n",out);
    if ( target->kind == XL6_IR_CALL_TARGET_EXTERN_C ) {
        if ( carrierCType == NULL || !xl6IrCgenEmitDynamicNativeWitness(out, module, target, diags) ) return 0;
    } else {
        xl6OutputPuts("    ", out);
        if ( carrierCType != NULL ) xl6OutputPuts("callResult = ", out);
        else if ( resultType != NULL ) xl6OutputPuts("returned = ", out);
        xl6IrCgenEmitDispatchTargetCall(out,target,NULL,method->parameterCount);
        xl6OutputPuts(";\n",out);
    }
    if(vector) {
        for(parameter=0;parameter<method->parameterCount;++parameter) {
            const XL6_IrDynamicMethodParameter* item=&module->dynamicMethodParameters[method->firstParameter+parameter];
            if(item->abi==XL6_IR_PARAMETER_ABI_OWNED)
                xl6OutputPrintf(out,"    a%lu = (%s){0};\n",(unsigned long)parameter,xl6IrCgenType(module,item->type));
        }
    }
    if (carrierCType != NULL)
        xl6OutputPuts("    if (!callResult.ok) { xl6_error_result failure = {false, xrtTakeError()}; xl6ErrorScopePropagateFailure(failure); }\n", out);
    for ( parameter = 0; parameter < method->parameterCount; ++parameter ) {
        const XL6_IrDynamicMethodParameter* item =
            &module->dynamicMethodParameters[
                method->firstParameter + parameter];
        const XL6_IrType* type = xl6IrTypeGet(&module->types, item->type);
        if ( item->abi != XL6_IR_PARAMETER_ABI_BORROW ) continue;
        if ( type->cAbi == XL6_IR_C_ABI_CLOSURE_PTR )
            xl6OutputPrintf(out, "    xlir_closure_release(a%lu); a%lu = NULL;\n",
                (unsigned long)parameter, (unsigned long)parameter);
        else if (type->cAbi==XL6_IR_C_ABI_RUNTIME_FUTURE_PTR)
            xl6OutputPrintf(out,"    xrtFutureDestroy(a%lu); a%lu = NULL;\n",(unsigned long)parameter,(unsigned long)parameter);
        else if ( type->cAbi == XL6_IR_C_ABI_MANAGED_STRING )
            xl6OutputPrintf(out, "    xlir_string_drop(a%lu); a%lu = (xlir_string){0};\n",
                (unsigned long)parameter, (unsigned long)parameter);
        else if ( type->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR ||
                  type->cAbi == XL6_IR_C_ABI_MANAGED_BYTES )
            xl6OutputPrintf(out,
                "    xrtValueRelease(a%lu); a%lu = NULL;\n",
                (unsigned long)parameter, (unsigned long)parameter);
        if(vector)xl6OutputPuts("    if(xrtGetError())xl6ErrorScopeCaptureFailure(&adapterScope);\n",out);
    }
    if ( carrierCType != NULL ) {
        if(vector) {
            xl6OutputPuts("    if (!callResult.ok) goto fail;\n",out);
            if(resultType)xl6OutputPuts("    returned = callResult.value; callResult.value = (",out);
            if(resultType)xl6OutputPrintf(out,"%s){0};\n",resultCType);
        } else {
        xl6OutputPuts(
            "    if (!callResult.ok) { if (xrtGetError() == NULL) xrtSetErrorTake(xlir_error_create(\"RuntimeError\", \"dynamic method target returned an error without runtime state\")); return NULL; }\n",
            out);
        if ( resultType != NULL )
            xl6OutputPuts("    returned = callResult.value;\n", out);
        }
    }
    if(vector) {
        if(!resultType) {
            xl6OutputPuts("    boxedResult = xrtValueRetain(xrtValueNull());\n    if(!boxedResult || !xrtCallResultSetTake(&prepared,0,&boxedResult))goto fail;\n",out);
        } else {
            size_t count=method->returnCount;
            for(size_t field=0;field<count;++field) {
                char place[64]; snprintf(place,sizeof(place),count>1?"returned.e%lu":"returned",(unsigned long)field);
                XL6_IrTypeId fieldType=count>1?resultType->params[field]:method->returnType;
                if(!xl6IrCgenEmitDynamicReturnedBox(out,module,fieldType,method->firstScalarResultCodec+field+1,place))return 0;
                xl6OutputPrintf(out,"    if(!boxedResult || adapterScope.failed || xrtGetError() || !xrtCallResultSetTake(&prepared,%lu,&boxedResult))goto fail;\n",(unsigned long)field);
            }
        }
        if(resultType && !xl6IrCgenEmitOwnedPlaceRelease(out,module,method->returnType,"returned",diags))return 0;
        xl6OutputPuts("    if(adapterScope.failed || xrtGetError() || !xrtCallResultMove(resultVector,&prepared))goto fail;\n"
            "    { xl6_error_result outcome = xl6ErrorScopeEnd(&adapterScope); if(!outcome.ok) { xrtCallResultClear(resultVector); xl6ErrorScopePropagateFailure(outcome); return false; } return true; }\n"
            "conversion_fail:\nfail:\n    xl6ErrorScopeCaptureFailure(&adapterScope);\n",out);
        for(parameter=0;parameter<method->parameterCount;++parameter) {
            const XL6_IrDynamicMethodParameter* item=&module->dynamicMethodParameters[method->firstParameter+parameter];
            char place[48]; snprintf(place,sizeof(place),"a%lu",(unsigned long)parameter);
            if(!xl6IrCgenEmitOwnedPlaceRelease(out,module,item->type,place,diags))return 0;
            xl6OutputPuts("    if(xrtGetError())xl6ErrorScopeCaptureFailure(&adapterScope);\n",out);
        }
        if(resultType && !xl6IrCgenEmitOwnedPlaceRelease(out,module,method->returnType,"returned",diags))return 0;
        xl6OutputPuts("    xrtValueRelease(boxedResult); xrtValueRelease(variadic); xrtCallResultUnit(&prepared);\n"
            "    { xl6_error_result outcome = xl6ErrorScopeEnd(&adapterScope); xl6ErrorScopePropagateFailure(outcome); return false; }\n}\n",out);
        return !xl6OutputHasFailed(out);
    }
    if ( resultType == NULL )
        xl6OutputPuts(
            "    boxedResult = xrtValueRetain(xrtValueNull());\n", out);
    else if (xl6IrDynamicMethodScalarCodecGet(module,method->firstScalarResultCodec+1)) {
        if(!xl6IrCgenEmitDynamicReturnedBox(out,module,method->returnType,method->firstScalarResultCodec+1,"returned"))return 0;
    }
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
    else if ( resultType->cAbi == XL6_IR_C_ABI_CLOSURE_PTR || resultType->cAbi==XL6_IR_C_ABI_RUNTIME_FUTURE_PTR ) {
        if (!xl6IrCgenEmitDynamicReturnedBox(out, module, method->returnType, method->firstScalarResultCodec+1,"returned")) return 0;
    }
    else
        return xl6IrCgenDiag(diags, module, method->declaration,
            "XLIR C emitter dynamic method result boxing ABI is unresolved");
    xl6OutputPuts(
        "    if (boxedResult == NULL && xrtGetError() == NULL) xrtSetErrorTake(xlir_error_create(\"RuntimeError\", \"dynamic method result boxing failed\"));\n"
        "    return boxedResult;\n"
        "conversion_fail:\nfail:\n"
        "    { xl6_error_result failure = {false, xrtTakeError()}; xl6ErrorScopePropagateFailure(failure); }\n",
        out);
    for ( parameter = 0; parameter < method->parameterCount; ++parameter ) {
        const XL6_IrDynamicMethodParameter* item =
            &module->dynamicMethodParameters[
                method->firstParameter + parameter];
        const XL6_IrType* type = xl6IrTypeGet(&module->types, item->type);
        if ( type->cAbi == XL6_IR_C_ABI_CLOSURE_PTR )
            xl6OutputPrintf(out, "    xlir_closure_release(a%lu);\n", (unsigned long)parameter);
        else if (type->cAbi==XL6_IR_C_ABI_RUNTIME_FUTURE_PTR)
            xl6OutputPrintf(out,"    xrtFutureDestroy(a%lu);\n",(unsigned long)parameter);
        else if ( type->cAbi == XL6_IR_C_ABI_MANAGED_STRING )
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
