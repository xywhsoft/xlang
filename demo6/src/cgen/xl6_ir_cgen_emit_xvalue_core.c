static int xl6IrCgenEmitXValueCoreRuntime(
    XL6_Output* out, const XL6_IrModule* module,
    const XL6_IrCgenNeeds* needs, XL6_DiagList* diags)
{
    size_t i;
    size_t parameter;
    if ( needs->needsXValueRuntime || module->abi.entryKind == XL6_IR_MODULE_ENTRY_XLANG_MAIN ) {
        xl6OutputPuts(
            "#ifndef XLIR_XRT_DECLARATIONS\n"
            "typedef struct xvalue xvalue;\n"
            "#endif\n", out);
        if ( needs->needsXValueRuntime )
             xl6OutputPuts("extern xvalue* xrtValueNull(void);\n"
                          "extern xvalue* xrtValueBool(bool);\n"
                          "extern xvalue* xrtValueInt(int64_t);\n"
                          "extern xvalue* xrtValueUInt(uint64_t);\n"
                           "extern xvalue* xrtValueFloat(double);\n"
                           "extern xvalue* xrtValueTime(int64_t);\n"
                           "extern xvalue* xrtValueChar(uint32_t);\n"
                           "extern bool xrtValueGetChar(const xvalue*, uint32_t*);\n"
                           "extern xvalue* xrtValueRetain(const xvalue*);\n"
                           "extern xvalue* xrtValueClone(const xvalue*);\n"
                           "extern xvalue* xl6ValueCloneV1(const xvalue*);\n"
                           "extern xvalue* xl6ValueDeepCloneV1(const xvalue*);\n"
                           "extern void xrtValueRelease(xvalue*);\n"
                           "#ifndef XLIR_XRT_DECLARATIONS\nextern int xrtValueType(const xvalue*);\n#endif\n"
                           "extern bool xrtValueTruthy(const xvalue*);\n", out);
        if ( needs->needsXValueWeakRuntime )
            xl6OutputPuts(
                "extern xvalue* xrtValueWeakRef(const xvalue*);\n"
                "extern xvalue* xrtValueWeakRefLock(const xvalue*);\n"
                "static xvalue* xlir_xvalue_weak_create(const xvalue* source)\n"
                "{\n"
                "    if (source == NULL || xrtValueType(source) == 0) return xrtValueNull();\n"
                "    return xrtValueWeakRef(source);\n"
                "}\n"
                "static xvalue* xlir_xvalue_weak_lock(const xvalue* source)\n"
                "{\n"
                "    if (source == NULL || xrtValueType(source) == 0) return xrtValueNull();\n"
                "    return xrtValueWeakRefLock(source);\n"
                "}\n",
                out);
        if ( needs->needsXValueClassFinalizerRuntime )
            xl6OutputPuts("#include <__xl_runtime/xl6_code_lease.h>\n", out);
        if ( needs->needsPersistenceDynamicFields ||
             (needs->needsXValueDynamicFieldRuntime && needs->openDynamicField != NULL &&
              needs->openDynamicHandleField != NULL) )
            xl6OutputPuts(
                "typedef struct xrtobject xrtdynamicfields;\n"
                "struct xvaluehandleops;\n"
                "extern bool xrtValueGetHandle(const xvalue*, void**, const struct xvaluehandleops**, void**);\n",
                out);
        if ( needs->needsPersistenceDynamicFields )
            xl6OutputPuts(
                "extern xvalue* xrtDynamicFieldsToValue(const xrtdynamicfields*);\n"
                "extern xrtdynamicfields* xrtDynamicFieldsFromValue(const xvalue*);\n"
                "extern bool xrtDynamicFieldsMerge(xrtdynamicfields*, const xrtdynamicfields*, bool);\n"
                "extern void xrtDynamicFieldsUnref(xrtdynamicfields*);\n",
                out);
        if ( needs->needsXValueDynamicFieldRuntime && needs->openDynamicField != NULL &&
             needs->openDynamicHandleField != NULL )
            xl6OutputPuts(
                "extern const xvalue* xrtDynamicFieldsGet(const xrtdynamicfields*, xlir_string_view);\n"
                "extern bool xrtDynamicFieldsSetNew(xrtdynamicfields*, xlir_string_view, xvalue*);\n",
                out);
        if ( (needs->needsPersistenceFormats & XL6_IR_PERSIST_FORMAT_JSON) != 0 )
            xl6OutputPuts(
                "extern char* xrtJsonStringify(const xvalue*, bool, size_t*);\n"
                "extern xvalue* xrtJsonParse(xlir_string_view);\n"
                "extern bool xrtJsonStringifyFile(const char*, const xvalue*, bool);\n"
                "extern xvalue* xrtJsonParseFile(const char*);\n",
                out);
        if ( (needs->needsPersistenceFormats & XL6_IR_PERSIST_FORMAT_XLON) != 0 )
            xl6OutputPuts(
                "extern char* xrtXlonStringify(const xvalue*, bool, size_t*);\n"
                "extern xvalue* xrtXlonParse(xlir_string_view);\n"
                "extern bool xrtXlonStringifyFile(const char*, const xvalue*, bool);\n"
                "extern xvalue* xrtXlonParseFile(const char*);\n",
                out);
        if ( needs->needsCollectionAlgebraRuntime ) {
            xl6OutputPuts(
                "extern xvalue* xl6ArrayConcatV1(const xvalue*, const xvalue*);\n"
                "extern xvalue* xrtValueSetUnion(const xvalue*, const xvalue*);\n"
                "extern xvalue* xrtValueSetIntersection(const xvalue*, const xvalue*);\n"
                "extern xvalue* xrtValueSetDifference(const xvalue*, const xvalue*);\n"
                "extern xvalue* xrtValueSetSymmetricDifference(const xvalue*, const xvalue*);\n"
                "extern bool xrtValueSetIsSubset(const xvalue*, const xvalue*, bool);\n"
                "extern bool xrtValueSetIsSuperset(const xvalue*, const xvalue*, bool);\n",
                out);
            if ( needs->needsCollectionListMergeRuntime )
                xl6OutputPuts(
                    "static xvalue* xlir_xvalue_list_merge_right(const xvalue* left, const xvalue* right)\n"
                    "{\n"
                    "    xvalue* result = xrtValueClone(left);\n"
                    "    if (result == NULL || !xrtValueIntMapMerge(result, right, XVALUE_MERGE_REPLACE)) {\n"
                    "        xrtValueRelease(result); return NULL;\n"
                    "    }\n"
                    "    return result;\n"
                    "}\n",
                    out);
            if ( needs->needsCollectionListWithoutRuntime )
                xl6OutputPuts(
                    "extern bool xrtValueIntMapRemove(xvalue*, int64_t);\n"
                    "static xvalue* xlir_xvalue_list_without_i64_key(const xvalue* left, int64_t key)\n"
                    "{\n"
                    "    xvalue* result = xrtValueClone(left);\n"
                    "    if (result != NULL) (void)xrtValueIntMapRemove(result, key);\n"
                    "    return result;\n"
                    "}\n",
                    out);
            if ( needs->needsCollectionDictMergeRuntime )
                xl6OutputPuts(
                    "static xvalue* xlir_xvalue_dict_merge_right(const xvalue* left, const xvalue* right)\n"
                    "{\n"
                    "    xvalue* result = xrtValueClone(left);\n"
                    "    if (result == NULL || !xrtValueObjectMerge(result, right, XVALUE_MERGE_REPLACE)) {\n"
                    "        xrtValueRelease(result); return NULL;\n"
                    "    }\n"
                    "    return result;\n"
                    "}\n",
                    out);
            if ( needs->needsCollectionDictWithoutRuntime )
                xl6OutputPuts(
                    "extern bool xrtValueObjectRemove(xvalue*, xlir_string_view);\n"
                    "static xvalue* xlir_xvalue_dict_without_string_key(const xvalue* left, xlir_string key)\n"
                    "{\n"
                    "    xvalue* result = xrtValueClone(left);\n"
                    "    if (result != NULL) (void)xrtValueObjectRemove(result, key);\n"
                    "    return result;\n"
                    "}\n",
                    out);
        }
        if ( needs->needsXValueTypeIdentityRuntime ) {
            const XL6_IrRuntimeTypeMap* map = &module->runtimeTypeMap;
            xl6OutputPuts(
                "extern uint64_t xrtValueTypeId(const xvalue*);\n"
                "extern bool xrtValueTypeIdBind(xvalue*, uint64_t);\n"
                "extern bool xrtValueTypeIdRebind(xvalue*, uint64_t);\n"
                "static xvalue* xlir_xvalue_type_bind(xvalue* value, uint64_t typeId)\n"
                "{ if (value == NULL || typeId == 0 || !xrtValueTypeIdBind(value, typeId)) abort(); return value; }\n"
                "static xvalue* xlir_xvalue_type_rebind_owned(xvalue* value, uint64_t typeId)\n"
                "{ if (value == NULL || typeId == 0 || !xrtValueTypeIdRebind(value, typeId)) abort(); return value; }\n",
                out);
            xl6OutputPrintf(out,
                "static uint64_t xlir_xvalue_type_id(const xvalue* value)\n"
                "{\n"
                "    uint64_t bound = xrtValueTypeId(value);\n"
                "    if (bound != 0) return bound;\n"
                "    if (value == NULL) return 0x%016llXULL;\n"
                "    switch (xrtValueType(value)) {\n"
                "    case 0: return 0x%016llXULL;\n"
                "    case 1: return 0x%016llXULL;\n"
                "    case 2: return 0x%016llXULL;\n"
                "    case 3: return 0x%016llXULL;\n"
                "    case 4: return 0x%016llXULL;\n"
                "    case 5: return 0x%016llXULL;\n"
                "    case 6: return 0x%016llXULL;\n"
                "    case 7: return 0x%016llXULL;\n"
                "    case 8: return 0x%016llXULL;\n"
                "    case 9: return 0x%016llXULL;\n"
                "    case 10: return 0x%016llXULL;\n"
                "    case 11: return 0x%016llXULL;\n"
                "    case 12: return 0x%016llXULL;\n"
                "    case 13: return 0x%016llXULL;\n"
                "    case 14: return 0x%016llXULL;\n"
                "    default: return 0x%016llXULL;\n"
                "    }\n"
                "}\n",
                (unsigned long long)map->nullTypeValueId,
                (unsigned long long)map->nullTypeValueId,
                (unsigned long long)map->boolTypeValueId,
                (unsigned long long)map->i64TypeValueId,
                (unsigned long long)map->f64TypeValueId,
                (unsigned long long)map->stringTypeValueId,
                (unsigned long long)map->bytesTypeValueId,
                (unsigned long long)map->timeTypeValueId,
                (unsigned long long)map->pointerTypeValueId,
                (unsigned long long)map->handleTypeValueId,
                (unsigned long long)map->arrayTypeValueId,
                (unsigned long long)map->intMapTypeValueId,
                (unsigned long long)map->setTypeValueId,
                (unsigned long long)map->objectTypeValueId,
                (unsigned long long)map->u64TypeValueId,
                (unsigned long long)map->charTypeValueId,
                (unsigned long long)map->fallbackTypeValueId);
        }
        if ( needs->needsXValueCheckedCastRuntime )
            xl6OutputPrintf(out,
                "struct xvaluehandleops;\n"
                "extern bool xrtValueGetBool(const xvalue*, bool*);\n"
                "extern bool xrtValueGetInt(const xvalue*, int64_t*);\n"
                "extern bool xrtValueGetUInt(const xvalue*, uint64_t*);\n"
                "extern bool xrtValueGetFloat(const xvalue*, double*);\n"
                "extern bool xrtValueGetTime(const xvalue*, int64_t*);\n"
                "extern bool xrtValueGetString(const xvalue*, xlir_string_view*);\n"
                "extern bool xrtValueGetPointer(const xvalue*, void**);\n"
                "extern bool xl6ErrorValueReadRefV1(const xvalue*, uint64_t, xerror**);\n"
                "extern char* xrtStrDupN(const char*, size_t);\n"
                "static bool xlir_xvalue_cast_checked(const xvalue* source, uint64_t expectedTypeId, int castAbi, void* outValue)\n"
                "{\n"
                "    xlir_string_view text = { 0 }; char* copy = NULL;\n"
                "    if (outValue == NULL || expectedTypeId == 0) goto runtime_failure;\n"
                "    switch (castAbi) {\n"
                "    case %d: *(bool*)outValue = false; break;\n"
                "    case %d: *(int64_t*)outValue = 0; break;\n"
                "    case %d: *(uint64_t*)outValue = 0; break;\n"
                "    case %d: *(double*)outValue = 0.0; break;\n"
                "    case %d: *(int64_t*)outValue = 0; break;\n"
                "    case %d: *(char**)outValue = NULL; break;\n"
                "    case %d: *(void**)outValue = NULL; break;\n"
                "    case %d: *(xerror**)outValue = NULL; break;\n"
                "    case %d: *(xvalue**)outValue = NULL; break;\n"
                "    default: goto runtime_failure;\n"
                "    }\n"
                "    if (source == NULL || xlir_xvalue_type_id(source) != expectedTypeId) goto incompatible;\n"
                "    switch (castAbi) {\n"
                "    case %d: if (xrtValueGetBool(source, (bool*)outValue)) return true; break;\n"
                "    case %d: if (xrtValueGetInt(source, (int64_t*)outValue)) return true; break;\n"
                "    case %d: if (xrtValueGetUInt(source, (uint64_t*)outValue)) return true; break;\n"
                "    case %d: if (xrtValueGetFloat(source, (double*)outValue)) return true; break;\n"
                "    case %d: if (xrtValueGetTime(source, (int64_t*)outValue)) return true; break;\n"
                "    case %d:\n"
                "        if (!xrtValueGetString(source, &text)) break;\n"
                "        copy = xrtStrDupN(text.Data, text.Size);\n"
                "        if (copy == NULL) goto runtime_failure;\n"
                "        *(char**)outValue = copy; return true;\n"
                "    case %d: if (xrtValueGetPointer(source, (void**)outValue)) return true; break;\n"
                "    case %d:\n"
                "        if (xl6ErrorValueReadRefV1(source, expectedTypeId, (xerror**)outValue)) return true;\n"
                "        break;\n"
                "    case %d:\n"
                "        *(xvalue**)outValue = xrtValueRetain(source);\n"
                "        if (*(xvalue**)outValue != NULL) return true;\n"
                "        goto runtime_failure;\n"
                "    default: goto runtime_failure;\n"
                "    }\n"
                "incompatible:\n"
                "    if (xrtGetError() == NULL) xrtSetErrorTake(xlir_error_create(\"TypeError\", \"dynamic value does not match requested as target\"));\n"
                "    return false;\n"
                "runtime_failure:\n"
                "    if (xrtGetError() == NULL) xrtSetErrorTake(xlir_error_create(\"MemoryError\", \"checked cast result allocation failed\"));\n"
                "    return false;\n"
                "}\n",
                (int)XL6_IR_CAST_XVALUE_TO_BOOL_CHECKED,
                (int)XL6_IR_CAST_XVALUE_TO_I64_CHECKED,
                (int)XL6_IR_CAST_XVALUE_TO_U64_CHECKED,
                (int)XL6_IR_CAST_XVALUE_TO_F64_CHECKED,
                (int)XL6_IR_CAST_XVALUE_TO_TIME_CHECKED,
                (int)XL6_IR_CAST_XVALUE_TO_STRING_CHECKED,
                (int)XL6_IR_CAST_XVALUE_TO_RAW_POINTER_CHECKED,
                (int)XL6_IR_CAST_XVALUE_TO_ERROR_CHECKED,
                (int)XL6_IR_CAST_XVALUE_TO_BYTES_CHECKED,
                (int)XL6_IR_CAST_XVALUE_TO_BOOL_CHECKED,
                (int)XL6_IR_CAST_XVALUE_TO_I64_CHECKED,
                (int)XL6_IR_CAST_XVALUE_TO_U64_CHECKED,
                (int)XL6_IR_CAST_XVALUE_TO_F64_CHECKED,
                (int)XL6_IR_CAST_XVALUE_TO_TIME_CHECKED,
                (int)XL6_IR_CAST_XVALUE_TO_STRING_CHECKED,
                (int)XL6_IR_CAST_XVALUE_TO_RAW_POINTER_CHECKED,
                (int)XL6_IR_CAST_XVALUE_TO_ERROR_CHECKED,
                (int)XL6_IR_CAST_XVALUE_TO_BYTES_CHECKED);
        if ( needs->needsXValueNullableStringCastRuntime )
            xl6OutputPuts(
                "extern bool xrtValueGetString(const xvalue*, xlir_string_view*);\n"
                "extern char* xrtStrDupN(const char*, size_t);\n"
                "static xlir_string xlir_xvalue_try_cast_nullable_string(const xvalue* source, uint64_t expectedTypeId)\n"
                "{\n"
                "    xlir_string_view text = { 0 }; xlir_string copy = {0};\n"
                "    if (source == NULL || xrtValueType(source) == 0 || xlir_xvalue_type_id(source) != expectedTypeId) return copy;\n"
                "    if (!xrtValueGetString(source, &text)) goto runtime_failure;\n"
                "    copy = xlir_string_from_utf8(text.Data, text.Size);\n"
                "    if (copy.Data != NULL) return copy;\n"
                "runtime_failure:\n"
                "    if (xrtGetError() == NULL) xrtSetErrorTake(xlir_error_create(\"MemoryError\", \"nullable string cast result allocation failed\"));\n"
                "    return (xlir_string){0};\n"
                "}\n",
                out);
        if ( needs->needsXValueNullableRetainProjectionRuntime )
            xl6OutputPuts(
                "static xvalue* xlir_xvalue_project_nullable_retain(const xvalue* source, uint64_t exactTypeId)\n"
                "{\n"
                "    xvalue* retained;\n"
                "    if (source == NULL) abort();\n"
                "    if (exactTypeId != 0 && xrtValueType(source) != 0 && xlir_xvalue_type_id(source) != exactTypeId) abort();\n"
                "    retained = xrtValueRetain(source); if (retained == NULL) abort();\n"
                "    return retained;\n"
                "}\n",
                out);
        if ( needs->needsXValueRetainProjectionRuntime )
            xl6OutputPuts(
                "static xvalue* xlir_xvalue_project_retain(const xvalue* source, uint64_t exactTypeId)\n"
                "{\n"
                "    xvalue* retained;\n"
                "    if (source == NULL || xrtValueType(source) == 0) abort();\n"
                "    if (exactTypeId != 0 && xlir_xvalue_type_id(source) != exactTypeId) abort();\n"
                "    retained = xrtValueRetain(source); if (retained == NULL) abort();\n"
                "    return retained;\n"
                "}\n",
                out);
        if ( needs->needsXValueScalarProjectionRuntime )
            xl6OutputPuts(
                "extern bool xrtValueGetBool(const xvalue*, bool*);\n"
                "extern bool xrtValueGetInt(const xvalue*, int64_t*);\n"
                "extern bool xrtValueGetUInt(const xvalue*, uint64_t*);\n"
                "extern bool xrtValueGetFloat(const xvalue*, double*);\n"
                "extern bool xrtValueGetTime(const xvalue*, int64_t*);\n",
                out);
        if ( needs->needsXValueRawPointerProjectionRuntime )
            xl6OutputPuts(
                "extern bool xrtValueGetPointer(const xvalue*, void**);\n",
                out);
        if ( needs->needsXValueUnionRuntime )
            xl6OutputPuts(
                "typedef xbytesview xlir_bytes_view;\n"
                "static xvalue* xlir_xvalue_union_box(const void* source, size_t size, uint64_t typeId)\n"
                "{\n"
                "    xlir_bytes_view bytes;\n"
                "    xvalue* value;\n"
                "    if (source == NULL || size == 0 || typeId == 0) abort();\n"
                "    bytes.Data = (const unsigned char*)source;\n"
                "    bytes.Size = size;\n"
                "    value = xrtValueBytes(bytes);\n"
                "    if (value == NULL) return NULL;\n"
                "    return xlir_xvalue_type_bind(value, typeId);\n"
                "}\n"
                "static void xlir_xvalue_union_unbox(const xvalue* source, uint64_t typeId, void* destination, size_t size)\n"
                "{\n"
                "    xlir_bytes_view bytes;\n"
                "    size_t i;\n"
                "    if (source == NULL || destination == NULL || size == 0 || typeId == 0 ||\n"
                "        xlir_xvalue_type_id(source) != typeId ||\n"
                "        !xrtValueGetBytes(source, &bytes) || bytes.Data == NULL || bytes.Size != size) abort();\n"
                "    for (i = 0; i < size; ++i) ((unsigned char*)destination)[i] = bytes.Data[i];\n"
                "}\n",
                out);
        if ( needs->needsXValueUnionCheckedCastRuntime )
            xl6OutputPuts(
                "static bool xlir_xvalue_union_cast_checked(const xvalue* source, uint64_t typeId, void* destination, size_t size)\n"
                "{\n"
                "    xlir_bytes_view bytes; size_t i;\n"
                "    if (destination == NULL || size == 0 || typeId == 0) goto runtime_failure;\n"
                "    for (i = 0; i < size; ++i) ((unsigned char*)destination)[i] = 0;\n"
                "    if (source == NULL || xrtValueType(source) == 0 || xlir_xvalue_type_id(source) != typeId ||\n"
                "        !xrtValueGetBytes(source, &bytes) || bytes.Data == NULL || bytes.Size != size) goto incompatible;\n"
                "    for (i = 0; i < size; ++i) ((unsigned char*)destination)[i] = bytes.Data[i];\n"
                "    return true;\n"
                "incompatible:\n"
                "    if (xrtGetError() == NULL) xrtSetErrorTake(xlir_error_create(\"TypeError\", \"dynamic value does not match requested union target\"));\n"
                "    return false;\n"
                "runtime_failure:\n"
                "    if (xrtGetError() == NULL) xrtSetErrorTake(xlir_error_create(\"RuntimeError\", \"checked union cast has an invalid verified ABI\"));\n"
                "    return false;\n"
                "}\n",
                out);
        if ( needs->needsXValueIteratorRuntime )
            xl6OutputPuts(
                "#ifndef XLIR_XRT_DECLARATIONS\n"
                "typedef struct xvalueiter xvalueiter;\n"
                "typedef struct xvaluekey { int Type; union { size_t Index; int64_t Integer; xlir_string_view String; }; } xvaluekey;\n"
                "extern xvalueiter* xrtValueIterCreate(const xvalue*);\n"
                "extern int xrtValueIterAdvance(xvalueiter*, xvaluekey*, xvalue**);\n"
                "extern void xrtValueIterDestroy(xvalueiter*);\n"
                "#endif\n",
                out);
        if ( needs->needsXValueIteratorRuntime )
            xl6OutputPuts(
                "extern void* xrtMalloc(size_t);\n"
                "extern void xrtFree(void*);\n"
                "extern bool xrtValueGetInt(const xvalue*, int64_t*);\n"
                "extern bool xrtValueGetUInt(const xvalue*, uint64_t*);\n"
                "extern bool xrtValueGetBool(const xvalue*, bool*);\n"
                "extern bool xrtValueGetFloat(const xvalue*, double*);\n"
                "static int64_t xlir_xvalue_iterator_item_i64(const xlir_xvalue_iterator* iterator)\n"
                "{\n"
                "    const XL6_ValueIteratorView* state = (xl6ValueIteratorRuntimeV2Get()->View)(iterator);\n"
                "    int64_t value = 0;\n"
                "    if (state == NULL || state->item == NULL || !xrtValueGetInt(state->item, &value)) abort();\n"
                "    return value;\n"
                "}\n"
                "static uint64_t xlir_xvalue_iterator_item_u64(const xlir_xvalue_iterator* iterator)\n"
                "{\n"
                "    const XL6_ValueIteratorView* state = (xl6ValueIteratorRuntimeV2Get()->View)(iterator);\n"
                "    uint64_t value = 0;\n"
                "    if (state == NULL || state->item == NULL || !xrtValueGetUInt(state->item, &value)) abort();\n"
                "    return value;\n"
                "}\n"
                "static bool xlir_xvalue_iterator_item_bool(const xlir_xvalue_iterator* iterator)\n"
                "{\n"
                "    const XL6_ValueIteratorView* state = (xl6ValueIteratorRuntimeV2Get()->View)(iterator);\n"
                "    bool value = false;\n"
                "    if (state == NULL || state->item == NULL || !xrtValueGetBool(state->item, &value)) abort();\n"
                "    return value;\n"
                "}\n"
                "static double xlir_xvalue_iterator_item_f64(const xlir_xvalue_iterator* iterator)\n"
                "{\n"
                "    const XL6_ValueIteratorView* state = (xl6ValueIteratorRuntimeV2Get()->View)(iterator);\n"
                "    double value = 0.0;\n"
                "    if (state == NULL || state->item == NULL || !xrtValueGetFloat(state->item, &value)) abort();\n"
                "    return value;\n"
                "}\n"
                "static xvalue* xlir_xvalue_iterator_item_xvalue_retain(const xlir_xvalue_iterator* iterator)\n"
                "{\n"
                "    const XL6_ValueIteratorView* state = (xl6ValueIteratorRuntimeV2Get()->View)(iterator);\n"
                "    xvalue* retained;\n"
                "    if (state == NULL || state->item == NULL) abort();\n"
                "    retained = xrtValueRetain(state->item); if (retained == NULL) abort();\n"
                "    return retained;\n"
                "}\n"
                "static xvalue* xlir_xvalue_iterator_item_borrow(const xlir_xvalue_iterator* iterator)\n"
                "{ const XL6_ValueIteratorView* state = (xl6ValueIteratorRuntimeV2Get()->View)(iterator);\n"
                "  if (state == NULL || state->item == NULL) abort();\n"
                "  return state->item; }\n"
                "static int64_t xlir_xvalue_iterator_key_ordinal_i64(const xlir_xvalue_iterator* iterator)\n"
                "{\n"
                "    const XL6_ValueIteratorView* state = (xl6ValueIteratorRuntimeV2Get()->View)(iterator);\n"
                "    if (state == NULL || state->item == NULL || state->key.Type != 1 || state->key.Index > INT64_MAX) abort();\n"
                "    return (int64_t)state->key.Index;\n"
                "}\n"
                "static int64_t xlir_xvalue_iterator_key_i64(const xlir_xvalue_iterator* iterator)\n"
                "{\n"
                "    const XL6_ValueIteratorView* state = (xl6ValueIteratorRuntimeV2Get()->View)(iterator);\n"
                "    if (state == NULL || state->item == NULL || state->key.Type != 2) abort();\n"
                "    return state->key.Integer;\n"
                "}\n",
                out);
        if ( needs->needsXValueIteratorDynamicRuntime )
            xl6OutputPuts(
                "#ifndef XLIR_XRT_DECLARATIONS\nextern int xrtValueType(const xvalue*);\n#endif\n"
                "extern xvalue* xrtValueInt(int64_t);\n"
                "extern xvalue* xrtValueString(xlir_string_view);\n"
                "static xlir_xvalue_iterator* xlir_xvalue_iterator_create_checked(const xvalue* source, int expectedCollectionTag)\n"
                "{\n"
                "    xlir_xvalue_iterator* state; int type;\n"
                "    if (source == NULL) goto incompatible;\n"
                "    type = xrtValueType(source);\n"
                "    if (expectedCollectionTag != 0) {\n"
                "        if (expectedCollectionTag < 9 || expectedCollectionTag > 12 || type != expectedCollectionTag) goto incompatible;\n"
                "    } else if (type < 9 || type > 12) goto incompatible;\n"
                "    state = xlir_xvalue_iterator_create(source);\n"
                "    if (state == NULL) goto runtime_failure;\n"
                "    return state;\n"
                "incompatible:\n"
                "    xrtSetErrorTake(xlir_error_create(\"TypeError\", \"for in value must be array, list, set or dict\"));\n"
                "    return NULL;\n"
                "runtime_failure:\n"
                "    if (xrtGetError() == NULL) xrtSetErrorTake(xlir_error_create(\"RuntimeError\", \"dynamic iterator snapshot creation failed\"));\n"
                "    return NULL;\n"
                "}\n"
                "static xvalue* xlir_xvalue_iterator_key_dynamic_xvalue(const xlir_xvalue_iterator* iterator)\n"
                "{\n"
                "    const XL6_ValueIteratorView* state = (xl6ValueIteratorRuntimeV2Get()->View)(iterator);\n"
                "    xvalue* value = NULL;\n"
                "    if (state == NULL || state->item == NULL) abort();\n"
                "    if (state->key.Type == 0) { if (state->ordinal > INT64_MAX) abort(); value = xrtValueInt((int64_t)state->ordinal); }\n"
                "    else if (state->key.Type == 1) { if (state->key.Index > INT64_MAX) abort(); value = xrtValueInt((int64_t)state->key.Index); }\n"
                "    else if (state->key.Type == 2) value = xrtValueInt(state->key.Integer);\n"
                "    else if (state->key.Type == 3) { xlir_string_view key = { state->key.String.Data, state->key.String.Size }; value = xrtValueString(key); }\n"
                "    else abort();\n"
                "    return value;\n"
                "}\n"
                "static xvalue* xlir_xvalue_iterator_dynamic_logical_xvalue(const xlir_xvalue_iterator* iterator)\n"
                "{\n"
                "    const XL6_ValueIteratorView* state = (xl6ValueIteratorRuntimeV2Get()->View)(iterator);\n"
                "    if (state == NULL || state->item == NULL) abort();\n"
                "    if (state->key.Type == 3) return xlir_xvalue_iterator_key_dynamic_xvalue(iterator);\n"
                "    return xlir_xvalue_iterator_item_xvalue_retain(iterator);\n"
                "}\n",
                out);
        if ( needs->needsXValueIteratorStringRuntime )
            xl6OutputPuts(
                "extern bool xrtValueGetString(const xvalue*, xlir_string_view*);\n"
                "extern char* xrtStrDupN(const char*, size_t);\n"
                "static xlir_string xlir_xvalue_iterator_item_string_copy(const xlir_xvalue_iterator* iterator)\n"
                "{\n"
                "    const XL6_ValueIteratorView* state = (xl6ValueIteratorRuntimeV2Get()->View)(iterator);\n"
                "    xlir_string_view value = { 0 };\n"
                "    if (state == NULL || state->item == NULL || !xrtValueGetString(state->item, &value)) abort();\n"
                "    return xlir_string_from_utf8(value.Data, value.Size);\n"
                "}\n"
                "static xlir_string xlir_xvalue_iterator_key_string_copy(const xlir_xvalue_iterator* iterator)\n"
                "{\n"
                "    const XL6_ValueIteratorView* state = (xl6ValueIteratorRuntimeV2Get()->View)(iterator);\n"
                "    if (state == NULL || state->key.Type != 3) abort();\n"
                "    return xlir_string_from_utf8(state->key.String.Data, state->key.String.Size);\n"
                "}\n",
                out);
        if ( needs->needsXValueIteratorNullableStringRuntime )
            xl6OutputPuts(
                "extern bool xrtValueGetString(const xvalue*, xlir_string_view*);\n"
                "extern char* xrtStrDupN(const char*, size_t);\n"
                    "static bool xlir_xvalue_project_nullable_string_copy(const xvalue* source, xlir_string* output)\n"
                    "{\n"
                    "    xlir_string_view value = { 0 }; *output = (xlir_string){0};\n"
                    "    if (source == NULL) return false;\n"
                    "    if (xrtValueType(source) == 0) return true;\n"
                    "    if (!xrtValueGetString(source, &value)) return false;\n"
                    "    *output = xlir_string_from_utf8(value.Data, value.Size);\n"
                    "    return output->Data != NULL;\n"
                    "}\n",
                out);
        if ( needs->needsXValueStringProjectionRuntime )
            xl6OutputPuts(
                "extern bool xrtValueGetString(const xvalue*, xlir_string_view*);\n"
                "extern char* xrtStrDupN(const char*, size_t);\n"
                "static xlir_string xlir_xvalue_project_string_copy(const xvalue* source)\n"
                "{\n"
                "    xlir_string_view value = { 0 };\n"
                "    if (source == NULL || xrtValueType(source) == 0 ||\n"
                "        !xrtValueGetString(source, &value)) return (xlir_string){0};\n"
                "    return xlir_string_from_utf8(value.Data, value.Size);\n"
                "}\n",
                out);
        if ( needs->needsXValueNativeHandleProjectionRuntime )
            xl6OutputPuts(
                "extern bool xrtValueGetPointer(const xvalue*, void**);\n"
                "static void* xlir_xvalue_project_native_handle_borrow(const xvalue* source, uint64_t typeId)\n"
                "{\n"
                "    void* handle = NULL;\n"
                "    if (source == NULL || typeId == 0 || xrtValueTypeId(source) != typeId ||\n"
                "        !xrtValueGetPointer(source, &handle)) abort();\n"
                "    return handle;\n"
                "}\n",
                out);
        if ( needs->needsXValueCollectionCheckedRebindRuntime )
            xl6OutputPuts(
                "extern xvalue* xl6CollectionCheckedTakeV1(xvalue*, int, unsigned int, bool, uint64_t);\n",
                out);
        if (needs->needsXValueRetainProjectionRuntime)
            xl6OutputPuts("extern xvalue* xl6ValueCheckedRetainV1(const xvalue*, unsigned int, bool, uint64_t);\n",out);
        if (needs->needsXValueScalarProjectionRuntime)
            xl6OutputPuts("extern bool xl6ValueReadF32V1(const xvalue*, uint64_t, float*);\n",out);
        if ( needs->needsXValueRuntime )
            xl6OutputPuts("extern xvalue* xrtValuePointer(void*);\n", out);
        if ( needs->needsXValueHandleRuntime )
            if ( !xl6IrCgenNeedsXrtDeclarations(needs) ) xl6OutputPuts(
                "typedef bool (*xlir_xvalue_handle_clone)(void*, void**, void*);\n"
                "typedef void (*xlir_xvalue_handle_drop)(void*, void*);\n"
                "typedef uint64_t (*xlir_xvalue_handle_hash)(void*, void*);\n"
                "typedef bool (*xlir_xvalue_handle_equal)(void*, void*, void*);\n"
                "typedef struct xvaluehandleops {\n"
                "    xlir_xvalue_handle_clone Clone;\n"
                "    xlir_xvalue_handle_drop Drop;\n"
                "    xlir_xvalue_handle_hash Hash;\n"
                "    xlir_xvalue_handle_equal Equal;\n"
                "} xvaluehandleops;\n"
                "extern xvalue* xrtValueHandleTake(void**, const xvaluehandleops*, void*);\n",
                out);
        if ( needs->needsXValueErrorBoxRuntime )
            xl6OutputPuts(
                "extern xvalue* xl6ErrorValueRefV1(const xerror*, uint64_t);\n",
                out);
        if ( needs->needsXValueErrorProjectionRuntime )
            xl6OutputPuts(
                "extern bool xl6ErrorValueReadRefV1(const xvalue*, uint64_t, xerror**);\n"
                "static xerror* xlir_xvalue_project_error_ref(const xvalue* value, uint64_t typeId)\n"
                "{\n"
                "    xerror* result = NULL;\n"
                "    if (!xl6ErrorValueReadRefV1(value, typeId, &result)) abort();\n"
                "    return result;\n"
                "}\n",
                out);
        if ( needs->needsXValueCountRuntime )
            xl6OutputPuts("extern size_t xrtValueCount(const xvalue*);\n"
                "#include <__xl_runtime/xl6_collection_count.h>\n", out);
        if (needs->needsListMethodsRuntime)
            xl6OutputPuts("#include <__xl_runtime/xl6_list_methods.h>\n",out);
        if ( needs->needsXValueSafeCountRuntime )
            xl6OutputPuts("#ifndef XLIR_XRT_DECLARATIONS\nextern int xrtValueType(const xvalue*);\n#endif\n"
                          "extern xvalue* xrtValueNull(void);\n"
                          "extern xvalue* xrtValueRetain(const xvalue*);\n"
                          "extern xvalue* xrtValueInt(int64_t);\n", out);
        if ( needs->needsXValueIntegerConvertRuntime )
            xl6OutputPuts("#ifndef XLIR_XRT_DECLARATIONS\nextern int xrtValueType(const xvalue*);\n#endif\n"
                          "extern bool xrtValueGetBool(const xvalue*, bool*);\n"
                          "extern bool xrtValueGetInt(const xvalue*, int64_t*);\n"
                          "extern bool xrtValueGetUInt(const xvalue*, uint64_t*);\n"
                          "extern bool xrtValueGetFloat(const xvalue*, double*);\n"
                          "extern bool xrtValueGetString(const xvalue*, xlir_string_view*);\n"
                          "extern char* xrtStrDupN(const char*, size_t);\n"
                          "extern void xrtFree(void*);\n", out);
        if ( needs->needsStringViews )
            xl6OutputPuts("extern xvalue* xrtValueString(xlir_string_view);\n", out);
        if ( needs->needsXValueFutureRuntime )
            xl6OutputPuts("extern xvalue* xrtValueFuture(xfuture*);\n", out);
        if ( needs->needsXValueArrayRuntime )
            xl6OutputPuts("extern xvalue* xrtValueArray(void);\n"
                           "#include <__xl_runtime/xl6_array_value_write.h>\n"
                           "extern bool xrtValueArrayAppendNew(xvalue*, xvalue*);\n"
                           "extern bool xrtValueArrayAppend(xvalue*, const xvalue*);\n", out);
        if (needs->needsXValueArrayOwnedReadRuntime)
            xl6OutputPuts("#include <__xl_runtime/xl6_array_value_read.h>\n",out);
        if ( needs->needsXValueArrayReadRuntime )
            xl6OutputPuts("extern size_t xrtValueCount(const xvalue*);\n"
                          "extern xvalue* xrtValueArrayAt(const xvalue*, int64_t);\n"
                          "extern xvalue* xrtValueArrayGet(const xvalue*, size_t);\n"
                          "static xvalue* xlir_xvalue_array_at_i64_or_null(const xvalue* array, int64_t index)\n"
                          "{\n"
                          "    if (array == NULL || xrtValueType(array) == 0) return xrtValueNull();\n"
                          "    size_t count = xrtValueCount(array); size_t resolved;\n"
                          "    if (index >= 0) {\n"
                          "        if ((uint64_t)index >= (uint64_t)count) return xrtValueNull();\n"
                          "        resolved = (size_t)index;\n"
                          "    } else {\n"
                          "        uint64_t offset = (uint64_t)(-(index + 1)) + 1u;\n"
                          "        if (offset > (uint64_t)count) return xrtValueNull();\n"
                          "        resolved = count - (size_t)offset;\n"
                          "    }\n"
                          "    return xrtValueArrayGet(array, resolved);\n"
                          "}\n", out);
        if (needs->needsXValueArrayIntegerReadRuntime || needs->needsXValueArrayU64ReadRuntime ||
            needs->needsXValueArrayBoolReadRuntime || needs->needsXValueArrayF64ReadRuntime)
            xl6OutputPuts("#include <__xl_runtime/xl6_array_scalar_read.h>\n", out);
        if ( needs->needsXValueArrayStringReadRuntime )
            xl6OutputPuts("#include <__xl_runtime/xl6_array_string_read.h>\n",out);
        if ( needs->needsXValueArraySetRuntime ) {
            xl6OutputPuts("#include <__xl_runtime/xl6_array_value_write.h>\n",out);
        }
        if ( needs->needsXValueIntMapRuntime )
            xl6OutputPuts("extern xvalue* xrtValueIntMap(void);\n"
                          "extern bool xrtValueIntMapSetNew(xvalue*, int64_t, xvalue*);\n", out);
        if ( needs->needsXValueIntMapReadRuntime )
            xl6OutputPuts("extern xvalue* xrtValueIntMapGet(const xvalue*, int64_t);\n", out);
        if ( needs->needsXValueIntMapIntegerReadRuntime )
            xl6OutputPuts("extern bool xrtValueGetInt(const xvalue*, int64_t*);\n"
                          "static int64_t xlir_xvalue_int_map_at_i64_integer(const xvalue* map, int64_t index)\n"
                          "{ xvalue* item = xrtValueIntMapGet(map, index); int64_t value = 0;\n"
                          "  if (item == NULL || !xrtValueGetInt(item, &value)) abort();\n"
                          "  return value; }\n", out);
        if ( needs->needsXValueIntMapU64ReadRuntime )
            xl6OutputPuts("extern bool xrtValueGetUInt(const xvalue*, uint64_t*);\n"
                          "static uint64_t xlir_xvalue_int_map_at_i64_u64(const xvalue* map, int64_t index)\n"
                          "{ xvalue* item = xrtValueIntMapGet(map, index); uint64_t value = 0;\n"
                          "  if (item == NULL || !xrtValueGetUInt(item, &value)) abort();\n"
                          "  return value; }\n", out);
        if ( needs->needsXValueIntMapBoolReadRuntime )
            xl6OutputPuts("extern bool xrtValueGetBool(const xvalue*, bool*);\n"
                          "static bool xlir_xvalue_int_map_at_i64_bool(const xvalue* map, int64_t index)\n"
                          "{ xvalue* item = xrtValueIntMapGet(map, index); bool value = false;\n"
                          "  if (item == NULL || !xrtValueGetBool(item, &value)) abort();\n"
                          "  return value; }\n", out);
        if ( needs->needsXValueIntMapF64ReadRuntime )
            xl6OutputPuts("extern bool xrtValueGetFloat(const xvalue*, double*);\n"
                          "static double xlir_xvalue_int_map_at_i64_f64(const xvalue* map, int64_t index)\n"
                          "{ xvalue* item = xrtValueIntMapGet(map, index); double value = 0.0;\n"
                          "  if (item == NULL || !xrtValueGetFloat(item, &value)) abort();\n"
                          "  return value; }\n", out);
        if ( needs->needsXValueIntMapStringReadRuntime )
            xl6OutputPuts("extern bool xrtValueGetString(const xvalue*, xlir_string_view*);\n"
                          "extern char* xrtStrDupN(const char*, size_t);\n"
                          "static xlir_string xlir_xvalue_int_map_at_i64_string_copy(const xvalue* map, int64_t index)\n"
                          "{ xvalue* item = xrtValueIntMapGet(map, index); xlir_string_view value = {0}; xlir_string copied;\n"
                          "  if (item == NULL || !xrtValueGetString(item, &value)) abort();\n"
                          "  copied = xlir_string_from_utf8(value.Data, value.Size); if (copied.Data == NULL) abort();\n"
                          "  return copied; }\n", out);
    }
    return !xl6OutputHasFailed(out);
}
