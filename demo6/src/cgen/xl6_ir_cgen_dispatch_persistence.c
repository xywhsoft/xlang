static int xl6IrCgenClassDispatchSlotIsReachable(
    const XL6_IrModule* module, size_t slotIndex)
{
    size_t i;
    int hasImplementation = 0;
    for ( i = 0; i < module->classDispatchImplCount; ++i ) {
        const XL6_IrClassDispatchImpl* impl = &module->classDispatchImpls[i];
        const XL6_IrCallTarget* target;
        if ( impl->slot != slotIndex ) continue;
        hasImplementation = 1;
        target = xl6IrCallTargetGet(module, impl->implementationSymbol);
        if ( target == NULL || target->reachability == XL6_IR_REACHABILITY_UNREACHABLE ) return 0;
    }
    return hasImplementation;
}

static int xl6IrCgenProtocolDispatchSlotIsReachable(
    const XL6_IrModule* module, size_t slotIndex)
{
    size_t i;
    int hasImplementation = 0;
    for ( i = 0; i < module->protocolDispatchImplCount; ++i ) {
        const XL6_IrProtocolDispatchImpl* impl = &module->protocolDispatchImpls[i];
        const XL6_IrCallTarget* target;
        if ( impl->slot != slotIndex ) continue;
        hasImplementation = 1;
        target = xl6IrCallTargetGet(module, impl->implementationSymbol);
        if ( target == NULL || target->reachability == XL6_IR_REACHABILITY_UNREACHABLE ) return 0;
    }
    return hasImplementation;
}
/* Both table domains expose the same verified physical call signature. */
static int xl6IrCgenEmitDispatcherSignature(
    XL6_Output* out, const XL6_IrModule* module, const char* prefix,
    size_t slotIndex, XL6_IrNodeId declaration, XL6_IrTypeId resultType,
    XL6_IrErrorAbi errorAbi, size_t parameterCount,
    const XL6_IrTypeId* parameterTypes, const XL6_IrParameterAbi* parameterAbis,
    XL6_DiagList* diags)
{
    const char* result = errorAbi == XL6_IR_ERROR_ABI_MAY_RAISE
        ? xl6IrCgenErrorResultType(module, resultType)
        : resultType != XL6_IR_TYPE_INVALID ? xl6IrCgenType(module, resultType) : "void";
    size_t p;
    if (result == NULL || (parameterCount != 0 && (parameterTypes == NULL || parameterAbis == NULL)))
        return xl6IrCgenDiag(diags, module, declaration, "XLIR dispatch signature ABI is unresolved");
    xl6OutputPrintf(out, "static %s xlir_%sdispatch_%lu(xvalue* receiver", result, prefix, (unsigned long)(slotIndex + 1));
    for (p = 0; p < parameterCount; ++p) {
        const char* type = xl6IrCgenType(module, parameterTypes[p]);
        if (type == NULL || parameterAbis[p] == XL6_IR_PARAMETER_ABI_INVALID)
            return xl6IrCgenDiag(diags, module, declaration, "XLIR dispatch parameter ABI is unresolved");
        xl6OutputPrintf(out, ", %s%s a%lu", type,
            xl6IrCgenParameterAbiIsIndirect(parameterAbis[p]) ? "*" : "", (unsigned long)p);
    }
    xl6OutputPuts(")", out);
    return !xl6OutputHasFailed(out);
}
static int xl6IrCgenEmitProtocolDispatcherSignature(
    XL6_Output* out, const XL6_IrModule* module,
    const XL6_IrProtocolDispatchSlot* slot, size_t slotIndex, XL6_DiagList* diags)
{
    return xl6IrCgenEmitDispatcherSignature(out, module, "p", slotIndex,
        slot->declaration, slot->returnType, slot->errorAbi, slot->parameterCount,
        slot->parameterTypes, slot->parameterAbis, diags);
}
static int xl6IrCgenEmitProtocolDispatcherPrototypes(
    XL6_Output* out, const XL6_IrModule* module, XL6_DiagList* diags)
{
    size_t slotIndex;
    for ( slotIndex = 0; slotIndex < module->protocolDispatchSlotCount;
          ++slotIndex ) {
        if ( !xl6IrCgenProtocolDispatchSlotIsReachable(module, slotIndex) )
            continue;
        if ( !xl6IrCgenEmitProtocolDispatcherSignature(
                out, module, &module->protocolDispatchSlots[slotIndex],
                slotIndex, diags) ) return 0;
        xl6OutputPuts(";\n", out);
    }
    return !xl6OutputHasFailed(out);
}

/* Class dispatch is emitted exclusively from the verified XLIR slot/row
 * tables.  The generated C neither inspects source declarations nor performs
 * a name/ancestry fallback when a runtime tag has no row. */
static int xl6IrCgenEmitVirtualDispatcherSignature(
    XL6_Output* out, const XL6_IrModule* module, size_t slotIndex, XL6_DiagList* diags)
{
    const XL6_IrClassDispatchSlot* slot = &module->classDispatchSlots[slotIndex];
    return xl6IrCgenEmitDispatcherSignature(out, module, "v", slotIndex,
        slot->declaration, slot->returnType, slot->errorAbi, slot->parameterCount,
        slot->parameterTypes, slot->parameterAbis, diags);
}

