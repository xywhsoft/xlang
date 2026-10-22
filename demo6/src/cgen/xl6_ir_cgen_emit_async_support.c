/* Closed enum-to-C mappings, not protocol selection. The independent IR
 * verifier has already fixed targets, modes and the complete field vector. */
static const char* xl6IrCgenClassHashModeV3(XL6_IrClassHashAbi abi)
{
    switch (abi) {
    case XL6_IR_CLASS_HASH_ABI_NONE:
    case XL6_IR_CLASS_HASH_ABI_INTERNAL_CALL: return "XL6_CLASS_IDENTITY_NONE_V3";
    case XL6_IR_CLASS_HASH_ABI_RUNTIME_REFERENCE: return "XL6_CLASS_IDENTITY_REFERENCE_V3";
    case XL6_IR_CLASS_HASH_ABI_RUNTIME_DERIVED: return "XL6_CLASS_IDENTITY_DERIVED_V3";
    default: return NULL;
    }
}

static const char* xl6IrCgenClassEqualModeV3(XL6_IrClassEqualityAbi abi)
{
    switch (abi) {
    case XL6_IR_CLASS_EQUALITY_ABI_NONE:
    case XL6_IR_CLASS_EQUALITY_ABI_INTERNAL_CALL: return "XL6_CLASS_IDENTITY_NONE_V3";
    case XL6_IR_CLASS_EQUALITY_ABI_RUNTIME_REFERENCE: return "XL6_CLASS_IDENTITY_REFERENCE_V3";
    case XL6_IR_CLASS_EQUALITY_ABI_RUNTIME_DERIVED: return "XL6_CLASS_IDENTITY_DERIVED_V3";
    default: return NULL;
    }
}

