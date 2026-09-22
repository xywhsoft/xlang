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
                "static int64_t xlir_string_to_time_checked(const char* text)\n"
                "{\n"
                "    int64_t result = 0;\n"
                "    if (text == NULL) text = \"\";\n"
                "    if (xrtTimeParseAny(xrtStrView(text), &result)) return result;\n"
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
                "static char* xlir_time_to_string(int64_t value)\n"
                "{ return xrtTimeFormat(value, 0, xrtStrView(\"%F %T\")); }\n",
                out);
        xl6OutputPuts("\n", out);
    }
    if ( needs->needsBoolToStringRuntime || needs->needsI64ToStringRuntime || needs->needsU64ToStringRuntime ||
         needs->needsF64ToStringRuntime || needs->needsXValueToStringRuntime ) {
        xl6OutputPuts("extern char* xrtStrDupN(const char*, size_t);\n", out);
        if ( needs->needsI64ToStringRuntime || needs->needsXValueToStringRuntime )
            xl6OutputPuts("extern bool xrtIntFormatTo(int64_t, xlir_string_view, char*, size_t, size_t*);\n", out);
        if ( needs->needsU64ToStringRuntime )
            xl6OutputPuts("extern bool xrtUIntFormatTo(uint64_t, xlir_string_view, char*, size_t, size_t*);\n", out);
        if ( needs->needsF64ToStringRuntime || needs->needsXValueToStringRuntime )
            xl6OutputPuts("extern bool xrtNumFormatTo(double, xlir_string_view, char*, size_t, size_t*);\n", out);
        if ( needs->needsBoolToStringRuntime || needs->needsXValueToStringRuntime )
            xl6OutputPuts(
                "static char* xlir_bool_to_string(bool value)\n"
                "{ return xrtStrDup(value ? \"true\" : \"false\"); }\n", out);
        if ( needs->needsI64ToStringRuntime || needs->needsXValueToStringRuntime )
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
        if ( needs->needsF64ToStringRuntime || needs->needsXValueToStringRuntime )
            xl6OutputPuts(
                "static char* xlir_f64_to_string(double value)\n"
                "{ char buffer[128]; size_t size = 0;\n"
                "  if (!xrtNumFormatTo(value, (xlir_string_view){\"\", 0}, buffer, sizeof(buffer), &size)) return xrtStrDup(\"\");\n"
                "  return xrtStrDupN(buffer, size); }\n", out);
        if ( needs->needsXValueToStringRuntime )
            xl6OutputPuts(
                "#ifndef XLIR_XRT_DECLARATIONS\nextern int xrtValueType(const xvalue*);\n#endif\n"
                "extern bool xrtValueGetBool(const xvalue*, bool*);\n"
                "extern bool xrtValueGetInt(const xvalue*, int64_t*);\n"
                "extern bool xrtValueGetUInt(const xvalue*, uint64_t*);\n"
                "extern bool xrtValueGetFloat(const xvalue*, double*);\n"
                "extern bool xrtValueGetString(const xvalue*, xlir_string_view*);\n"
                "extern char* xrtXsonStringify(const xvalue*, bool, size_t*);\n"
                "static char* xlir_xvalue_to_string(const xvalue* value)\n"
                "{\n"
                "    bool boolean = false; int64_t integer = 0; uint64_t unsignedInteger = 0; double number = 0.0;\n"
                "    xlir_string_view text = {0}; size_t size = 0;\n"
                "    switch (value != NULL ? xrtValueType(value) : 0) {\n"
                "    case 0: return xrtStrDup(\"null\");\n"
                "    case 1: return xrtValueGetBool(value, &boolean) ? xlir_bool_to_string(boolean) : xrtStrDup(\"\");\n"
                "    case 2: return xrtValueGetInt(value, &integer) ? xlir_i64_to_string(integer) : xrtStrDup(\"\");\n"
                "    case 13: return xrtValueGetUInt(value, &unsignedInteger) ? xlir_u64_to_string(unsignedInteger) : xrtStrDup(\"\");\n"
                "    case 3: return xrtValueGetFloat(value, &number) ? xlir_f64_to_string(number) : xrtStrDup(\"\");\n"
                "    case 4: return xrtValueGetString(value, &text) ? xrtStrDupN(text.Data, text.Size) : xrtStrDup(\"\");\n"
                "    default: return xrtXsonStringify(value, false, &size);\n"
                "    }\n"
                "}\n", out);
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
        size_t persistenceLayoutIndex;
        xl6OutputPuts(
            "typedef uint64_t (*xlir_value_identity_hash)(const xvalue*, void*);\n"
            "typedef bool (*xlir_value_identity_equal)(const xvalue*, const xvalue*, void*);\n"
            "extern bool xrtValueHash(const xvalue*, uint64_t*);\n"
            "extern bool xrtValueEqual(const xvalue*, const xvalue*);\n"
            "#include \"__xl_runtime/xl6_code_lease.h\"\n",
            out);
        for ( persistenceLayoutIndex = 0;
              persistenceLayoutIndex < module->classLayoutCount;
              ++persistenceLayoutIndex ) {
            const XL6_IrClassLayout* persistenceLayout =
                &module->classLayouts[persistenceLayoutIndex];
            if ( persistenceLayout->persistenceConstructionAbi !=
                     XL6_IR_PERSISTENCE_CONSTRUCTION_SCHEMA_DEFAULT ) continue;
            xl6OutputPrintf(out,
                "static uint64_t xlir_class_reference_hash_t%u(const xvalue* value, void* data)\n"
                "{ (void)data; if (value == NULL) abort(); return (uint64_t)(uintptr_t)value; }\n"
                "static bool xlir_class_reference_equal_t%u(const xvalue* left, const xvalue* right, void* data)\n"
                "{ (void)data; return left == right; }\n"
                "static xvalue* xlir_class_identity_bind_t%u(xvalue* value)\n"
                "{ if (value == NULL || !xl6ClassRuntimeV2Get()->BindIdentity(value, xlir_class_reference_hash_t%u, xlir_class_reference_equal_t%u)) abort(); return value; }\n\n",
                (unsigned int)persistenceLayout->type,
                (unsigned int)persistenceLayout->type,
                (unsigned int)persistenceLayout->type,
                (unsigned int)persistenceLayout->type,
                (unsigned int)persistenceLayout->type);
        }
        for ( instructionIndex = 0;
              instructionIndex < module->instructionCount;
              ++instructionIndex ) {
            const XL6_IrInstruction* instruction =
                &module->instructions[instructionIndex];
            const XL6_IrClassLayout* layout;
            const XL6_IrType* classType;
            size_t priorInstruction;
            size_t fieldOffset;
            int identityOp;
            if ( !xl6IrCgenInstructionIsReachable(module, instructionIndex) ||
                 instruction->kind != XL6_IR_INSTR_RUNTIME_CALL ) continue;
            identityOp = instruction->runtimeOp ==
                XL6_IR_RUNTIME_XVALUE_CLASS_IDENTITY_BIND;
            if ( !identityOp && instruction->runtimeOp !=
                 XL6_IR_RUNTIME_CLASS_DERIVED_EQUAL ) continue;
            for ( priorInstruction = 0;
                  priorInstruction < instructionIndex;
                  ++priorInstruction ) {
                const XL6_IrInstruction* prior =
                    &module->instructions[priorInstruction];
                if ( xl6IrCgenInstructionIsReachable(
                         module, priorInstruction) &&
                     prior->kind == XL6_IR_INSTR_RUNTIME_CALL &&
                     (prior->runtimeOp ==
                          XL6_IR_RUNTIME_XVALUE_CLASS_IDENTITY_BIND ||
                      prior->runtimeOp ==
                          XL6_IR_RUNTIME_CLASS_DERIVED_EQUAL) &&
                     prior->payloadType == instruction->payloadType )
                    break;
            }
            if ( priorInstruction != instructionIndex ) continue;
            if ( identityOp
                     ? !xl6IrCgenRuntimeClassIdentityBindAbi(
                           module, instruction)
                     : !xl6IrCgenRuntimeClassDerivedEqualAbi(
                           module, instruction) )
                return xl6IrCgenDiag(diags, module, instruction->origin,
                    "XLIR C emitter class value identity helper ABI is unresolved");
            layout = xl6IrCgenClassLayout(
                module, instruction->payloadType);
            classType = xl6IrTypeGet(
                &module->types, instruction->payloadType);
            if ( layout == NULL || classType == NULL ||
                 classType->typeValueId == 0 )
                return xl6IrCgenDiag(diags, module, instruction->origin,
                    "XLIR C emitter class value identity layout is unresolved");
            if ( layout->persistenceConstructionAbi ==
                     XL6_IR_PERSISTENCE_CONSTRUCTION_SCHEMA_DEFAULT )
                continue;
            if ( layout->hashAbi ==
                     XL6_IR_CLASS_HASH_ABI_RUNTIME_REFERENCE &&
                 layout->equalityAbi ==
                     XL6_IR_CLASS_EQUALITY_ABI_RUNTIME_REFERENCE ) {
                xl6OutputPrintf(out,
                    "static uint64_t xlir_class_reference_hash_t%u(const xvalue* value, void* data)\n"
                    "{ (void)data; if (value == NULL) abort(); return (uint64_t)(uintptr_t)value; }\n"
                    "static bool xlir_class_reference_equal_t%u(const xvalue* left, const xvalue* right, void* data)\n"
                    "{ (void)data; return left == right; }\n"
                    "static xvalue* xlir_class_identity_bind_t%u(xvalue* value)\n"
                    "{ if (value == NULL || !xl6ClassRuntimeV2Get()->BindIdentity(value, xlir_class_reference_hash_t%u, xlir_class_reference_equal_t%u)) abort(); return value; }\n\n",
                    (unsigned int)instruction->payloadType,
                    (unsigned int)instruction->payloadType,
                    (unsigned int)instruction->payloadType,
                    (unsigned int)instruction->payloadType,
                    (unsigned int)instruction->payloadType);
                continue;
            }
            xl6OutputPrintf(out,
                "static bool xlir_class_derive_equal_t%u(const xvalue* left, const xvalue* right)\n"
                "{\n"
                "    xvalue* left_item; xvalue* right_item;\n"
                "    if (left == right) return true;\n"
                "    if (left == NULL || right == NULL || xrtValueType(left) == 0 || xrtValueType(right) == 0) return false;\n"
                "    if (xlir_xvalue_object_get_literal_i64(left, xrtStrView(\"$xlir.class.runtime-type\")) != INT64_C(%u) ||\n"
                "        xlir_xvalue_object_get_literal_i64(right, xrtStrView(\"$xlir.class.runtime-type\")) != INT64_C(%u)) abort();\n",
                (unsigned int)instruction->payloadType,
                (unsigned int)layout->runtimeTypeTag,
                (unsigned int)layout->runtimeTypeTag);
            for ( fieldOffset = layout->inheritedFieldCount;
                  fieldOffset < layout->fieldCount;
                  ++fieldOffset ) {
                const XL6_IrClassField* field = &module->classFields[
                    layout->firstField + fieldOffset];
                if ( !field->compareDeriveIncluded ) continue;
                xl6OutputPuts(
                    "    left_item = xrtValueObjectGet(left, xrtStrView(", out);
                xl6IrCgenEmitCStringLiteral(out,
                    field->name != NULL ? field->name : "");
                xl6OutputPuts(
                    ")); right_item = xrtValueObjectGet(right, xrtStrView(", out);
                xl6IrCgenEmitCStringLiteral(out,
                    field->name != NULL ? field->name : "");
                xl6OutputPuts(
                    "));\n"
                    "    if (left_item == NULL || right_item == NULL) abort();\n"
                    "    if (!xrtValueEqual(left_item, right_item)) return false;\n",
                    out);
            }
            xl6OutputPuts("    return true;\n}\n", out);
            if ( layout->hashAbi ==
                     XL6_IR_CLASS_HASH_ABI_RUNTIME_DERIVED &&
                 layout->hashTarget == 0 &&
                 layout->equalityAbi ==
                     XL6_IR_CLASS_EQUALITY_ABI_RUNTIME_DERIVED &&
                 layout->equalityTarget == 0 ) {
            xl6OutputPrintf(out,
                "static uint64_t xlir_class_derive_hash_t%u(const xvalue* value, void* data)\n"
                "{\n"
                "    uint64_t hash = 0x%016llXULL ^ 0x9E3779B97F4A7C15ULL; uint64_t item_hash; xvalue* item;\n"
                "    (void)data;\n"
                "    if (value == NULL || xrtValueType(value) == 0) abort();\n"
                "    if (xlir_xvalue_object_get_literal_i64(value, xrtStrView(\"$xlir.class.runtime-type\")) != INT64_C(%u)) abort();\n",
                (unsigned int)instruction->payloadType,
                (unsigned long long)classType->typeValueId,
                (unsigned int)layout->runtimeTypeTag);
            for ( fieldOffset = layout->inheritedFieldCount;
                  fieldOffset < layout->fieldCount;
                  ++fieldOffset ) {
                const XL6_IrClassField* field = &module->classFields[
                    layout->firstField + fieldOffset];
                if ( !field->compareDeriveIncluded ) continue;
                xl6OutputPuts(
                    "    item = xrtValueObjectGet(value, xrtStrView(", out);
                xl6IrCgenEmitCStringLiteral(out,
                    field->name != NULL ? field->name : "");
                xl6OutputPuts(
                    ")); if (item == NULL || !xrtValueHash(item, &item_hash)) abort();\n"
                    "    hash ^= item_hash + 0x9E3779B97F4A7C15ULL + (hash << 6) + (hash >> 2);\n",
                    out);
            }
            xl6OutputPuts("    return hash;\n}\n", out);
            xl6OutputPrintf(out,
                "static bool xlir_class_derive_equal_callback_t%u(const xvalue* left, const xvalue* right, void* data)\n"
                "{ (void)data; return xlir_class_derive_equal_t%u(left, right); }\n"
                "static xvalue* xlir_class_identity_bind_t%u(xvalue* value)\n"
                "{ if (value == NULL || !xl6ClassRuntimeV2Get()->BindIdentity(value, xlir_class_derive_hash_t%u, xlir_class_derive_equal_callback_t%u)) abort(); return value; }\n\n",
                (unsigned int)instruction->payloadType,
                (unsigned int)instruction->payloadType,
                (unsigned int)instruction->payloadType,
                (unsigned int)instruction->payloadType,
                (unsigned int)instruction->payloadType);
            }
        }
    }
    if ( needs->needsClassDerivedToStringRuntime ) {
        size_t instructionIndex;
        xl6OutputPuts(
            "static char* xlir_string_append_take(char* left, const char* right)\n"
            "{\n"
            "    char* joined;\n"
            "    if (left == NULL || right == NULL) { xrtFree(left); return NULL; }\n"
            "    joined = xrtStrConcat(xrtStrView(left), xrtStrView(right));\n"
            "    xrtFree(left);\n"
            "    return joined;\n"
            "}\n\n",
            out);
        for ( instructionIndex = 0;
              instructionIndex < module->instructionCount;
              ++instructionIndex ) {
            const XL6_IrInstruction* instruction =
                &module->instructions[instructionIndex];
            const XL6_IrClassLayout* layout;
            const XL6_IrType* classType;
            size_t priorInstruction;
            size_t fieldOffset;
            int wroteField = 0;
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
                 classType->name == NULL )
                return xl6IrCgenDiag(diags, module, instruction->origin,
                    "XLIR C emitter derived class string layout name is unresolved");
            xl6OutputPrintf(out,
                "static char* xlir_class_derive_to_string_t%u(const xvalue* value)\n"
                "{\n"
                "    char* result; char* text; xvalue* item;\n"
                "    if (value == NULL || xrtValueType(value) == 0) return xrtStrDup(\"null\");\n"
                "    if (xlir_xvalue_object_get_literal_i64(value, xrtStrView(\"$xlir.class.runtime-type\")) != INT64_C(%u)) abort();\n"
                "    result = xrtStrDup(",
                (unsigned int)instruction->payloadType,
                (unsigned int)layout->runtimeTypeTag);
            xl6IrCgenEmitCStringLiteral(out, classType->name);
            xl6OutputPuts("); if (result == NULL) return NULL;\n", out);
            xl6OutputPuts(
                "    result = xlir_string_append_take(result, \"{\");\n"
                "    if (result == NULL) return NULL;\n",
                out);
            for ( fieldOffset = layout->inheritedFieldCount;
                  fieldOffset < layout->fieldCount;
                  ++fieldOffset ) {
                const XL6_IrClassField* field =
                    &module->classFields[
                        layout->firstField + fieldOffset];
                if ( !field->stringDeriveIncluded ) continue;
                xl6OutputPuts(
                    "    result = xlir_string_append_take(result, ", out);
                xl6IrCgenEmitCStringLiteral(
                    out, wroteField ? ", " : "");
                xl6OutputPuts("); if (result == NULL) return NULL;\n", out);
                xl6OutputPuts(
                    "    result = xlir_string_append_take(result, ", out);
                xl6IrCgenEmitCStringLiteral(
                    out, field->name != NULL ? field->name : "");
                xl6OutputPuts("); if (result == NULL) return NULL;\n", out);
                xl6OutputPuts(
                    "    result = xlir_string_append_take(result, \"=\");\n"
                    "    if (result == NULL) return NULL;\n"
                    "    item = xrtValueObjectGet(value, xrtStrView(",
                    out);
                xl6IrCgenEmitCStringLiteral(
                    out, field->name != NULL ? field->name : "");
                xl6OutputPuts(
                    "));\n"
                    "    text = xlir_xvalue_to_string(item != NULL ? item : xrtValueNull());\n"
                    "    result = xlir_string_append_take(result, text);\n"
                    "    xrtFree(text);\n"
                    "    if (result == NULL) return NULL;\n",
                    out);
                wroteField = 1;
            }
            xl6OutputPuts(
                "    return xlir_string_append_take(result, \"}\");\n"
                "}\n\n",
                out);
        }
    }
    if ( needs->needsStringContainsRuntime ) {
        xl6OutputPuts(
            "extern bool xrtStrContains(xlir_string_view, xlir_string_view);\n"
            "extern bool xrtStrCaseContains(xlir_string_view, xlir_string_view);\n"
            "bool xl6_xrt_string_contains(const char* text, const char* part, bool ignoreCase)\n"
            "{\n"
            "    xlir_string_view textView = xrtStrView(text);\n"
            "    xlir_string_view partView = xrtStrView(part);\n"
            "    return ignoreCase ? xrtStrCaseContains(textView, partView) : xrtStrContains(textView, partView);\n"
            "}\n\n", out);
    }
    if ( needs->needsXValueTableRemoveBridgeRuntime ) {
        xl6OutputPuts(
            "extern bool xrtValueObjectRemove(xvalue*, xlir_string_view);\n"
            "bool xl6_xrt_std_table_remove(xvalue* table, const char* key, unsigned int keyLen)\n"
            "{\n"
            "    xlir_string_view view = xrtStrView(key != NULL ? key : \"\");\n"
            "    if (keyLen != 0u) view.Size = (size_t)keyLen;\n"
            "    return xrtValueObjectRemove(table, view);\n"
            "}\n\n", out);
    }
    if ( needs->needsStringIndexOfBridgeRuntime ) {
        xl6OutputPuts(
            "extern size_t xrtUtf8Find(xlir_string_view, xlir_string_view, size_t);\n"
            "extern size_t xrtUtf8CaseFind(xlir_string_view, xlir_string_view, size_t);\n"
            "int64_t xl6_xrt_string_index_of(const char* text, const char* part, bool ignoreCase)\n"
            "{\n"
            "    size_t index = ignoreCase\n"
            "        ? xrtUtf8CaseFind(xrtStrView(text), xrtStrView(part), 0u)\n"
            "        : xrtUtf8Find(xrtStrView(text), xrtStrView(part), 0u);\n"
            "    return index == SIZE_MAX ? -1 : (int64_t)index;\n"
            "}\n\n", out);
    }
    if ( needs->needsStringIntFormatBridgeRuntime ) {
        xl6OutputPuts(
            "extern char* xrtIntFormat(int64_t, xlir_string_view);\n"
            "void* xl6_xrt_string_int_format(int64_t value, const char* format)\n"
            "{ return xrtIntFormat(value, xrtStrView(format)); }\n\n",
            out);
    }
    if ( needs->needsStringUIntFormatBridgeRuntime ) {
        xl6OutputPuts(
            "extern char* xrtUIntFormat(uint64_t, xlir_string_view);\n"
            "void* xl6_xrt_string_uint_format(uint64_t value, const char* format)\n"
            "{ return xrtUIntFormat(value, xrtStrView(format)); }\n\n",
            out);
    }
    if ( needs->needsStringNumFormatBridgeRuntime ) {
        xl6OutputPuts(
            "extern char* xrtNumFormat(double, xlir_string_view);\n"
            "void* xl6_xrt_string_num_format(double value, const char* format)\n"
            "{ return xrtNumFormat(value, xrtStrView(format)); }\n\n",
            out);
    }
    if ( needs->needsTakeCstrBridgeRuntime ) {
        xl6OutputPuts(
            "char* xl6_xrt_take_cstr(void* value)\n"
            "{\n"
            "    char* text = xrtStrDup(value != NULL ? (const char*)value : \"\");\n"
            "    if (value != NULL) xrtFree(value);\n"
            "    return text;\n"
            "}\n\n",
            out);
    }
    if ( needs->needsDynamicFieldsBridgeRuntime ) {
        xl6OutputPuts(
            "static void* xl6_xrt_dynamic_fields_create(void)\n"
            "{ return xrtDynamicFieldsCreate(); }\n"
            "static void xl6_xrt_dynamic_fields_destroy(void* handle)\n"
            "{ xrtDynamicFieldsUnref((xrtdynamicfields*)handle); }\n"
            "static xvalue* xl6_xrt_dynamic_fields_get(void* handle, const char* name)\n"
            "{\n"
            "    xvalue* value = xrtDynamicFieldsGetRef((const xrtdynamicfields*)handle, xrtStrView(name));\n"
            "    return value != NULL ? value : xrtValueNull();\n"
            "}\n"
            "static bool xl6_xrt_dynamic_fields_set(void* handle, const char* name, xvalue* value)\n"
            "{ return xrtDynamicFieldsSetRef((xrtdynamicfields*)handle, xrtStrView(name), value); }\n\n",
            out);
    }
    if ( needs->needsPrintRuntime ) {
        /* The xvalue tag ABI is public and stable, but generated C must not
         * repeat its numeric layout as magic literals.  Snapshot the values
         * from the XRT headers used to build this compiler and give every
         * emitted branch a named ABI constant. */
        xl6OutputPrintf(out,
            "enum xlir_xvalue_tag_abi { XLIR_XVALUE_NULL = %d, XLIR_XVALUE_BOOL = %d, "
            "XLIR_XVALUE_INT = %d, XLIR_XVALUE_FLOAT = %d, XLIR_XVALUE_STRING = %d, "
            "XLIR_XVALUE_POINTER = %d, XLIR_XVALUE_ARRAY = %d, XLIR_XVALUE_UINT = %d };\n",
            (int)XVALUE_NULL, (int)XVALUE_BOOL, (int)XVALUE_INT,
            (int)XVALUE_FLOAT, (int)XVALUE_STRING, (int)XVALUE_POINTER,
            (int)XVALUE_ARRAY, (int)XVALUE_UINT);
        xl6OutputPuts(
            "#ifndef XLIR_XRT_DECLARATIONS\nextern int xrtValueType(const xvalue*);\n#endif\n"
            "extern bool xrtValueGetBool(const xvalue*, bool*);\n"
            "extern bool xrtValueGetInt(const xvalue*, int64_t*);\n"
            "extern bool xrtValueGetUInt(const xvalue*, uint64_t*);\n"
            "extern bool xrtValueGetFloat(const xvalue*, double*);\n"
            "extern bool xrtValueGetString(const xvalue*, xlir_string_view*);\n"
            "extern bool xrtValueGetPointer(const xvalue*, void**);\n"
            "extern xvalue* xrtValueArrayAt(const xvalue*, int64_t);\n"
            "extern bool xrtIntFormatTo(int64_t, xlir_string_view, char*, size_t, size_t*);\n"
            "extern bool xrtUIntFormatTo(uint64_t, xlir_string_view, char*, size_t, size_t*);\n"
            "extern bool xrtNumFormatTo(double, xlir_string_view, char*, size_t, size_t*);\n"
            "extern char* xrtXsonStringify(const xvalue*, bool, size_t*);\n"
            "static bool xlir_print_append(xstrbuf* output, const char* text, size_t size)\n"
            "{ return size == 0 || (text != NULL && xrtStrBufAppend(output, xrtStrViewN(text, size))); }\n"
            "static bool xlir_print_one(xstrbuf* output, const xvalue* value)\n"
            "{\n"
            "    char buffer[128]; size_t size = 0; xlir_string_view text = {0}; bool boolean = false;\n"
            "    int64_t integer = 0; uint64_t unsignedInteger = 0; double number = 0.0; void* pointer = NULL;\n"
            "    char* serialized = NULL; bool appended = false; int written = 0;\n"
            "    switch (value != NULL ? xrtValueType(value) : XLIR_XVALUE_NULL) {\n"
            "    case XLIR_XVALUE_NULL: return xlir_print_append(output, \"null\", 4);\n"
            "    case XLIR_XVALUE_BOOL:\n"
            "        return xrtValueGetBool(value, &boolean) &&\n"
            "            xlir_print_append(output, boolean ? \"true\" : \"false\", boolean ? 4 : 5);\n"
            "    case XLIR_XVALUE_INT:\n"
            "        return xrtValueGetInt(value, &integer) &&\n"
            "            xrtIntFormatTo(integer, (xlir_string_view){\"\", 0}, buffer, sizeof(buffer), &size) &&\n"
            "            xlir_print_append(output, buffer, size);\n"
            "    case XLIR_XVALUE_UINT:\n"
            "        return xrtValueGetUInt(value, &unsignedInteger) &&\n"
            "            xrtUIntFormatTo(unsignedInteger, (xlir_string_view){\"\", 0}, buffer, sizeof(buffer), &size) &&\n"
            "            xlir_print_append(output, buffer, size);\n"
            "    case XLIR_XVALUE_FLOAT:\n"
            "        return xrtValueGetFloat(value, &number) &&\n"
            "            xrtNumFormatTo(number, (xlir_string_view){\"\", 0}, buffer, sizeof(buffer), &size) &&\n"
            "            xlir_print_append(output, buffer, size);\n"
            "    case XLIR_XVALUE_STRING:\n"
            "        return xrtValueGetString(value, &text) && xlir_print_append(output, text.Data, text.Size);\n"
            "    case XLIR_XVALUE_POINTER:\n"
            "        if (!xrtValueGetPointer(value, &pointer)) return false;\n"
            "        written = snprintf(buffer, sizeof(buffer), \"%p\", pointer);\n"
            "        return written >= 0 && (size_t)written < sizeof(buffer) &&\n"
            "            xlir_print_append(output, buffer, (size_t)written);\n"
            "    default:\n"
            "        serialized = xrtXsonStringify(value, false, &size);\n"
            "        if (serialized == NULL) return false;\n"
            "        appended = xlir_print_append(output, serialized, size);\n"
            "        xrtFree(serialized);\n"
            "        return appended;\n"
            "    }\n"
            "}\n"
            "void xl6_xrt_print(xvalue* values, bool newline)\n"
            "{\n"
            "    xstrbuf output; size_t count = 0; size_t i = 0; bool ready = true;\n"
            "    xrtStrBufInit(&output);\n"
            "    (void)xrtConsoleFlush(XCONSOLE_STDOUT);\n"
            "    if (values != NULL && (int)xrtValueType(values) == (int)XLIR_XVALUE_ARRAY) {\n"
            "        count = xrtValueCount(values);\n"
            "        for (i = 0; ready && i < count; ++i) {\n"
            "            if (i != 0) ready = xlir_print_append(&output, \" \", 1);\n"
            "            if (ready) ready = xlir_print_one(&output, xrtValueArrayAt(values, (int64_t)i));\n"
            "        }\n"
            "    } else ready = xlir_print_one(&output, values);\n"
            "    if (ready) {\n"
            "        if (newline) (void)xrtConsoleWriteLine(XCONSOLE_STDOUT, xrtStrBufView(&output));\n"
            "        else (void)xrtConsoleWrite(XCONSOLE_STDOUT, xrtStrBufView(&output));\n"
            "    }\n"
            "    xrtStrBufFree(&output);\n"
            "    (void)xrtConsoleFlush(XCONSOLE_STDOUT);\n"
            "}\n\n", out);
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
    for ( i = 0; i < module->callTargetCount; ++i ) {
        if ( module->callTargets[i].reachability != XL6_IR_REACHABILITY_UNREACHABLE &&
             (module->callTargets[i].kind == XL6_IR_CALL_TARGET_EXTERN_C ||
              module->callTargets[i].kind == XL6_IR_CALL_TARGET_NATIVE_FACTORY) &&
             !xl6IrCgenEmitExternPrototype(out, module, &module->callTargets[i], diags) ) return 0;
    }
    if ( module->callTargetCount != 0 ) xl6OutputPuts("\n", out);
    if ( needs->needsNativeHandleTakeRuntime ) {
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