static int xl6IrCgenEmitVirtualDispatcherPrototypes(
    XL6_Output* out, const XL6_IrModule* module, XL6_DiagList* diags)
{
    size_t slot;
    for (slot = 0; slot < module->classDispatchSlotCount; ++slot) {
        if (!xl6IrCgenClassDispatchSlotIsReachable(module, slot)) continue;
        if (!xl6IrCgenEmitVirtualDispatcherSignature(out, module, slot, diags)) return 0;
        xl6OutputPuts(";\n", out);
    }
    return !xl6OutputHasFailed(out);
}
static void xl6IrCgenEmitCStringLiteral(XL6_Output* out, const char* text);
static void xl6IrCgenEmitPersistenceByteView(XL6_Output*, const char*, int);

/* Emit the target's physical call. Receiver projection and result conversion
 * have already been selected in CallTarget; no declaration lookup is needed. */
static void xl6IrCgenEmitDispatchTargetCall(
    XL6_Output* out, const XL6_IrCallTarget* target,
    const XL6_IrClassField* externReceiverField, size_t parameterCount)
{
    size_t p;
    if (target->resultConversionAbi == XL6_IR_CALL_RESULT_CONVERSION_BORROWED_STRING_COPY)
        xl6OutputPuts("xlir_string_from_cstr(", out);
    if (target->kind == XL6_IR_CALL_TARGET_INTERNAL_ASYNC)
        xl6OutputPrintf(out, "xlir_async_start_%u(receiver", (unsigned int)target->symbol);
    else if (target->kind == XL6_IR_CALL_TARGET_INTERNAL_GENERATOR)
        xl6OutputPrintf(out, "xlir_generator_start_%u(receiver", (unsigned int)target->symbol);
    else if (target->kind == XL6_IR_CALL_TARGET_EXTERN_C) {
        xl6OutputPrintf(out, "%s(%s(receiver, xlir_literal_view(", target->cName,
            externReceiverField->nativeDestroySymbol != 0
                ? "xlir_xvalue_object_get_literal_native_handle"
                : "xlir_xvalue_object_get_literal_raw_pointer");
        xl6IrCgenEmitCStringLiteral(out, externReceiverField->name);
        xl6OutputPuts("))", out);
    } else xl6OutputPrintf(out, "xlir_fn_%u(receiver", (unsigned int)target->symbol);
    for (p = 0; p < parameterCount; ++p)
        xl6OutputPrintf(out, ", a%lu", (unsigned long)p);
    xl6OutputPuts(")", out);
    if (target->resultConversionAbi == XL6_IR_CALL_RESULT_CONVERSION_BORROWED_STRING_COPY)
        xl6OutputPuts(")", out);
}

/* A dispatch boundary joins effects, not implementation bodies. Non-raising
 * direct calls keep their original ABI; asynchronous launch failure remains
 * distinct from a Future's later rejection. */
static int xl6IrCgenEmitDispatchWitness(
    XL6_Output* out, const XL6_IrModule* module, XL6_IrTypeId returnType,
    XL6_IrErrorAbi errorAbi, const XL6_IrCallTarget* target,
    const XL6_IrClassField* externReceiverField, size_t parameterCount,
    XL6_DiagList* diags)
{
    int carrier = errorAbi == XL6_IR_ERROR_ABI_MAY_RAISE;
    int lift = carrier && target->errorPhysicalAbi == XL6_IR_CALL_ERROR_PHYSICAL_NONE;
    if (target->errorPhysicalAbi == XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE) {
        if (!carrier || externReceiverField == NULL)
            return xl6IrCgenDiag(diags, module, target->declaration,
                "XLIR foreign witness has no joined outcome or native receiver ABI");
        xl6OutputPrintf(out, "%s result = {false};\n", xl6IrCgenErrorResultType(module, returnType));
        xl6OutputPuts("    xl6_foreign_error_scope scope = {0}; xl6_foreign_error_result outcome;\n"
            "    if (!xl6ForeignCallBegin(&scope)) {\n", out);
        if (!xl6IrCgenEmitPreparationRollback(out, module, target, NULL, 1, "outcome", diags)) return 0;
        xl6OutputPuts("        xrtSetErrorTake(outcome.error); return result;\n"
            "    }\n    ", out);
        if (returnType != XL6_IR_TYPE_INVALID) xl6OutputPuts("result.value = ", out);
        xl6IrCgenEmitDispatchTargetCall(out, target, externReceiverField, parameterCount);
        xl6OutputPuts(";\n", out);
        if (returnType != XL6_IR_TYPE_INVALID) {
            xl6OutputPuts("    if (scope.failed || xrtGetError() != NULL) {\n"
                "        xl6ErrorScopeCaptureFailure(&scope);\n", out);
            if (!xl6IrCgenEmitOwnedPlaceRelease(out, module, returnType, "result.value", diags)) return 0;
            xl6OutputPuts("    }\n", out);
        }
        xl6OutputPuts("    outcome = xl6ForeignCallEnd(&scope); result.ok = outcome.ok;\n"
            "    if (!outcome.ok) xrtSetErrorTake(outcome.error);\n"
            "    return result; ", out);
        return !xl6OutputHasFailed(out);
    }
    if (target->errorPhysicalAbi != XL6_IR_CALL_ERROR_PHYSICAL_NONE &&
        target->errorPhysicalAbi != XL6_IR_CALL_ERROR_PHYSICAL_RESULT_CARRIER)
        return xl6IrCgenDiag(diags, module, target->declaration,
            "XLIR dispatch witness outcome adapter is unresolved");
    if (lift) {
        xl6OutputPrintf(out, "%s result = {true}; ", xl6IrCgenErrorResultType(module, returnType));
        if (returnType != XL6_IR_TYPE_INVALID) xl6OutputPuts("result.value = ", out);
    } else if (returnType != XL6_IR_TYPE_INVALID || carrier) xl6OutputPuts("return ", out);
    xl6IrCgenEmitDispatchTargetCall(out, target, externReceiverField, parameterCount);
    xl6OutputPuts("; ", out);
    if (lift) xl6OutputPuts("return result; ", out);
    else if (returnType == XL6_IR_TYPE_INVALID && !carrier) xl6OutputPuts("return; ", out);
    return !xl6OutputHasFailed(out);
}