static int xl6IrCgenEmitAsyncAndCallableRuntime(
    XL6_Output* out, const XL6_IrModule* module,
    const XL6_IrCgenNeeds* needs, XL6_DiagList* diags)
{
    size_t i;
    size_t parameter;
    if ( needs->needsFutureAggregateAll ) {
        size_t aggregateIndex;
        for ( aggregateIndex = 0;
              aggregateIndex < module->instructionCount; ++aggregateIndex ) {
            const XL6_IrInstruction* aggregate =
                &module->instructions[aggregateIndex];
            size_t earlier;
            if ( !xl6IrCgenInstructionIsReachable(module, aggregateIndex) ||
                 aggregate->kind != XL6_IR_INSTR_RUNTIME_CALL ||
                  aggregate->asyncResultAbi == XL6_IR_ASYNC_RESULT_VOID ||
                  (aggregate->runtimeOp != XL6_IR_RUNTIME_FUTURE_ALL_XVALUES &&
                  aggregate->runtimeOp !=
                      XL6_IR_RUNTIME_FUTURE_ALL_XVALUES_ARRAY) )
                continue;
            for ( earlier = 0; earlier < aggregateIndex; ++earlier ) {
                const XL6_IrInstruction* prior = &module->instructions[earlier];
                if ( xl6IrCgenInstructionIsReachable(module, earlier) &&
                     prior->kind == XL6_IR_INSTR_RUNTIME_CALL &&
                      (prior->runtimeOp == XL6_IR_RUNTIME_FUTURE_ALL_XVALUES ||
                       prior->runtimeOp ==
                           XL6_IR_RUNTIME_FUTURE_ALL_XVALUES_ARRAY) &&
                      prior->asyncResultAbi != XL6_IR_ASYNC_RESULT_VOID &&
                      prior->payloadType == aggregate->payloadType ) break;
            }
            if ( earlier == aggregateIndex &&
                 !xl6IrCgenEmitFuturePayloadBoxer(
                     out, module, aggregate, diags) ) return 0;
        }
        xl6OutputPuts("\n", out);
    }
    if ( needs->needsTaskSpawnRuntime ) {
        size_t spawnIndex;
        for ( spawnIndex = 0;
              spawnIndex < module->instructionCount; ++spawnIndex ) {
            const XL6_IrInstruction* spawn =
                &module->instructions[spawnIndex];
            if ( !xl6IrCgenInstructionIsReachable(module, spawnIndex) ||
                 spawn->kind != XL6_IR_INSTR_RUNTIME_CALL ||
                 spawn->runtimeOp != XL6_IR_RUNTIME_TASK_SPAWN_CLOSURE )
                continue;
            if ( !xl6IrCgenEmitTaskSpawnHelper(
                     out, module, spawn, spawnIndex, diags) ) return 0;
        }
    }
    if ( needs->needsFutureContinuationRuntime ) {
        size_t continuationIndex;
        for ( continuationIndex = 0;
              continuationIndex < module->instructionCount;
              ++continuationIndex ) {
            const XL6_IrInstruction* continuation =
                &module->instructions[continuationIndex];
            if ( !xl6IrCgenInstructionIsReachable(
                     module, continuationIndex) ||
                 continuation->kind != XL6_IR_INSTR_RUNTIME_CALL ||
                 continuation->runtimeOp !=
                     XL6_IR_RUNTIME_FUTURE_CONTINUE_CLOSURE )
                continue;
            if ( !xl6IrCgenEmitFutureContinuationHelper(
                     out, module, continuation, continuationIndex, diags) )
                return 0;
        }
    }
    if ( needs->needsStringTimeConvertRuntime || needs->needsXValueTimeConvertRuntime ||
         needs->needsTimeToStringRuntime ) {
        if ( needs->needsStringTimeConvertRuntime )
            xl6OutputPuts(
                "extern bool xrtTimeParseAny(xlir_string_view, int64_t*);\n"
                "static int64_t xlir_string_to_time_checked(xlir_string text)\n"
                "{\n"
                "    int64_t result = 0;\n"
                "    if (xrtTimeParseAny(text, &result)) return result;\n"
                "    if (xrtGetError() == NULL)\n"
                "        xrtSetErrorTake(xlir_error_create(\"ValueError\", \"invalid time string\"));\n"
                "    return 0;\n"
                "}\n",
                out);
        if ( needs->needsXValueTimeConvertRuntime )
            xl6OutputPuts(
                "extern bool xrtValueGetTime(const xvalue*, int64_t*);\n"
                "static int64_t xlir_xvalue_to_time_checked(const xvalue* value)\n"
                "{\n"
                "    int64_t result = 0;\n"
                "    if (value != NULL && xrtValueGetTime(value, &result)) return result;\n"
                "    if (xrtGetError() == NULL)\n"
                "        xrtSetErrorTake(xlir_error_create(\"TypeError\", \"dynamic value is not time\"));\n"
                "    return 0;\n"
                "}\n",
                out);
        if ( needs->needsTimeToStringRuntime )
            xl6OutputPuts(
                "extern char* xrtTimeFormat(int64_t, int, xlir_string_view);\n"
                "static xlir_string xlir_time_to_string(int64_t value)\n"
                "{ return xlir_string_take_cstr(xrtTimeFormat(value, 0, xrtStrView(\"%F %T\"))); }\n",
                out);
        xl6OutputPuts("\n", out);
    }
    if ( needs->needsBoolToStringRuntime || needs->needsI64ToStringRuntime || needs->needsU64ToStringRuntime ||
         needs->needsF64ToStringRuntime || needs->needsXValueToStringRuntime ) {
        xl6OutputPuts("extern char* xrtStrDupN(const char*, size_t);\n", out);
        if ( needs->needsI64ToStringRuntime )
            xl6OutputPuts("extern bool xrtIntFormatTo(int64_t, xlir_string_view, char*, size_t, size_t*);\n", out);
        if ( needs->needsU64ToStringRuntime )
            xl6OutputPuts("extern bool xrtUIntFormatTo(uint64_t, xlir_string_view, char*, size_t, size_t*);\n", out);
        if ( needs->needsF64ToStringRuntime )
            xl6OutputPuts("extern bool xrtNumFormatTo(double, xlir_string_view, char*, size_t, size_t*);\n", out);
        if ( needs->needsBoolToStringRuntime )
            xl6OutputPuts(
                "static char* xlir_bool_to_string(bool value)\n"
                "{ return xrtStrDup(value ? \"true\" : \"false\"); }\n", out);
        if ( needs->needsI64ToStringRuntime )
            xl6OutputPuts(
                "static char* xlir_i64_to_string(int64_t value)\n"
                "{ char buffer[64]; size_t size = 0;\n"
                "  if (!xrtIntFormatTo(value, (xlir_string_view){\"\", 0}, buffer, sizeof(buffer), &size)) return xrtStrDup(\"\");\n"
                "  return xrtStrDupN(buffer, size); }\n", out);
        if ( needs->needsU64ToStringRuntime )
            xl6OutputPuts(
                "static char* xlir_u64_to_string(uint64_t value)\n"
                "{ char buffer[64]; size_t size = 0;\n"
                "  if (!xrtUIntFormatTo(value, (xlir_string_view){\"\", 0}, buffer, sizeof(buffer), &size)) return xrtStrDup(\"\");\n"
                "  return xrtStrDupN(buffer, size); }\n", out);
        if ( needs->needsF64ToStringRuntime )
            xl6OutputPuts(
                "static char* xlir_f64_to_string(double value)\n"
                "{ char buffer[128]; size_t size = 0;\n"
                "  if (!xrtNumFormatTo(value, (xlir_string_view){\"\", 0}, buffer, sizeof(buffer), &size)) return xrtStrDup(\"\");\n"
                "  return xrtStrDupN(buffer, size); }\n", out);
        if ( needs->needsXValueToStringRuntime )
            xl6OutputPuts(
                "extern xlir_string xl6ValueTextV1(const xvalue*);\n", out);
        xl6OutputPuts("\n", out);
    }
    if ( needs->needsTypeIdToStringRuntime ) {
        size_t typeIndex;
        xl6OutputPuts(
            "static char* xlir_type_id_to_string(uint64_t type_id)\n"
            "{\n",
            out);
        for ( typeIndex = 0; typeIndex < module->types.count; ++typeIndex ) {
            const XL6_IrType* type = &module->types.items[typeIndex];
            size_t priorType;
            if ( type->typeValueId == 0 ) continue;
            for ( priorType = 0; priorType < typeIndex; ++priorType )
                if ( module->types.items[priorType].typeValueId ==
                         type->typeValueId )
                    break;
            if ( priorType != typeIndex ) continue;
            xl6OutputPrintf(out,
                "    if (type_id == 0x%016llXULL) return xrtStrDup(",
                (unsigned long long)type->typeValueId);
            xl6IrCgenEmitCStringLiteral(
                out, type->displayName != NULL
                    ? type->displayName : "unknown");
            xl6OutputPuts(");\n", out);
        }
        xl6OutputPuts(
            "    return xrtStrDup(\"unknown\");\n"
            "}\n\n",
            out);
    }
    if ( needs->needsClassValueIdentityRuntime ) {
        size_t instructionIndex;
        xl6OutputPuts(
            "#include \"__xl_runtime/xl6_code_lease.h\"\n",
            out);
        if (module->abi.classIdentityAbi != XL6_IR_CLASS_IDENTITY_RESIDENT_SCHEMA_V3)
            return xl6IrCgenDiag(diags, module, XL6_IR_NODE_INVALID,
                "XLIR class identity resident schema ABI is unresolved");
        for(size_t targetIndex=0;targetIndex<module->callTargetCount;++targetIndex){
            const XL6_IrCallTarget* target=&module->callTargets[targetIndex];
            if(target->reachability==XL6_IR_REACHABILITY_UNREACHABLE||
                target->nativeContextAbi!=XL6_IR_CALL_NATIVE_CONTEXT_CLASS_RESULT_V1)continue;
            size_t prior=0;for(;prior<targetIndex;++prior)
                if(module->callTargets[prior].reachability!=XL6_IR_REACHABILITY_UNREACHABLE&&
                    module->callTargets[prior].nativeContextAbi==XL6_IR_CALL_NATIVE_CONTEXT_CLASS_RESULT_V1&&
                    module->callTargets[prior].nativeContextType==target->nativeContextType)break;
            if(prior!=targetIndex)continue;
            const XL6_IrClassIdentityPlan* identity=xl6IrClassIdentityPlanGet(module,target->nativeContextType);
            const XL6_IrClassField* field=xl6IrNativeClassResultContextField(module,target);
            if(!identity||!field||!field->nativeHandlePolicyCName)return 0;
            /* A foreign Future result can be retained without any synchronous
             * constructor/bind instruction for T. Its descriptor still calls
             * this exact resident getter, so declare it independently. */
            xl6OutputPrintf(out,"extern const struct XL6_NativeHandlePolicyV1* %s(void);\n",field->nativeHandlePolicyCName);
            xl6OutputPrintf(out,"static const XL6_ClassIdentitySchemaV3 xlir_native_result_schema_t%u = {\n"
                "    sizeof(XL6_ClassIdentitySchemaV3), UINT64_C(0x%016llX), %u,\n"
                "    XL6_CLASS_IDENTITY_REFERENCE_V3, XL6_CLASS_IDENTITY_REFERENCE_V3, NULL, 0, {0}\n};\n\n",
                (unsigned)target->nativeContextType,(unsigned long long)identity->identity,(unsigned)identity->runtimeTypeTag);
        }
        for ( instructionIndex = 0;
              instructionIndex < module->instructionCount;
              ++instructionIndex ) {
            const XL6_IrInstruction* instruction =
                &module->instructions[instructionIndex];
            const XL6_IrClassIdentityPlan* identityPlan;
            const XL6_IrClassLayout* classLayout;
            const char* hashMode;
            const char* equalMode;
            size_t priorInstruction;
            size_t fieldOffset;
            if ( !xl6IrCgenInstructionIsReachable(module, instructionIndex) ||
                 instruction->kind != XL6_IR_INSTR_RUNTIME_CALL ||
                 instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_CLASS_NEW ) continue;
            for ( priorInstruction = 0;
                  priorInstruction < instructionIndex;
                  ++priorInstruction ) {
                const XL6_IrInstruction* prior =
                    &module->instructions[priorInstruction];
                if ( xl6IrCgenInstructionIsReachable(
                         module, priorInstruction) &&
                     prior->kind == XL6_IR_INSTR_RUNTIME_CALL &&
                     prior->runtimeOp == XL6_IR_RUNTIME_XVALUE_CLASS_NEW &&
                     prior->type == instruction->type )
                    break;
            }
            if ( priorInstruction != instructionIndex ) continue;
            identityPlan = xl6IrClassIdentityPlanGet(module, instruction->type);
            classLayout = xl6IrCgenClassLayout(module, instruction->type);
            if (identityPlan == NULL || classLayout == NULL ||
                (hashMode = xl6IrCgenClassHashModeV3(identityPlan->hashAbi)) == NULL ||
                (equalMode = xl6IrCgenClassEqualModeV3(identityPlan->equalityAbi)) == NULL)
                return xl6IrCgenDiag(diags, module, instruction->origin,
                    "XLIR C emitter class value identity schema is unresolved");
            if (identityPlan->fieldCount != 0) {
                xl6OutputPrintf(out,
                    "static const XL6_ClassIdentityFieldV3 xlir_class_identity_fields_t%u[] = {\n",
                    (unsigned int)instruction->type);
                for (fieldOffset = 0; fieldOffset < identityPlan->fieldCount; ++fieldOffset) {
                    const XL6_IrClassField* field = &module->classFields[
                        module->classIdentityFields[identityPlan->firstField + fieldOffset].classField];
                    const XL6_IrType* fieldType = xl6IrTypeGet(&module->types, field->type);
                    xl6OutputPuts("    {", out);
                    xl6IrCgenEmitCStringLiteral(out, field->name);
                    xl6OutputPrintf(out, ", %lu, UINT64_C(0x%016llX)},\n",
                        (unsigned long)strlen(field->name),
                        (unsigned long long)fieldType->typeValueId);
                }
                xl6OutputPuts("};\n", out);
            }
            xl6OutputPrintf(out,
                "static const XL6_ClassIdentitySchemaV3 xlir_class_identity_schema_t%u = {\n"
                "    sizeof(XL6_ClassIdentitySchemaV3), UINT64_C(0x%016llX), %u,\n"
                "    %s, %s, ",
                (unsigned int)instruction->type,
                (unsigned long long)identityPlan->identity,
                (unsigned int)identityPlan->runtimeTypeTag, hashMode, equalMode);
            if (identityPlan->fieldCount != 0)
                xl6OutputPrintf(out, "xlir_class_identity_fields_t%u",
                    (unsigned int)instruction->type);
            else xl6OutputPuts("NULL", out);
            xl6OutputPrintf(out, ", %lu, ", (unsigned long)identityPlan->fieldCount);
            if (classLayout->persistenceObjectAbi == XL6_IR_PERSISTENCE_OBJECT_OPEN_DYNAMIC_ROOT ||
                classLayout->persistenceObjectAbi == XL6_IR_PERSISTENCE_OBJECT_OPEN_DYNAMIC_SCHEMA) {
                if (classLayout->persistenceDynamicField >= module->classFieldCount ||
                    classLayout->persistenceDynamicHandleField >= module->classFieldCount)
                    return xl6IrCgenDiag(diags, module, instruction->origin,
                        "XLIR class schema has no verified dynamic storage path");
                const XL6_IrClassField* storage = &module->classFields[classLayout->persistenceDynamicField];
                const XL6_IrClassField* handle = &module->classFields[classLayout->persistenceDynamicHandleField];
                xl6OutputPuts("{", out);
                xl6IrCgenEmitCStringLiteral(out, storage->name);
                xl6OutputPrintf(out, ", %lu, ", (unsigned long)strlen(storage->name));
                xl6IrCgenEmitCStringLiteral(out, handle->name);
                xl6OutputPrintf(out, ", %lu}", (unsigned long)strlen(handle->name));
            } else xl6OutputPuts("{0}", out);
            xl6OutputPuts("\n};\n\n", out);
        }
    }
    if ( needs->needsClassDerivedToStringRuntime ) {
        size_t instructionIndex;
        xl6OutputPuts("#include <__xl_runtime/xl6_class_text.h>\n", out);
        for ( instructionIndex = 0;
              instructionIndex < module->instructionCount;
              ++instructionIndex ) {
            const XL6_IrInstruction* instruction =
                &module->instructions[instructionIndex];
            const XL6_IrClassLayout* layout;
            const XL6_IrType* classType;
            size_t priorInstruction;
            size_t fieldOffset;
            size_t includedFields = 0;
            if ( !xl6IrCgenInstructionIsReachable(
                     module, instructionIndex) ||
                 instruction->kind != XL6_IR_INSTR_RUNTIME_CALL ||
                 instruction->runtimeOp !=
                     XL6_IR_RUNTIME_CLASS_DERIVED_TO_STRING )
                continue;
            for ( priorInstruction = 0;
                  priorInstruction < instructionIndex;
                  ++priorInstruction )
                if ( xl6IrCgenInstructionIsReachable(
                         module, priorInstruction) &&
                     module->instructions[priorInstruction].kind ==
                         XL6_IR_INSTR_RUNTIME_CALL &&
                     module->instructions[priorInstruction].runtimeOp ==
                         XL6_IR_RUNTIME_CLASS_DERIVED_TO_STRING &&
                     module->instructions[priorInstruction].payloadType ==
                         instruction->payloadType )
                    break;
            if ( priorInstruction != instructionIndex ) continue;
            if ( !xl6IrCgenRuntimeClassDerivedStringAbi(
                     module, instruction) )
                return xl6IrCgenDiag(diags, module, instruction->origin,
                    "XLIR C emitter derived class string helper ABI is unresolved");
            layout = xl6IrCgenClassLayout(
                module, instruction->payloadType);
            classType = xl6IrTypeGet(
                &module->types, instruction->payloadType);
            if ( layout == NULL || classType == NULL ||
                 classType->name == NULL || classType->typeValueId == 0 )
                return xl6IrCgenDiag(diags, module, instruction->origin,
                    "XLIR C emitter derived class string layout name is unresolved");
            for ( fieldOffset = layout->inheritedFieldCount;
                  fieldOffset < layout->fieldCount; ++fieldOffset )
                if ( module->classFields[layout->firstField + fieldOffset].stringDeriveIncluded )
                    ++includedFields;
            if ( includedFields != 0 )
                xl6OutputPrintf(out,
                    "static const xstrview xlir_class_string_fields_t%u[] = {\n",
                    (unsigned int)instruction->payloadType);
            for ( fieldOffset = layout->inheritedFieldCount;
                  fieldOffset < layout->fieldCount;
                  ++fieldOffset ) {
                const XL6_IrClassField* field =
                    &module->classFields[
                        layout->firstField + fieldOffset];
                if ( !field->stringDeriveIncluded ) continue;
                xl6OutputPuts("    { ", out);
                xl6IrCgenEmitCStringLiteral(
                    out, field->name != NULL ? field->name : "");
                xl6OutputPuts(", sizeof(", out);
                xl6IrCgenEmitCStringLiteral(
                    out, field->name != NULL ? field->name : "");
                xl6OutputPuts(") - 1u },\n", out);
            }
            if ( includedFields != 0 ) xl6OutputPuts("};\n", out);
            xl6OutputPrintf(out,
                "static const XL6_ClassTextSchemaV1 xlir_class_string_schema_t%u = {\n    { ",
                (unsigned int)instruction->payloadType);
            xl6IrCgenEmitCStringLiteral(out, classType->name);
            xl6OutputPuts(", sizeof(", out);
            xl6IrCgenEmitCStringLiteral(out, classType->name);
            xl6OutputPrintf(out, ") - 1u }, INT64_C(%u), UINT64_C(0x%016llX), ",
                (unsigned int)layout->runtimeTypeTag,
                (unsigned long long)classType->typeValueId);
            if ( includedFields != 0 )
                xl6OutputPrintf(out, "xlir_class_string_fields_t%u, %luu\n};\n\n",
                    (unsigned int)instruction->payloadType, (unsigned long)includedFields);
            else xl6OutputPuts("NULL, 0u\n};\n\n", out);
        }
    }
    if ( needs->needsStringContainsRuntime ) {
        xl6OutputPuts(
            "extern bool xrtStrContains(xlir_string_view, xlir_string_view);\n"
            "extern bool xrtStrCaseContains(xlir_string_view, xlir_string_view);\n"
            "bool xl6_xrt_string_contains(xlir_string text, xlir_string part, bool ignoreCase)\n"
            "{\n"
            "    xlir_string_view textView = text;\n"
            "    xlir_string_view partView = part;\n"
            "    return ignoreCase ? xrtStrCaseContains(textView, partView) : xrtStrContains(textView, partView);\n"
            "}\n\n", out);
    }
    if ( needs->needsStringIndexOfBridgeRuntime ) {
        xl6OutputPuts(
            "extern size_t xrtUtf8Find(xlir_string_view, xlir_string_view, size_t);\n"
            "extern size_t xrtUtf8CaseFind(xlir_string_view, xlir_string_view, size_t);\n"
            "int64_t xl6_xrt_string_index_of(xlir_string text, xlir_string part, bool ignoreCase)\n"
            "{\n"
            "    size_t index = ignoreCase\n"
            "        ? xrtUtf8CaseFind(text, part, 0u)\n"
            "        : xrtUtf8Find(text, part, 0u);\n"
            "    return index == SIZE_MAX ? -1 : (int64_t)index;\n"
            "}\n\n", out);
    }
    if ( needs->needsTakeCstrBridgeRuntime ) {
        xl6OutputPuts(
            "xlir_string xl6_xrt_take_cstr(void* value)\n"
            "{\n"
            "    xlir_string text = value != NULL\n"
            "        ? (xlir_string){ (const char*)value, strlen((const char*)value) }\n"
            "        : xlir_string_from_utf8(\"\", 0);\n"
            "    return text;\n"
            "}\n\n",
            out);
    }
    if ( needs->needsPrintRuntime ) {
        xl6OutputPuts("extern bool xl6_xrt_print(xvalue*, bool);\n", out);
    }
    if ( needs->needsLanguageFunctionRuntime ) {
        xl6OutputPuts(
            "extern bool xrtValueGetBool(const xvalue*, bool*);\n"
            "extern bool xrtValueGetInt(const xvalue*, int64_t*);\n"
            "extern bool xrtValueGetUInt(const xvalue*, uint64_t*);\n"
            "extern bool xrtValueGetFloat(const xvalue*, double*);\n"
            "extern bool xrtValueGetPointer(const xvalue*, void**);\n",
            out);
        if ( needs->needsLanguageFunctionStringRuntime )
            xl6OutputPuts(
                "extern bool xrtValueGetString(const xvalue*, xlir_string_view*);\n"
                "extern char* xrtStrDupN(const char*, size_t);\n"
                "extern xvalue* xrtValueString(xlir_string_view);\n",
                out);
        size_t targetIndex;
        for ( targetIndex = 0; targetIndex < module->callTargetCount;
              ++targetIndex ) {
            const XL6_IrCallTarget* target =
                &module->callTargets[targetIndex];
            size_t parameterIndex;
            if ( target->reachability == XL6_IR_REACHABILITY_UNREACHABLE )
                continue;
            for ( parameterIndex = 0;
                  parameterIndex < target->parameterCount;
                  ++parameterIndex ) {
                size_t priorTarget;
                size_t priorParameter;
                XL6_IrTypeId signatureType;
                int emitted = 0;
                if ( target->parameterPhysicalAbis[parameterIndex] !=
                         XL6_IR_CALL_PARAMETER_PHYSICAL_LANGUAGE_FUNCTION_VALUE )
                    continue;
                signatureType = target->parameterTypes[parameterIndex];
                for ( priorTarget = 0; priorTarget <= targetIndex && !emitted;
                      ++priorTarget ) {
                    const XL6_IrCallTarget* prior =
                        &module->callTargets[priorTarget];
                    size_t limit = priorTarget == targetIndex
                        ? parameterIndex : prior->parameterCount;
                    if ( prior->reachability ==
                             XL6_IR_REACHABILITY_UNREACHABLE ) continue;
                    for ( priorParameter = 0; priorParameter < limit;
                          ++priorParameter )
                        if ( prior->parameterPhysicalAbis[priorParameter] ==
                                  XL6_IR_CALL_PARAMETER_PHYSICAL_LANGUAGE_FUNCTION_VALUE &&
                             prior->parameterTypes[priorParameter] ==
                                 signatureType ) {
                            emitted = 1;
                            break;
                        }
                }
                if ( !emitted &&
                     !xl6IrCgenEmitLanguageFunctionValueAdapter(
                         out, module, signatureType, diags) ) return 0;
            }
        }
        xl6OutputPuts("\n", out);
    }
    /* One representation thunk per explicitly requested callback signature.
     * The IR fixes the exact T; there is no result allocation, ownership or
     * TaskPool algorithm here, only the verified bool(env,T*) call ABI. */
    for(size_t type=1;type<module->types.count;++type){
        int requested=0;
        for(size_t call=0;call<module->callTargetCount&&!requested;++call){
            const XL6_IrCallTarget* target=&module->callTargets[call];
            if(target->reachability==XL6_IR_REACHABILITY_UNREACHABLE)continue;
            for(size_t p=0;p<target->parameterCount;++p)
                if(target->parameterPhysicalAbis[p]==XL6_IR_CALL_PARAMETER_PHYSICAL_CHECKED_RESULT_CLOSURE_BORROW&&
                    target->parameterTypes[p]==type){requested=1;break;}
        }
        if(!requested)continue;
        const XL6_IrType* signature=xl6IrTypeGet(&module->types,(XL6_IrTypeId)type);
        const char* result=signature->callableResultType?xl6IrCgenType(module,signature->callableResultType):NULL;
        if(signature->callableResultType&&!result)return 0;
        xl6OutputPrintf(out,"static bool xlir_checked_result_t%u(XL6_Closure* closure, void* output)\n{\n",(unsigned)type);
        if(result)xl6OutputPrintf(out,"    return ((bool (*)(void*, %s*))closure->invoke)(closure->environment, (%s*)output);\n",result,result);
        else xl6OutputPuts("    (void)output; return ((bool (*)(void*))closure->invoke)(closure->environment);\n",out);
        xl6OutputPuts("}\n",out);
    }
    for ( i = 0; i < module->callTargetCount; ++i ) {
        if ( module->callTargets[i].reachability != XL6_IR_REACHABILITY_UNREACHABLE &&
             (module->callTargets[i].kind == XL6_IR_CALL_TARGET_EXTERN_C ||
              module->callTargets[i].kind == XL6_IR_CALL_TARGET_NATIVE_FACTORY) &&
             !xl6IrCgenEmitExternPrototype(out, module, &module->callTargets[i], diags) ) return 0;
    }
    if ( module->callTargetCount != 0 ) xl6OutputPuts("\n", out);
    if ( needs->needsNativeHandleTakeRuntime ) {
        if (needs->needsNativeHandlePolicyRuntime)
            xl6OutputPuts("#include <__xl_runtime/xl6_native_handle.h>\n", out);
        for ( i = 0; i < module->instructionCount; ++i ) {
            const XL6_IrInstruction* instruction = &module->instructions[i];
            const XL6_IrCallTarget* destroy;
            size_t prior;
            int emitted = 0;
            if ( !xl6IrCgenInstructionIsReachable(module, i) ||
                 instruction->kind != XL6_IR_INSTR_RUNTIME_CALL ||
                 instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_NATIVE_HANDLE_TAKE ) continue;
            for ( prior = 0; prior < i; ++prior ) {
                const XL6_IrInstruction* candidate = &module->instructions[prior];
                if ( xl6IrCgenInstructionIsReachable(module, prior) &&
                     candidate->kind == XL6_IR_INSTR_RUNTIME_CALL &&
                     candidate->runtimeOp == XL6_IR_RUNTIME_XVALUE_NATIVE_HANDLE_TAKE &&
                     candidate->targetSymbol == instruction->targetSymbol ) {
                    emitted = 1;
                    break;
                }
            }
            if ( emitted ) continue;
            destroy = xl6IrCallTargetGet(module, instruction->targetSymbol);
            if ( destroy == NULL || destroy->kind != XL6_IR_CALL_TARGET_EXTERN_C ||
                 destroy->cName == NULL || destroy->cName[0] == '\0' )
                return xl6IrCgenDiag(diags, module, instruction->origin,
                    "XLIR C emitter native handle transfer has no destructor linkage ABI");
            if (destroy->nativeHandlePolicyAbi == XL6_IR_NATIVE_HANDLE_POLICY_RESIDENT_V1) {
                xl6OutputPrintf(out,
                    "extern const XL6_NativeHandlePolicyV1* %s(void);\n"
                    "static xvalue* xlir_native_handle_take_s%u(void** handle)\n"
                    "{ return xl6NativeHandleTakeV1(handle, %s()); }\n\n",
                    destroy->nativeHandlePolicyCName, (unsigned int)instruction->targetSymbol,
                    destroy->nativeHandlePolicyCName);
                continue;
            }
            {
                char callback[80];
                snprintf(callback, sizeof(callback), "xlir_native_handle_drop_s%u", (unsigned int)instruction->targetSymbol);
                if (!xl6IrCgenEmitNativeDestructorCallback(out, destroy, callback))
                    return xl6IrCgenDiag(diags, module, instruction->origin,
                        "XLIR native destructor has no exact cleanup outcome ABI");
            }
            xl6OutputPrintf(out,
                "static const xvaluehandleops xlir_native_handle_ops_s%u = "
                "{ NULL, xlir_native_handle_drop_s%u, NULL, NULL };\n"
                "static xvalue* xlir_native_handle_take_s%u(void** handle)\n"
                "{\n"
                "    void* owned = handle != NULL ? *handle : NULL;\n"
                "    xvalue* value;\n"
                "    if (handle != NULL) *handle = NULL;\n"
                "    value = xrtValueHandleTake(handle != NULL ? &owned : NULL, &xlir_native_handle_ops_s%u, NULL);\n"
                "    if (value == NULL && owned != NULL) xlir_native_handle_drop_s%u(owned, NULL);\n"
                "    return value;\n"
                "}\n\n",
                (unsigned int)instruction->targetSymbol,
                (unsigned int)instruction->targetSymbol,
                (unsigned int)instruction->targetSymbol,
                (unsigned int)instruction->targetSymbol, (unsigned int)instruction->targetSymbol);
        }
    }
    return !xl6OutputHasFailed(out);
}