static int xl6IrCgenEmitVirtualDispatchers(
    XL6_Output* out, const XL6_IrModule* module, XL6_DiagList* diags)
{
    size_t slotIndex;
    for (slotIndex = 0; slotIndex < module->classDispatchSlotCount; ++slotIndex) {
        const XL6_IrClassDispatchSlot* slot = &module->classDispatchSlots[slotIndex];
        size_t parameter, implementation;
        if (!xl6IrCgenClassDispatchSlotIsReachable(module, slotIndex)) continue;
        if (!xl6IrCgenEmitVirtualDispatcherSignature(out, module, slotIndex, diags)) return 0;
        xl6OutputPuts("\n{\n    switch (xlir_xvalue_object_get_literal_i64(receiver, xrtStrView(\"$xlir.class.runtime-type\"))) {\n", out);
        for (implementation = 0; implementation < module->classDispatchImplCount; ++implementation) {
            const XL6_IrClassDispatchImpl* impl = &module->classDispatchImpls[implementation];
            const XL6_IrClassLayout* layout = NULL;
            const XL6_IrCallTarget* target;
            if (impl->slot != slotIndex) continue;
            target = xl6IrCallTargetGet(module, impl->implementationSymbol);
            for (parameter = 0; parameter < module->classLayoutCount; ++parameter)
                if (module->classLayouts[parameter].type == impl->ownerType) {
                    layout = &module->classLayouts[parameter]; break;
                }
            if (layout == NULL || layout->runtimeTypeTag == 0 || target == NULL)
                return xl6IrCgenDiag(diags, module, impl->implementationDeclaration,
                    "XLIR virtual dispatch implementation has no runtime type tag ABI");
            xl6OutputPrintf(out, "    case %u: { ", (unsigned int)layout->runtimeTypeTag);
            if (!xl6IrCgenEmitDispatchWitness(out, module, slot->returnType, slot->errorAbi,
                    target, NULL, slot->parameterCount, diags)) return 0;
            xl6OutputPuts("}\n", out);
        }
        xl6OutputPuts("    default: abort();\n    }\n}\n\n", out);
    }
    return !xl6OutputHasFailed(out);
}

/* Protocol dispatch consumes the same runtime object tag as class dispatch,
 * but its rows are independently verified protocol witnesses. */

static int xl6IrCgenEmitProtocolDispatchers(
    XL6_Output* out, const XL6_IrModule* module, XL6_DiagList* diags)
{
    size_t slotIndex;
    for ( slotIndex = 0; slotIndex < module->protocolDispatchSlotCount; ++slotIndex ) {
        const XL6_IrProtocolDispatchSlot* slot = &module->protocolDispatchSlots[slotIndex];
        size_t parameter;
        size_t implementation;
        if ( !xl6IrCgenProtocolDispatchSlotIsReachable(module, slotIndex) ) continue;
        if ( !xl6IrCgenEmitProtocolDispatcherSignature(
                out, module, slot, slotIndex, diags) ) return 0;
        xl6OutputPuts("\n{\n    switch (xlir_xvalue_object_get_literal_i64(receiver, xrtStrView(\"$xlir.class.runtime-type\"))) {\n", out);
        for ( implementation = 0; implementation < module->protocolDispatchImplCount; ++implementation ) {
            const XL6_IrProtocolDispatchImpl* impl = &module->protocolDispatchImpls[implementation];
            const XL6_IrClassLayout* layout = NULL;
            const XL6_IrCallTarget* target;
            const XL6_IrClassField* externReceiverField = NULL;
            if ( impl->slot != slotIndex ) continue;
            for ( parameter = 0; parameter < module->classLayoutCount; ++parameter )
                if ( module->classLayouts[parameter].type == impl->ownerType ) {
                    layout = &module->classLayouts[parameter]; break;
                }
            target = xl6IrCallTargetGet(module, impl->implementationSymbol);
            if ( layout == NULL || layout->runtimeTypeTag == 0 || target == NULL ||
                  (target->kind != XL6_IR_CALL_TARGET_INTERNAL &&
                   target->kind != XL6_IR_CALL_TARGET_INTERNAL_ASYNC &&
                   target->kind != XL6_IR_CALL_TARGET_INTERNAL_GENERATOR &&
                   target->kind != XL6_IR_CALL_TARGET_EXTERN_C) ||
                 target->returnType != slot->returnType )
                return xl6IrCgenDiag(diags, module, impl->implementationDeclaration,
                                     "XLIR protocol witness has no runtime type tag/callable ABI");
            if ( target->kind == XL6_IR_CALL_TARGET_EXTERN_C &&
                 (externReceiverField = xl6IrCgenProtocolExternReceiverField(
                      module, slot, target)) == NULL )
                return xl6IrCgenDiag(diags, module, impl->implementationDeclaration,
                    "XLIR protocol extern witness has no exact native receiver adapter ABI");
            xl6OutputPrintf(out, "    case %u: { ", (unsigned int)layout->runtimeTypeTag);
            if (!xl6IrCgenEmitDispatchWitness(out, module, slot->returnType, slot->errorAbi,
                    target, externReceiverField, slot->parameterCount, diags)) return 0;
            xl6OutputPuts("}\n", out);
        }
        xl6OutputPuts("    default: abort();\n    }\n", out);
        xl6OutputPuts("}\n\n", out);
    }
    return !xl6OutputHasFailed(out);
}
static void xl6IrCgenEmitCStringLiteral(XL6_Output* out, const char* text)
{
    const unsigned char* cursor = (const unsigned char*)(text != NULL ? text : "");
    xl6OutputPuts("\"", out);
    while ( *cursor != 0 ) {
        char unit[2];
        switch ( *cursor ) {
        case '\\': xl6OutputPuts("\\\\", out); break;
        case '"': xl6OutputPuts("\\\"", out); break;
        case '\n': xl6OutputPuts("\\n", out); break;
        case '\r': xl6OutputPuts("\\r", out); break;
        case '\t': xl6OutputPuts("\\t", out); break;
        default:
            if ( *cursor >= 0x20 && *cursor <= 0x7e ) {
                unit[0] = (char)*cursor;
                unit[1] = '\0';
                xl6OutputPuts(unit, out);
            } else {
                xl6OutputPrintf(out, "\\%03o", (unsigned int)*cursor);
            }
            break;
        }
        ++cursor;
    }
    xl6OutputPuts("\"", out);
}

static int xl6IrCgenPersistenceTypeReferencesDepth(
    const XL6_IrModule* module, XL6_IrTypeId root,
    XL6_IrTypeId wanted, size_t depth)
{
    const XL6_IrType* type = xl6IrTypeGet(&module->types, root);
    if ( root == wanted ) return 1;
    if ( type == NULL || depth > module->types.count ) return 0;
    if ( type->persistenceValueAbi != XL6_IR_PERSISTENCE_VALUE_NULLABLE &&
         type->persistenceValueAbi != XL6_IR_PERSISTENCE_VALUE_ARRAY &&
         type->persistenceValueAbi !=
             XL6_IR_PERSISTENCE_VALUE_LIST_SEQUENCE &&
         type->persistenceValueAbi !=
             XL6_IR_PERSISTENCE_VALUE_SET_SEQUENCE &&
         type->persistenceValueAbi !=
             XL6_IR_PERSISTENCE_VALUE_DICT_OBJECT )
        return 0;
    return xl6IrCgenPersistenceTypeReferencesDepth(
        module, type->persistencePayloadType, wanted, depth + 1);
}


static int xl6IrCgenPersistenceTypeUsed(
    const XL6_IrModule* module, XL6_IrTypeId wanted,
    uint32 format, int restore)
{
    size_t layoutIndex;
    if (!restore) return 0;
    for ( layoutIndex = 0; layoutIndex < module->classLayoutCount;
          ++layoutIndex ) {
        const XL6_IrClassLayout* layout =
            &module->classLayouts[layoutIndex];
        const XL6_IrPersistenceRestoreSchema* schema =
            xl6IrPersistenceRestoreSchemaGet(module, layout->type, format);
        size_t fieldIndex;
        if (schema == NULL) continue;
        for ( fieldIndex = 0; fieldIndex < schema->fieldCount;
              ++fieldIndex ) {
            const XL6_IrPersistenceRestoreField* field =
                &module->persistenceRestoreFields[schema->firstField + fieldIndex];
            if (field->assignAbi == XL6_IR_PERSIST_RESTORE_CLASS_IN_PLACE) continue;
            if ( xl6IrCgenPersistenceTypeReferencesDepth(
                     module, field->payloadType, wanted, 0) ) return 1;
        }
    }
    return 0;
}

static int xl6IrCgenEmitPersistenceSequenceSeed(XL6_Output* out,
    const XL6_IrModule* module, XL6_IrTypeId type, const char* result, XL6_DiagList* diags)
{
    XL6_IrNominalIdentitySetId set = XL6_IR_NOMINAL_IDENTITY_SET_INVALID;
    if (!xl6IrSequenceConstructionRecipe(module, type, XL6_IR_SEQUENCE_ROLE_ARRAY, &set) ||
        xl6IrCgenEmitSequenceCreateDepth(out, module, type, result, set, 0, NULL, 0, true) < 0)
        return xl6IrCgenDiag(diags, module, XL6_IR_NODE_INVALID,
            "XLIR persistence Array restore has no exact resident construction recipe");
    return 1;
}

static int xl6IrCgenEmitPersistenceClassDefault(
    XL6_Output* out, const XL6_IrModule* module,
    const XL6_IrClassLayout* layout, const char* formatName,
    XL6_DiagList* diags)
{
    const XL6_IrFunction* factory;
    if ( out == NULL || module == NULL || layout == NULL ||
         module->abi.classStorageAbi != XL6_IR_CLASS_STORAGE_RESIDENT_GRAPH_V2 ||
         layout->persistenceConstructionAbi !=
             XL6_IR_PERSISTENCE_CONSTRUCTION_SCHEMA_DEFAULT ||
         layout->persistenceDefaultFunction == 0 ||
         layout->persistenceDefaultFunction > module->functionCount )
        return xl6IrCgenDiag(diags, module,
            layout != NULL ? layout->declaration : XL6_IR_NODE_INVALID,
            "XLIR C emitter persistence class default has no lowered factory ABI");
    factory = &module->functions[layout->persistenceDefaultFunction - 1];
    if (factory->kind != XL6_IR_FUNCTION_CLASS_DEFAULT ||
        factory->reachability == XL6_IR_REACHABILITY_UNREACHABLE)
        return xl6IrCgenDiag(diags, module, layout->declaration,
            "XLIR C emitter persistence class default has no reachable factory");
    xl6OutputPrintf(out,
        "static xvalue* xlir_persist_%s_default_t%u(void)\n"
        "{\n"
        "    %s outcome = xlir_fn_%u();\n"
        "    return outcome.ok ? outcome.value : NULL;\n"
        "}\n\n",
        formatName, (unsigned int)layout->type,
        xl6IrCgenErrorResultType(module, layout->type), (unsigned int)factory->symbol);
    return !xl6OutputHasFailed(out);
}

static int xl6IrCgenEmitPersistenceTypeHelpers(
    XL6_Output* out, const XL6_IrModule* module, XL6_IrTypeId typeId,
    uint32 format, const char* formatName, XL6_DiagList* diags)
{
    const XL6_IrType* type = xl6IrTypeGet(&module->types, typeId);
    int restore = xl6IrCgenPersistenceTypeUsed(
        module, typeId, format, 1);
    if ( type == NULL || !restore ) return 1;
    if ( restore ) {
        if ( !xl6IrCgenPersistenceTypeSupportedDepth(
                 module, typeId, format, 0, 1) )
            return xl6IrCgenDiag(diags, module, XL6_IR_NODE_INVALID,
                "XLIR C emitter persistence restore has no exact construction ABI");
        xl6OutputPrintf(out,
            "static xvalue* xlir_persist_%s_restore_t%u(const xvalue* source)\n"
            "{\n",
            formatName, (unsigned int)typeId);
        if ( type->persistenceValueAbi ==
                 XL6_IR_PERSISTENCE_VALUE_XRT_CLONE ) {
            if (type->persistenceRestoreScalarAbi == XL6_VALUE_ADMIT_NONE)
                return xl6IrCgenDiag(diags, module, XL6_IR_NODE_INVALID,
                    "XLIR persistence restore scalar has no canonical wire domain");
            xl6OutputPrintf(out,
                "    return xl6PersistenceRestoreScalarV1(source, %u, %s);\n",
                (unsigned)type->persistenceRestoreScalarAbi,
                format == XL6_IR_PERSIST_FORMAT_JSON ? "true" : "false");
        } else if ( type->persistenceValueAbi ==
                        XL6_IR_PERSISTENCE_VALUE_CLASS_SCHEMA ) {
            xl6OutputPrintf(out,
                "    xvalue* result;\n"
                "    if (source == NULL || xrtValueType(source) != XVALUE_OBJECT) return NULL;\n"
                "    result = xlir_persist_%s_default_t%u();\n"
                "    if (result == NULL || !xlir_persist_%s_assign_t%u(result, source)) { xrtValueRelease(result); return NULL; }\n"
                "    return result;\n",
                formatName, (unsigned int)typeId,
                formatName, (unsigned int)typeId);
        } else if ( type->persistenceValueAbi ==
                        XL6_IR_PERSISTENCE_VALUE_NULLABLE ) {
            xl6OutputPrintf(out,
                "    if (source == NULL) return NULL;\n"
                "    if (xrtValueType(source) == XVALUE_NULL) return xrtValueRetain(xrtValueNull());\n"
                "    return xlir_persist_%s_restore_t%u(source);\n",
                formatName, (unsigned int)type->persistencePayloadType);
        } else if (type->persistenceValueAbi >= XL6_IR_PERSISTENCE_VALUE_ARRAY &&
                   type->persistenceValueAbi <= XL6_IR_PERSISTENCE_VALUE_DICT_OBJECT) {
            const char* kind = type->persistenceValueAbi == XL6_IR_PERSISTENCE_VALUE_ARRAY ? "XL6_PERSIST_RESTORE_ARRAY_V1" :
                type->persistenceValueAbi == XL6_IR_PERSISTENCE_VALUE_LIST_SEQUENCE ? "XL6_PERSIST_RESTORE_LIST_V1" :
                type->persistenceValueAbi == XL6_IR_PERSISTENCE_VALUE_SET_SEQUENCE ? "XL6_PERSIST_RESTORE_SET_V1" :
                "XL6_PERSIST_RESTORE_DICT_V1";
            if (type->persistenceRestoreCarrierAbi == XL6_IR_COLLECTION_CARRIER_SEQUENCE_V1) {
                xl6OutputPrintf(out,
                    "    xvalue* wire = xl6PersistenceRestoreCollectionV1(source, %s, 0x%016llXULL, xlir_persist_%s_restore_t%u);\n"
                    "    xvalue* result = NULL;\n"
                    "    if (wire == NULL) return NULL;\n",
                    kind, (unsigned long long)type->typeValueId,
                    formatName, (unsigned)type->persistencePayloadType);
                if (!xl6IrCgenEmitPersistenceSequenceSeed(out, module, typeId, "result", diags)) return 0;
                xl6OutputPuts("    return xl6PersistenceRestoreArrayTakeV1(result, wire);\n", out);
            } else {
                xl6OutputPrintf(out,
                    "    return xl6PersistenceRestoreCollectionV1(source, %s, 0x%016llXULL, xlir_persist_%s_restore_t%u);\n",
                    kind, (unsigned long long)type->typeValueId,
                    formatName, (unsigned)type->persistencePayloadType);
            }
        } else {
            return xl6IrCgenDiag(diags, module, XL6_IR_NODE_INVALID,
                "XLIR C emitter persistence restore has no exact value ABI");
        }
        xl6OutputPuts("}\n", out);
    }
    if ( restore ) xl6OutputPuts("\n", out);
    return !xl6OutputHasFailed(out);
}

static int xl6IrCgenEmitPersistenceHelpers(
    XL6_Output* out, const XL6_IrModule* module, uint32 format,
    int readUsed, XL6_DiagList* diags)
{
    const char* formatName;
    const char* stringifyName;
    const char* parseName;
    const char* writeName;
    const char* readName;
    const XL6_IrClassField* dynamicField = NULL;
    const XL6_IrClassField* dynamicHandleField = NULL;
    size_t layoutIndex;
    size_t typeIndex;
    if ( out == NULL || module == NULL ) return 0;
    if ( format == XL6_IR_PERSIST_FORMAT_JSON ) {
        formatName = "json";
        stringifyName = "xrtJsonStringify";
        parseName = "xrtJsonParse";
        writeName = "xrtJsonStringifyFile";
        readName = "xrtJsonParseFile";
    } else if ( format == XL6_IR_PERSIST_FORMAT_XLON ) {
        formatName = "xlon";
        stringifyName = "xrtXlonStringify";
        parseName = "xrtXlonParse";
        writeName = "xrtXlonStringifyFile";
        readName = "xrtXlonParseFile";
    } else {
        return xl6IrCgenDiag(diags, module, XL6_IR_NODE_INVALID,
            "XLIR C emitter persistence helper has no exact format ABI");
    }
    for ( layoutIndex = 0; layoutIndex < module->classLayoutCount;
          ++layoutIndex ) {
        const XL6_IrClassLayout* layout =
            &module->classLayouts[layoutIndex];
        if ( !xl6IrCgenPersistenceLayoutSupported(
                 module, layout->type, format) ||
             layout->persistenceObjectAbi !=
                 XL6_IR_PERSISTENCE_OBJECT_OPEN_DYNAMIC_SCHEMA ) continue;
        if ( layout->persistenceDynamicField >= module->classFieldCount ||
             layout->persistenceDynamicHandleField >=
                 module->classFieldCount )
            return xl6IrCgenDiag(diags, module, layout->declaration,
                "XLIR C emitter open persistence schema has no exact DynamicFields storage path");
        if ( dynamicField == NULL ) {
            dynamicField = &module->classFields[
                layout->persistenceDynamicField];
            dynamicHandleField = &module->classFields[
                layout->persistenceDynamicHandleField];
        } else if ( dynamicField != &module->classFields[
                       layout->persistenceDynamicField] ||
                   dynamicHandleField != &module->classFields[
                       layout->persistenceDynamicHandleField] ) {
            return xl6IrCgenDiag(diags, module, layout->declaration,
                "XLIR C emitter open persistence schemas disagree on DynamicFields storage identity");
        }
    }
    if ( dynamicField != NULL ) {
        xl6OutputPrintf(out,
            "static xrtdynamicfields* xlir_persist_%s_dynamic_fields(const xvalue* source)\n"
            "{\n"
            "    const xvalue* storage;\n"
            "    const xvalue* boxed;\n"
            "    void* handle = NULL;\n"
            "    if (source == NULL) return NULL;\n"
            "    storage = xrtValueObjectGet(source, xrtStrView(",
            formatName);
        xl6IrCgenEmitCStringLiteral(out, dynamicField->name);
        xl6OutputPuts("));\n    if (storage == NULL) return NULL;\n"
            "    boxed = xrtValueObjectGet(storage, xrtStrView(", out);
        xl6IrCgenEmitCStringLiteral(out, dynamicHandleField->name);
        xl6OutputPuts(
            "));\n"
            "    if (boxed == NULL || !xrtValueGetHandle(boxed, &handle, NULL, NULL)) return NULL;\n"
            "    return (xrtdynamicfields*)handle;\n"
            "}\n\n",
            out);
    }
    xl6OutputPuts(
        "#include <__xl_runtime/xl6_serialization_read.h>\n"
        "#include <__xl_runtime/xl6_persistence_restore.h>\n"
        "typedef struct xvalueiter xvalueiter;\n"
        "typedef struct xvaluekey xvaluekey;\n"
        "extern xvalue* xrtValueObjectLifo(void);\n"
        "extern xvalue* xrtValueObject(void);\n"
        "extern xvalue* xrtValueArray(void);\n"
        "extern xvalue* xrtValueArrayGet(const xvalue*, size_t);\n"
        "extern xvalue* xrtValueIntMap(void);\n"
        "extern bool xrtValueIntMapSetNew(xvalue*, int64_t, xvalue*);\n"
        "extern xvalue* xrtValueSet(void);\n"
        "extern bool xrtValueSetAddNew(xvalue*, xvalue*);\n"
        "extern xvalue* xrtValueObjectAt(const xvalue*, size_t, xlir_string_view*);\n"
        "extern xvalueiter* xrtValueIterCreate(const xvalue*);\n"
        "#ifndef XLIR_XRT_DECLARATIONS\n"
        "extern int xrtValueIterAdvance(xvalueiter*, xvaluekey*, xvalue**);\n"
        "#endif\n"
        "extern void xrtValueIterDestroy(xvalueiter*);\n"
        "extern size_t xrtValueCount(const xvalue*);\n"
        "extern bool xrtValueArrayAppendNew(xvalue*, xvalue*);\n"
        "extern bool xrtValueTypeIdBind(xvalue*, uint64_t);\n"
        "extern xvalue* xrtValueString(xlir_string_view);\n\n",
        out);
    for (typeIndex = 0; typeIndex < module->types.count; ++typeIndex)
        if (module->types.items[typeIndex].persistenceRestoreCarrierAbi == XL6_IR_COLLECTION_CARRIER_SEQUENCE_V1 &&
            xl6IrCgenPersistenceTypeUsed(module, (XL6_IrTypeId)(typeIndex + 1), format, 1)) {
            xl6OutputPuts("#include <__xl_runtime/xl6_persistence_restore_array.h>\n", out);
            break;
        }
    for ( typeIndex = 0; typeIndex < module->types.count; ++typeIndex ) {
        XL6_IrTypeId typeId = (XL6_IrTypeId)(typeIndex + 1);
        if ( xl6IrCgenPersistenceTypeUsed(module, typeId, format, 1) )
            xl6OutputPrintf(out,
                "static xvalue* xlir_persist_%s_restore_t%u(const xvalue*);\n",
                formatName, (unsigned int)typeId);
    }
    for ( layoutIndex = 0; layoutIndex < module->classLayoutCount;
          ++layoutIndex ) {
        const XL6_IrClassLayout* layout =
            &module->classLayouts[layoutIndex];
        if ( !xl6IrCgenPersistenceLayoutSupported(
                 module, layout->type, format) ) continue;
        xl6OutputPrintf(out,
            "static bool xlir_persist_%s_assign_t%u(xvalue*, const xvalue*);\n"
            "static bool xlir_persist_%s_parse_t%u(xlir_string, xvalue*, xvalue**);\n",
            formatName, (unsigned int)layout->type,
            formatName, (unsigned int)layout->type);
        if (readUsed) xl6OutputPrintf(out,
            "static bool xlir_persist_%s_read_t%u(xlir_string, xvalue*, xvalue**);\n",
            formatName, (unsigned int)layout->type);
        if ( layout->persistenceConstructionAbi ==
                 XL6_IR_PERSISTENCE_CONSTRUCTION_SCHEMA_DEFAULT &&
             xl6IrCgenPersistenceTypeUsed(module, layout->type, format, 1) )
            xl6OutputPrintf(out,
                "static xvalue* xlir_persist_%s_default_t%u(void);\n",
                formatName, (unsigned int)layout->type);
    }
    xl6OutputPuts("\n", out);
    for ( layoutIndex = 0; layoutIndex < module->classLayoutCount;
          ++layoutIndex ) {
        const XL6_IrClassLayout* layout =
            &module->classLayouts[layoutIndex];
        const XL6_IrPersistenceRestoreSchema* schema =
            xl6IrPersistenceRestoreSchemaGet(module, layout->type, format);
        size_t fieldIndex;
        if (schema == NULL) continue;
        if ( layout->persistenceConstructionAbi ==
                 XL6_IR_PERSISTENCE_CONSTRUCTION_SCHEMA_DEFAULT &&
             xl6IrCgenPersistenceTypeUsed(module, layout->type, format, 1) &&
             !xl6IrCgenEmitPersistenceClassDefault(
                 out, module, layout, formatName, diags) ) return 0;
        xl6OutputPrintf(out,
            "static bool xlir_persist_%s_assign_t%u(xvalue* target, const xvalue* source)\n"
            "{\n"
            "    const xvalue* borrowed;\n"
            "    xvalue* item;\n"
            "    (void)borrowed; (void)item;\n"
            "    if (target == NULL || source == NULL || xrtValueType(source) != XVALUE_OBJECT) return false;\n",
            formatName, (unsigned int)layout->type);
        if (schema->dynamicField != XL6_IR_CLASS_FIELD_INDEX_INVALID) {
            xl6OutputPrintf(out,
                "    {\n"
                "        xrtdynamicfields* targetFields = xlir_persist_%s_dynamic_fields(target);\n"
                "        xrtdynamicfields* sourceFields = xrtDynamicFieldsFromValue(source);\n"
                "        bool merged = targetFields != NULL && sourceFields != NULL &&\n"
                "            xrtDynamicFieldsMerge(targetFields, sourceFields, true);\n"
                "        if (sourceFields != NULL) xrtDynamicFieldsUnref(sourceFields);\n"
                "        if (!merged) return false;\n"
                "    }\n",
                formatName);
        }
        for ( fieldIndex = 0; fieldIndex < schema->fieldCount;
              ++fieldIndex ) {
            const XL6_IrPersistenceRestoreField* recipe =
                &module->persistenceRestoreFields[schema->firstField + fieldIndex];
            const XL6_IrClassField* field =
                &module->classFields[recipe->classField];
            xl6OutputPuts("    borrowed = xrtValueObjectGet(source, (xstrview)", out);
            xl6IrCgenEmitPersistenceByteView(out, recipe->sourceKey,
                recipe->sourceKeyAbi == XL6_IR_PERSIST_PROJECT_KEY_C_LITERAL);
            xl6OutputPuts(");\n", out);
            xl6OutputPuts("    if (borrowed != NULL) {\n", out);
            if (!recipe->acceptNull)
                xl6OutputPuts("        if (xrtValueType(borrowed) == XVALUE_NULL) return false;\n", out);
            if (recipe->assignAbi == XL6_IR_PERSIST_RESTORE_CLASS_IN_PLACE) {
                xl6OutputPuts(
                    "        xvalue* nestedTarget = xrtValueObjectGet(target, xrtStrView(",
                    out);
                xl6IrCgenEmitCStringLiteral(out, field->name);
                xl6OutputPrintf(out,
                    "));\n"
                    "        if (nestedTarget == NULL || !xlir_persist_%s_assign_t%u(nestedTarget, borrowed)) return false;\n",
                    formatName, (unsigned int)field->type);
            } else {
                xl6OutputPrintf(out,
                    "        item = xlir_persist_%s_restore_t%u(borrowed);\n"
                    "        if (item == NULL || !xrtValueObjectSetNew(target, xrtStrView(",
                    formatName, (unsigned int)field->type);
                xl6IrCgenEmitCStringLiteral(out, field->name);
                xl6OutputPuts("), item)) return false;\n", out);
            }
            xl6OutputPuts("    }", out);
            if (recipe->required)
                xl6OutputPuts(" else { return false; }", out);
            xl6OutputPuts("\n", out);
        }
        xl6OutputPrintf(out,
            "    return true;\n"
            "}\n"
            "static bool xlir_persist_%s_parse_t%u(xlir_string text, xvalue* target, xvalue** result)\n"
            "{\n"
            "    xerror* prior = xrtTakeError();\n"
            "    xvalue* source = text.Data != NULL && target != NULL ? %s(text) : NULL;\n"
            "    return xl6SerializeRestoreReadFinishV1(source, target, xlir_persist_%s_assign_t%u, prior, result);\n"
            "}\n",
            formatName, (unsigned int)layout->type, parseName,
            formatName, (unsigned int)layout->type);
        if (readUsed) xl6OutputPrintf(out,
            "static bool xlir_persist_%s_read_t%u(xlir_string path, xvalue* target, xvalue** result)\n"
            "{\n"
            "    xerror* prior = xrtTakeError();\n"
            "    const char* cpath = xlir_string_cstr(path);\n"
            "    xvalue* source = cpath != NULL && target != NULL ? %s(cpath) : NULL;\n"
            "    return xl6SerializeRestoreReadFinishV1(source, target, xlir_persist_%s_assign_t%u, prior, result);\n"
            "}\n\n",
            formatName, (unsigned int)layout->type, readName,
            formatName, (unsigned int)layout->type);
    }
    for ( typeIndex = 0; typeIndex < module->types.count; ++typeIndex )
        if ( !xl6IrCgenEmitPersistenceTypeHelpers(
                 out, module, (XL6_IrTypeId)(typeIndex + 1),
                 format, formatName, diags) ) return 0;
    if ( xl6OutputHasFailed(out) )
        return xl6IrCgenDiag(diags, module, XL6_IR_NODE_INVALID,
            "XLIR C emitter persistence helper output failed");
    return 1;
}

/* The native callback is only a physical ABI adapter. All destructor calls,
 * borrowed receiver views and failure continuation live in one XLIR body. */
static int xl6IrCgenEmitClassDestroyHelpers(
    XL6_Output* out, const XL6_IrModule* module, XL6_DiagList* diags)
{
    size_t instructionIndex;
    for ( instructionIndex = 0;
          instructionIndex < module->instructionCount;
          ++instructionIndex ) {
        const XL6_IrInstruction* instruction =
            &module->instructions[instructionIndex];
        const XL6_IrClassLayout* layout;
        size_t prior;
        if ( !xl6IrCgenInstructionIsReachable(module, instructionIndex) ||
             instruction->kind != XL6_IR_INSTR_RUNTIME_CALL ||
             instruction->runtimeOp !=
                 XL6_IR_RUNTIME_XVALUE_CLASS_NEW ) continue;
        for ( prior = 0; prior < instructionIndex; ++prior ) {
            const XL6_IrInstruction* candidate =
                &module->instructions[prior];
            if ( xl6IrCgenInstructionIsReachable(module, prior) &&
                 candidate->kind == XL6_IR_INSTR_RUNTIME_CALL &&
                 candidate->runtimeOp ==
                     XL6_IR_RUNTIME_XVALUE_CLASS_NEW &&
                 candidate->type == instruction->type ) break;
        }
        if ( prior != instructionIndex ) continue;
        layout = xl6IrCgenClassLayout(module, instruction->type);
        if ( layout == NULL || module->abi.classStorageAbi != XL6_IR_CLASS_STORAGE_RESIDENT_GRAPH_V2 )
            return xl6IrCgenDiag(diags, module, instruction->origin,
                "XLIR C emitter class destroy layout is unresolved");
        if (layout->destroyFunction == 0) continue;
        xl6OutputPrintf(out,
            "static bool xlir_class_destroy_finalizer_t%u(xvalue* value, void* data)\n"
            "{\n"
            "    xlir_result_void outcome; (void)data;\n"
            "    outcome = xlir_fn_%u(value);\n"
            "    return outcome.ok;\n",
            (unsigned int)instruction->type,
            (unsigned int)module->functions[layout->destroyFunction - 1].symbol);
        xl6OutputPuts("}\n\n", out);
    }
    return !xl6OutputHasFailed(out);
}
