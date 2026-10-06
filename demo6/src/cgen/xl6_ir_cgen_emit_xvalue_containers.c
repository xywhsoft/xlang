static int xl6IrCgenEmitXValueContainerRuntime(
    XL6_Output* out, const XL6_IrModule* module,
    const XL6_IrCgenNeeds* needs, XL6_DiagList* diags)
{
    size_t i;
    size_t parameter;
    if ( needs->needsXValueRuntime || module->abi.entryKind == XL6_IR_MODULE_ENTRY_XLANG_MAIN ) {
        if ( needs->needsXValueSetRuntime )
            xl6OutputPuts("extern xvalue* xrtValueSet(void);\n"
                          "extern bool xrtValueSetAddNew(xvalue*, xvalue*);\n"
                          "extern bool xrtValueSetHas(const xvalue*, const xvalue*);\n", out);
        if ( needs->needsXValueCollectionRemoveRuntime )
            xl6OutputPuts(
                "extern bool xrtValueArrayRemove(xvalue*, size_t, size_t);\n"
                "extern bool xrtValueIntMapRemove(xvalue*, int64_t);\n"
                "extern bool xrtValueSetRemove(xvalue*, const xvalue*);\n"
                "extern bool xrtValueObjectRemove(xvalue*, xlir_string_view);\n",
                out);
        if ( needs->needsXValueObjectRuntime ) {
            if ( !needs->needsStringViews )
                xl6OutputPuts(
                    "extern xstrview xrtStrView(const char*);\n", out);
            xl6OutputPuts("extern xvalue* xrtValueObject(void);\n"
                           "extern xvalue* xrtValueObjectGet(const xvalue*, xlir_string_view);\n"
                           "extern bool xrtValueObjectSetNew(xvalue*, xlir_string_view, xvalue*);\n"
                           "static xvalue* xlir_xvalue_object_get_literal_or_null(const xvalue* object, xlir_string_view key)\n"
                           "{\n"
                           "    if (object == NULL || xrtValueType(object) == 0) return xrtValueNull();\n"
                           "    xvalue* item = xrtValueObjectGet(object, key);\n"
                           "    return item != NULL ? item : xrtValueNull();\n"
                           "}\n"
                           "static xvalue* xlir_xvalue_object_get_string_or_null(const xvalue* object, xlir_string_view key)\n"
                           "{\n"
                           "    if (object == NULL || xrtValueType(object) == 0) return xrtValueNull();\n"
                           "    xvalue* item = xrtValueObjectGet(object, key);\n"
                           "    return item != NULL ? item : xrtValueNull();\n"
                           "}\n", out);
            if ( needs->needsXValueSafeClassFieldRuntime )
                xl6OutputPuts(
                    "static xvalue* xlir_xvalue_safe_class_field_get_or_null(const xvalue* source, xlir_string_view key)\n"
                    "{\n"
                    "    xvalue* item;\n"
                    "    if (source == NULL || xrtValueType(source) == 0) return xrtValueNull();\n"
                    "    item = xrtValueObjectGet(source, key);\n"
                    "    return item != NULL ? item : xrtValueNull();\n"
                    "}\n",
                    out);
            if ( needs->needsXValueDynamicFieldRuntime ) {
                if ( needs->openDynamicField != NULL &&
                     needs->openDynamicHandleField != NULL ) {
                    xl6OutputPuts(
                    "static xrtdynamicfields* xlir_xvalue_dynamic_fields(const xvalue* source)\n"
                    "{\n"
                    "    const xvalue* storage;\n"
                    "    const xvalue* boxed;\n"
                    "    void* handle = NULL;\n"
                    "    if (source == NULL || xrtValueObjectGet(source, xrtStrView(\"$xlir.class.runtime-type\")) == NULL) return NULL;\n"
                    "    storage = xrtValueObjectGet(source, xrtStrView(",
                    out);
                xl6IrCgenEmitCStringLiteral(out, needs->openDynamicField->name);
                xl6OutputPuts(
                    "));\n"
                    "    if (storage == NULL) return NULL;\n"
                    "    boxed = xrtValueObjectGet(storage, xrtStrView(",
                    out);
                xl6IrCgenEmitCStringLiteral(
                    out, needs->openDynamicHandleField->name);
                xl6OutputPuts(
                    "));\n"
                    "    if (boxed == NULL || !xrtValueGetHandle(boxed, &handle, NULL, NULL)) return NULL;\n"
                    "    return (xrtdynamicfields*)handle;\n"
                    "}\n"
                    "static xvalue* xlir_xvalue_dynamic_field_find(const xvalue* source, xlir_string_view key)\n"
                    "{\n"
                    "    xvalue* declared = xrtValueObjectGet(source, key);\n"
                    "    xrtdynamicfields* fields;\n"
                    "    const xvalue* dynamic;\n"
                    "    if (declared != NULL) return declared;\n"
                    "    fields = xlir_xvalue_dynamic_fields(source);\n"
                    "    dynamic = fields != NULL ? xrtDynamicFieldsGet(fields, key) : NULL;\n"
                    "    return (xvalue*)dynamic;\n"
                    "}\n"
                    "static bool xlir_xvalue_dynamic_field_set_new(xvalue* target, xlir_string_view key, xvalue* item)\n"
                    "{\n"
                    "    xrtdynamicfields* fields;\n"
                    "    if (target == NULL || item == NULL) { xrtValueRelease(item); return false; }\n"
                    "    if (xrtValueObjectGet(target, key) != NULL)\n"
                    "        return xrtValueObjectSetNew(target, key, item);\n"
                    "    fields = xlir_xvalue_dynamic_fields(target);\n"
                    "    return fields != NULL\n"
                    "        ? xrtDynamicFieldsSetNew(fields, key, item)\n"
                    "        : xrtValueObjectSetNew(target, key, item);\n"
                    "}\n",
                        out);
                } else {
                    /* Erased `var` access has no nominal class-layout
                     * dependency.  When this module contains no open-object
                     * schema, its dynamic-field operation deterministically
                     * reduces to ordinary object storage. */
                    xl6OutputPuts(
                        "static xvalue* xlir_xvalue_dynamic_field_find(const xvalue* source, xlir_string_view key)\n"
                        "{\n"
                        "    return xrtValueObjectGet(source, key);\n"
                        "}\n"
                        "static bool xlir_xvalue_dynamic_field_set_new(xvalue* target, xlir_string_view key, xvalue* item)\n"
                        "{\n"
                        "    if (target == NULL || item == NULL) { xrtValueRelease(item); return false; }\n"
                        "    return xrtValueObjectSetNew(target, key, item);\n"
                        "}\n",
                        out);
                }
            }
            if ( needs->needsXValueDynamicFieldRuntime )
                xl6OutputPuts(
                    "static xvalue* xlir_xvalue_dynamic_field_get_or_null(const xvalue* source, xlir_string_view key)\n"
                    "{\n"
                    "    xvalue* item = xlir_xvalue_dynamic_field_find(source, key);\n"
                    "    return item != NULL ? item : xrtValueNull();\n"
                    "}\n", out);
            if ( needs->needsXValueClassRuntime )
                xl6OutputPuts("extern xvalue* xrtValueObjectLifo(void);\n", out);
            if ( needs->needsTaggedEnumConstruction )
                xl6OutputPuts(
                    "static xvalue* xlir_tagged_enum_new(int64_t tag)\n"
                    "{\n"
                    "    xvalue* object = xrtValueObjectLifo();\n"
                    "    if (object == NULL) return NULL;\n"
                    "    /* SetNew consumes the tag on success and failure. */\n"
                    "    if (!xrtValueObjectSetNew(object, xrtStrView(\"$xlir.enum.variant\"), xrtValueInt(tag))) {\n"
                    "        xrtValueRelease(object);\n"
                    "        return NULL;\n"
                    "    }\n"
                    "    return object;\n"
                    "}\n", out);
            if ( needs->needsXValueObjectLiteralI64Runtime )
                xl6OutputPuts("extern bool xrtValueGetInt(const xvalue*, int64_t*);\n"
                              "static int64_t xlir_xvalue_object_get_literal_i64(const xvalue* object, xlir_string_view key)\n"
                              "{\n"
                              "    xvalue* item = xrtValueObjectGet(object, key);\n"
                              "    int64_t value = 0;\n"
                              "    if (item == NULL || !xrtValueGetInt(item, &value)) {\n"
                              "        abort();\n"
                              "    }\n"
                              "    return value;\n"
                              "}\n", out);
            if ( needs->needsXValueObjectLiteralU64Runtime )
                xl6OutputPuts("extern bool xrtValueGetUInt(const xvalue*, uint64_t*);\n"
                              "static uint64_t xlir_xvalue_object_get_literal_u64(const xvalue* object, xlir_string_view key)\n"
                              "{\n"
                              "    xvalue* item = xrtValueObjectGet(object, key);\n"
                              "    uint64_t value = 0;\n"
                              "    if (item == NULL || !xrtValueGetUInt(item, &value)) abort();\n"
                              "    return value;\n"
                              "}\n", out);
            if ( needs->needsXValueObjectLiteralBoolRuntime )
                xl6OutputPuts("extern bool xrtValueGetBool(const xvalue*, bool*);\n"
                              "static bool xlir_xvalue_object_get_literal_bool(const xvalue* object, xlir_string_view key)\n"
                              "{\n"
                              "    xvalue* item = xrtValueObjectGet(object, key);\n"
                              "    bool value = false;\n"
                              "    if (item == NULL || !xrtValueGetBool(item, &value)) abort();\n"
                              "    return value;\n"
                              "}\n", out);
            if ( needs->needsXValueObjectLiteralF64Runtime )
                xl6OutputPuts("extern bool xrtValueGetFloat(const xvalue*, double*);\n"
                              "static double xlir_xvalue_object_get_literal_f64(const xvalue* object, xlir_string_view key)\n"
                              "{\n"
                              "    xvalue* item = xrtValueObjectGet(object, key);\n"
                              "    double value = 0.0;\n"
                              "    if (item == NULL || !xrtValueGetFloat(item, &value)) abort();\n"
                              "    return value;\n"
                              "}\n", out);
            if ( needs->needsXValueObjectLiteralTimeRuntime )
                xl6OutputPuts("extern bool xrtValueGetTime(const xvalue*, int64_t*);\n"
                              "static int64_t xlir_xvalue_object_get_literal_time(const xvalue* object, xlir_string_view key)\n"
                              "{\n"
                              "    xvalue* item = xrtValueObjectGet(object, key);\n"
                              "    int64_t value = 0;\n"
                              "    if (item == NULL || !xrtValueGetTime(item, &value)) abort();\n"
                              "    return value;\n"
                              "}\n", out);
            if ( needs->needsXValueObjectLiteralRawPointerRuntime )
                xl6OutputPuts("extern bool xrtValueGetPointer(const xvalue*, void**);\n"
                              "static void* xlir_xvalue_object_get_literal_raw_pointer(const xvalue* object, xlir_string_view key)\n"
                              "{\n"
                              "    xvalue* item = xrtValueObjectGet(object, key);\n"
                              "    void* value = NULL;\n"
                              "    if (item == NULL || !xrtValueGetPointer(item, &value)) abort();\n"
                              "    return value;\n"
                              "}\n", out);
            if ( needs->needsXValueObjectLiteralNativePointerRuntime )
                xl6OutputPuts("extern bool xrtValueGetPointer(const xvalue*, void**);\n"
                              "static void* xlir_xvalue_object_get_literal_native_pointer(const xvalue* object, xlir_string_view key)\n"
                              "{\n"
                              "    xvalue* item = xrtValueObjectGet(object, key);\n"
                              "    void* value = NULL;\n"
                              "    if (item == NULL || !xrtValueGetPointer(item, &value)) abort();\n"
                              "    return value;\n"
                              "}\n", out);
            if ( needs->needsXValueObjectLiteralNativeHandleRuntime )
                xl6OutputPuts("#ifndef XLIR_XRT_DECLARATIONS\n"
                              "typedef struct xvaluehandleops xvaluehandleops;\n"
                              "#endif\n"
                              "extern bool xrtValueGetHandle(const xvalue*, void**, const xvaluehandleops**, void**);\n"
                              "static void* xlir_xvalue_object_get_literal_native_handle(const xvalue* object, xlir_string_view key)\n"
                              "{\n"
                              "    xvalue* item = xrtValueObjectGet(object, key);\n"
                              "    void* value = NULL;\n"
                              "    if (item == NULL || !xrtValueGetHandle(item, &value, NULL, NULL)) abort();\n"
                              "    return value;\n"
                              "}\n", out);
            if ( needs->needsXValueObjectLiteralNativeHandleTakeRuntime )
                xl6OutputPuts("extern bool xrtValueTakeHandle(xvalue*, void**);\n"
                              "static void* xlir_xvalue_object_take_literal_native_handle(xvalue* object, xlir_string_view key)\n"
                              "{\n"
                              "    xvalue* item = xrtValueObjectGet(object, key);\n"
                              "    void* value = NULL;\n"
                              "    if (item == NULL || !xrtValueTakeHandle(item, &value) || value == NULL) abort();\n"
                              "    return value;\n"
                              "}\n", out);
            if ( needs->needsXValueObjectLiteralStringRuntime )
                xl6OutputPuts("extern bool xrtValueGetString(const xvalue*, xlir_string_view*);\n"
                              "extern char* xrtStrDupN(const char*, size_t);\n"
                              "static xlir_string xlir_xvalue_object_get_literal_string_copy(const xvalue* object, xlir_string_view key)\n"
                              "{ xvalue* item = xrtValueObjectGet(object, key); xlir_string_view value = {0};\n"
                              "  if (item == NULL || !xrtValueGetString(item, &value)) abort();\n"
                              "  return xlir_string_from_utf8(value.Data, value.Size); }\n", out);
        }
        if ( needs->needsNominalIdentityRuntime &&
             !xl6IrCgenEmitNominalIdentityRuntime(out, module, diags) )
            return 0;
        if ( needs->needsXValueDynamicKeyIndexReadRuntime )
            xl6OutputPuts(
                "extern bool xrtValueGetInt(const xvalue*, int64_t*);\n"
                "extern bool xrtValueGetString(const xvalue*, xlir_string_view*);\n"
                "static xvalue* xlir_xvalue_index_get_xvalue_key_or_null(const xvalue* collection, const xvalue* key)\n"
                "{\n"
                "    int collectionType = xrtValueType(collection); int keyType = xrtValueType(key);\n"
                "    int64_t index = 0; xlir_string_view text = {0}; xvalue* item;\n"
                "    if (keyType == 2 && xrtValueGetInt(key, &index)) {\n"
                "        if (collectionType == 9) return xlir_xvalue_array_at_i64_or_null(collection, index);\n"
                "        else if (collectionType == 10) item = xrtValueIntMapGet(collection, index);\n"
                "        else return xrtValueNull();\n"
                "        return item != NULL ? item : xrtValueNull();\n"
                "    }\n"
                "    if (keyType == 4 && xrtValueGetString(key, &text)) {\n"
                "        if (collectionType != 12) return xrtValueNull();\n"
                "        item = xrtValueObjectGet(collection, text);\n"
                "        return item != NULL ? item : xrtValueNull();\n"
                "    }\n"
                "    return xrtValueNull();\n"
                "}\n", out);
        if ( needs->needsXValueDynamicKeyIndexWriteRuntime )
            xl6OutputPuts("#include <__xl_runtime/xl6_array_value_write.h>\n",out);
        if ( needs->needsXValueContainsRuntime )
            xl6OutputPuts(
                "typedef struct xvalueiter xvalueiter;\n"
                "typedef struct xvaluekey xvaluekey;\n"
                "extern bool xrtValueGetString(const xvalue*, xlir_string_view*);\n"
                "extern bool xrtValueSetHas(const xvalue*, const xvalue*);\n"
                "extern bool xrtValueObjectHas(const xvalue*, xlir_string_view);\n"
                "extern bool xrtStrContains(xlir_string_view, xlir_string_view);\n"
                "extern bool xrtValueEqual(const xvalue*, const xvalue*);\n"
                "extern xvalueiter* xrtValueIterCreate(const xvalue*);\n"
                "extern xvalue* xrtValueIterNext(xvalueiter*, xvaluekey*);\n"
                "extern void xrtValueIterDestroy(xvalueiter*);\n"
                "static bool xlir_xvalue_contains(const xvalue* values, const xvalue* item)\n"
                "{\n"
                "    int valuesType = xrtValueType(values); int itemType = xrtValueType(item);\n"
                "    xlir_string_view valuesText = {0}; xlir_string_view itemText = {0};\n"
                "    xvalueiter* iterator; xvalue* current; bool found = false;\n"
                "    if (valuesType == 11) return xrtValueSetHas(values, item);\n"
                "    if (valuesType == 12) {\n"
                "        if (itemType != 4 || !xrtValueGetString(item, &itemText)) return false;\n"
                "        return xrtValueObjectHas(values, itemText);\n"
                "    }\n"
                "    if (valuesType == 4) {\n"
                "        if (itemType != 4 || !xrtValueGetString(values, &valuesText) ||\n"
                "            !xrtValueGetString(item, &itemText)) return false;\n"
                "        return xrtStrContains(valuesText, itemText);\n"
                "    }\n"
                "    if (valuesType != 9 && valuesType != 10) return false;\n"
                "    iterator = xrtValueIterCreate(values);\n"
                "    if (iterator == NULL) return false;\n"
                "    while ((current = xrtValueIterNext(iterator, NULL)) != NULL) {\n"
                "        if (xrtValueEqual(current, item)) { found = true; break; }\n"
                "    }\n"
                "    xrtValueIterDestroy(iterator);\n"
                "    return found;\n"
                "}\n", out);
        if ( needs->needsXValueArrayMethodsRuntime )
            xl6OutputPuts("#include <__xl_runtime/xl6_array_methods.h>\n",out);
        if ( needs->needsXValueTypedCollectionBaseRuntime )
            xl6OutputPuts(
                "extern bool xrtValueClear(xvalue*);\n"
                "extern size_t xrtValueCapacity(const xvalue*);\n"
                "extern bool xrtValueReserve(xvalue*, size_t);\n"
                "extern bool xrtValueTrim(xvalue*);\n"
                "extern size_t xrtValueIntMapTrim(xvalue*, size_t);\n"
                "extern bool xrtValueArraySwap(xvalue*, size_t, size_t);\n"
                "extern bool xrtValueArrayInsert(xvalue*, size_t, const xvalue*);\n"
                "extern bool xrtValueSetIsDisjoint(const xvalue*, const xvalue*);\n",
                out);
        if ( needs->needsXValueTypedArrayResizeRuntime )
            xl6OutputPuts(
                "extern bool xl6ArrayResizeDefaultV1(xvalue*, int64_t, unsigned int);\n",
                out);
        if ( needs->needsXValueTypedCollectionTakeRuntime ) {
            xl6OutputPrintf(out,
                "enum xlir_typed_collection_abi {\n"
                "    XLIR_TYPED_COLLECTION_ARRAY = %d,\n"
                "    XLIR_TYPED_COLLECTION_LIST = %d,\n"
                "    XLIR_TYPED_COLLECTION_DICT = %d\n"
                "};\n",
                (int)XL6_IR_COLLECTION_ARRAY,
                (int)XL6_IR_COLLECTION_LIST,
                (int)XL6_IR_COLLECTION_DICT);
            xl6OutputPuts(
                "extern xvalue* xrtValueArrayTake(xvalue*, size_t);\n"
                "extern xvalue* xrtValueIntMapTake(xvalue*, int64_t);\n"
                "extern xvalue* xrtValueObjectTake(xvalue*, xlir_string_view);\n"
                "static xvalue* xlir_xvalue_typed_collection_take_raw(\n"
                "    xvalue* collection, unsigned int collectionAbi,\n"
                "    int64_t key, xlir_string text)\n"
                "{\n"
                "    switch (collectionAbi) {\n"
                "    case XLIR_TYPED_COLLECTION_ARRAY:\n"
                "        return key >= 0 ? xrtValueArrayTake(collection, (size_t)key) : NULL;\n"
                "    case XLIR_TYPED_COLLECTION_LIST:\n"
                "        return xrtValueIntMapTake(collection, key);\n"
                "    case XLIR_TYPED_COLLECTION_DICT:\n"
                "        return text.Data != NULL ? xrtValueObjectTake(collection, text) : NULL;\n"
                "    default:\n"
                "        return NULL;\n"
                "    }\n"
                "}\n",
                out);
        }
        if ( needs->needsXValueTypedArrayPopBoolRuntime )
            xl6OutputPuts(
                "extern xvalue* xrtValueArrayPop(xvalue*);\n"
                "extern bool xrtValueGetBool(const xvalue*, bool*);\n"
                "extern void xrtValueRelease(xvalue*);\n"
                "static bool xlir_xvalue_typed_array_pop_bool(xvalue* array)\n"
                "{ xvalue* item = xrtValueArrayPop(array); bool value = false;\n"
                "  if (item != NULL) { (void)xrtValueGetBool(item, &value); xrtValueRelease(item); }\n"
                "  return value; }\n",
                out);
        if ( needs->needsXValueTypedArrayPopI64Runtime )
            xl6OutputPuts(
                "extern xvalue* xrtValueArrayPop(xvalue*);\n"
                "extern bool xrtValueGetInt(const xvalue*, int64_t*);\n"
                "extern void xrtValueRelease(xvalue*);\n"
                "static int64_t xlir_xvalue_typed_array_pop_i64(xvalue* array)\n"
                "{ xvalue* item = xrtValueArrayPop(array); int64_t value = 0;\n"
                "  if (item != NULL) { (void)xrtValueGetInt(item, &value); xrtValueRelease(item); }\n"
                "  return value; }\n",
                out);
        if ( needs->needsXValueTypedArrayPopU64Runtime )
            xl6OutputPuts(
                "extern xvalue* xrtValueArrayPop(xvalue*);\n"
                "extern bool xrtValueGetUInt(const xvalue*, uint64_t*);\n"
                "extern void xrtValueRelease(xvalue*);\n"
                "static uint64_t xlir_xvalue_typed_array_pop_u64(xvalue* array)\n"
                "{ xvalue* item = xrtValueArrayPop(array); uint64_t value = 0;\n"
                "  if (item != NULL) { (void)xrtValueGetUInt(item, &value); xrtValueRelease(item); }\n"
                "  return value; }\n",
                out);
        if ( needs->needsXValueTypedArrayPopF64Runtime )
            xl6OutputPuts(
                "extern xvalue* xrtValueArrayPop(xvalue*);\n"
                "extern bool xrtValueGetFloat(const xvalue*, double*);\n"
                "extern void xrtValueRelease(xvalue*);\n"
                "static double xlir_xvalue_typed_array_pop_f64(xvalue* array)\n"
                "{ xvalue* item = xrtValueArrayPop(array); double value = 0.0;\n"
                "  if (item != NULL) { (void)xrtValueGetFloat(item, &value); xrtValueRelease(item); }\n"
                "  return value; }\n",
                out);
        if ( needs->needsXValueTypedArrayPopStringRuntime )
            xl6OutputPuts(
                "extern xvalue* xrtValueArrayPop(xvalue*);\n"
                "extern bool xrtValueGetString(const xvalue*, xlir_string_view*);\n"
                "extern char* xrtStrDupN(const char*, size_t);\n"
                "extern void xrtValueRelease(xvalue*);\n"
                "static xlir_string xlir_xvalue_typed_array_pop_string(xvalue* array)\n"
                "{ xvalue* item = xrtValueArrayPop(array); xlir_string_view text = {0}; xlir_string value;\n"
                "  if (item != NULL && xrtValueGetString(item, &text)) value = xlir_string_from_utf8(text.Data, text.Size);\n"
                "  else value = xlir_string_from_utf8(\"\", 0u);\n"
                "  if (item != NULL) xrtValueRelease(item);\n"
                "  return value; }\n",
                out);
        if ( needs->needsXValueTypedArrayPopXValueRuntime )
            xl6OutputPuts(
                "extern xvalue* xrtValueArrayPop(xvalue*);\n"
                "extern xvalue* xrtValueNull(void);\n"
                "static xvalue* xlir_xvalue_typed_array_pop_xvalue(xvalue* array)\n"
                "{ xvalue* item = xrtValueArrayPop(array); return item != NULL ? item : xrtValueNull(); }\n",
                out);
        if ( needs->needsXValueTypedCollectionTakeBoolRuntime )
            xl6OutputPuts(
                "extern bool xrtValueGetBool(const xvalue*, bool*);\n"
                "extern void xrtValueRelease(xvalue*);\n"
                "static bool xlir_xvalue_typed_collection_take_bool(\n"
                "    xvalue* collection, unsigned int abi, int64_t key, xlir_string text)\n"
                "{ xvalue* item = xlir_xvalue_typed_collection_take_raw(collection, abi, key, text); bool value = false;\n"
                "  if (item != NULL) { (void)xrtValueGetBool(item, &value); xrtValueRelease(item); }\n"
                "  return value; }\n",
                out);
        if ( needs->needsXValueTypedCollectionTakeI64Runtime )
            xl6OutputPuts(
                "extern bool xrtValueGetInt(const xvalue*, int64_t*);\n"
                "extern void xrtValueRelease(xvalue*);\n"
                "static int64_t xlir_xvalue_typed_collection_take_i64(\n"
                "    xvalue* collection, unsigned int abi, int64_t key, xlir_string text)\n"
                "{ xvalue* item = xlir_xvalue_typed_collection_take_raw(collection, abi, key, text); int64_t value = 0;\n"
                "  if (item != NULL) { (void)xrtValueGetInt(item, &value); xrtValueRelease(item); }\n"
                "  return value; }\n",
                out);
        if ( needs->needsXValueTypedCollectionTakeU64Runtime )
            xl6OutputPuts(
                "extern bool xrtValueGetUInt(const xvalue*, uint64_t*);\n"
                "extern void xrtValueRelease(xvalue*);\n"
                "static uint64_t xlir_xvalue_typed_collection_take_u64(\n"
                "    xvalue* collection, unsigned int abi, int64_t key, xlir_string text)\n"
                "{ xvalue* item = xlir_xvalue_typed_collection_take_raw(collection, abi, key, text); uint64_t value = 0;\n"
                "  if (item != NULL) { (void)xrtValueGetUInt(item, &value); xrtValueRelease(item); }\n"
                "  return value; }\n",
                out);
        if ( needs->needsXValueTypedCollectionTakeF64Runtime )
            xl6OutputPuts(
                "extern bool xrtValueGetFloat(const xvalue*, double*);\n"
                "extern void xrtValueRelease(xvalue*);\n"
                "static double xlir_xvalue_typed_collection_take_f64(\n"
                "    xvalue* collection, unsigned int abi, int64_t key, xlir_string text)\n"
                "{ xvalue* item = xlir_xvalue_typed_collection_take_raw(collection, abi, key, text); double value = 0.0;\n"
                "  if (item != NULL) { (void)xrtValueGetFloat(item, &value); xrtValueRelease(item); }\n"
                "  return value; }\n",
                out);
        if ( needs->needsXValueTypedCollectionTakeStringRuntime )
            xl6OutputPuts(
                "extern bool xrtValueGetString(const xvalue*, xlir_string_view*);\n"
                "extern char* xrtStrDupN(const char*, size_t);\n"
                "extern void xrtValueRelease(xvalue*);\n"
                "static xlir_string xlir_xvalue_typed_collection_take_string(\n"
                "    xvalue* collection, unsigned int abi, int64_t key, xlir_string text)\n"
                "{ xvalue* item = xlir_xvalue_typed_collection_take_raw(collection, abi, key, text); xlir_string_view valueView = {0}; xlir_string value;\n"
                "  if (item != NULL && xrtValueGetString(item, &valueView)) value = xlir_string_from_utf8(valueView.Data, valueView.Size);\n"
                "  else value = xlir_string_from_utf8(\"\", 0u);\n"
                "  if (item != NULL) xrtValueRelease(item);\n"
                "  return value; }\n",
                out);
        if ( needs->needsXValueTypedCollectionTakeXValueRuntime )
            xl6OutputPuts(
                "extern xvalue* xrtValueNull(void);\n"
                "static xvalue* xlir_xvalue_typed_collection_take_xvalue(\n"
                "    xvalue* collection, unsigned int abi, int64_t key, xlir_string text)\n"
                "{ xvalue* item = xlir_xvalue_typed_collection_take_raw(collection, abi, key, text);\n"
                "  return item != NULL ? item : xrtValueNull(); }\n",
                out);
        if ( needs->needsXValueDynamicCollectionRuntime )
            xl6OutputPuts(
                "enum xlir_dynamic_collection_operation {\n"
                "    XLIR_DYNAMIC_COLLECTION_PUSH = 1,\n"
                "    XLIR_DYNAMIC_COLLECTION_INSERT = 2,\n"
                "    XLIR_DYNAMIC_COLLECTION_REMOVE = 3,\n"
                "    XLIR_DYNAMIC_COLLECTION_REMOVE_VALUE = 4,\n"
                "    XLIR_DYNAMIC_COLLECTION_POP = 5,\n"
                "    XLIR_DYNAMIC_COLLECTION_TAKE = 6,\n"
                "    XLIR_DYNAMIC_COLLECTION_CLEAR = 7,\n"
                "    XLIR_DYNAMIC_COLLECTION_RESERVE = 8,\n"
                "    XLIR_DYNAMIC_COLLECTION_SWAP = 9,\n"
                "    XLIR_DYNAMIC_COLLECTION_ADD = 10,\n"
                "    XLIR_DYNAMIC_COLLECTION_CONTAINS = 11,\n"
                "    XLIR_DYNAMIC_COLLECTION_INDEX_OF = 12\n"
                "};\n"
                "extern xvalue* xrtValueNull(void);\n"
                "extern xvalue* xrtValueBool(bool);\n"
                "extern xvalue* xrtValueInt(int64_t);\n"
                "#ifndef XLIR_XRT_DECLARATIONS\nextern int xrtValueType(const xvalue*);\n#endif\n"
                "extern bool xrtValueGetInt(const xvalue*, int64_t*);\n"
                "extern bool xrtValueGetString(const xvalue*, xlir_string_view*);\n"
                "extern bool xrtValueEqual(const xvalue*, const xvalue*);\n"
                "extern void xrtValueRelease(xvalue*);\n"
                "extern size_t xrtValueCount(const xvalue*);\n"
                "extern xvalue* xrtValueArrayGet(const xvalue*, size_t);\n"
                "extern bool xrtValueArrayAppendNew(xvalue*, xvalue*);\n"
                "extern bool xrtValueArrayInsertNew(xvalue*, size_t, xvalue*);\n"
                "extern bool xrtValueArrayRemove(xvalue*, size_t, size_t);\n"
                "extern xvalue* xrtValueArrayTake(xvalue*, size_t);\n"
                "extern xvalue* xrtValueArrayPop(xvalue*);\n"
                "extern bool xrtValueArraySwap(xvalue*, size_t, size_t);\n"
                "extern bool xrtValueIntMapRemove(xvalue*, int64_t);\n"
                "extern xvalue* xrtValueIntMapTake(xvalue*, int64_t);\n"
                "extern bool xrtValueSetAddNew(xvalue*, xvalue*);\n"
                "extern bool xrtValueSetRemove(xvalue*, const xvalue*);\n"
                "extern bool xrtValueObjectRemove(xvalue*, xlir_string_view);\n"
                "extern xvalue* xrtValueObjectTake(xvalue*, xlir_string_view);\n"
                "extern bool xrtValueReserve(xvalue*, size_t);\n"
                "extern bool xrtValueClear(xvalue*);\n"
                "static xvalue* xlir_xvalue_dynamic_collection_method(\n"
                "    xvalue* collection, unsigned int operation, xvalue* arg0, xvalue* arg1)\n"
                "{\n"
                "    int type = xrtValueType(collection); int64_t first = 0; int64_t second = 0;\n"
                "    xlir_string_view text = {0}; bool ok = false; xvalue* value = NULL;\n"
                "    int returnsValue = 0; int returnsInteger = 0; int64_t integerResult = -1;\n"
                "    size_t index; size_t count;\n"
                "    switch (operation) {\n"
                "    case XLIR_DYNAMIC_COLLECTION_PUSH:\n"
                "        if (type == 9 && arg0 != NULL) { ok = xrtValueArrayAppendNew(collection, arg0); arg0 = NULL; }\n"
                "        break;\n"
                "    case XLIR_DYNAMIC_COLLECTION_INSERT:\n"
                "        if (type == 9 && arg0 != NULL && arg1 != NULL &&\n"
                "            xrtValueGetInt(arg0, &first) && first >= 0) {\n"
                "            ok = xrtValueArrayInsertNew(collection, (size_t)first, arg1); arg1 = NULL;\n"
                "        }\n"
                "        break;\n"
                "    case XLIR_DYNAMIC_COLLECTION_REMOVE:\n"
                "        if (arg0 == NULL) break;\n"
                "        if (type == 9 && xrtValueGetInt(arg0, &first) && first >= 0) {\n"
                "            count = 1u;\n"
                "            if (arg1 != NULL && (!xrtValueGetInt(arg1, &second) || second < 0)) break;\n"
                "            if (arg1 != NULL) count = (size_t)second;\n"
                "            ok = xrtValueArrayRemove(collection, (size_t)first, count);\n"
                "        } else if (arg1 == NULL && type == 10 && xrtValueGetInt(arg0, &first)) {\n"
                "            ok = xrtValueIntMapRemove(collection, first);\n"
                "        } else if (arg1 == NULL && type == 11) {\n"
                "            ok = xrtValueSetRemove(collection, arg0);\n"
                "        } else if (arg1 == NULL && type == 12 && xrtValueGetString(arg0, &text)) {\n"
                "            ok = xrtValueObjectRemove(collection, text);\n"
                "        }\n"
                "        break;\n"
                "    case XLIR_DYNAMIC_COLLECTION_REMOVE_VALUE:\n"
                "        if (type == 9 && arg0 != NULL) {\n"
                "            count = xrtValueCount(collection);\n"
                "            for (index = 0; index < count; ++index) {\n"
                "                value = xrtValueArrayGet(collection, index);\n"
                "                if (value != NULL && xrtValueEqual(value, arg0)) {\n"
                "                    ok = xrtValueArrayRemove(collection, index, 1u); break;\n"
                "                }\n"
                "            }\n"
                "            value = NULL;\n"
                "        }\n"
                "        break;\n"
                "    case XLIR_DYNAMIC_COLLECTION_POP:\n"
                "        returnsValue = 1; if (type == 9) value = xrtValueArrayPop(collection);\n"
                "        break;\n"
                "    case XLIR_DYNAMIC_COLLECTION_TAKE:\n"
                "        returnsValue = 1;\n"
                "        if (arg0 != NULL && type == 9 && xrtValueGetInt(arg0, &first) && first >= 0)\n"
                "            value = xrtValueArrayTake(collection, (size_t)first);\n"
                "        else if (arg0 != NULL && type == 10 && xrtValueGetInt(arg0, &first))\n"
                "            value = xrtValueIntMapTake(collection, first);\n"
                "        else if (arg0 != NULL && type == 12 && xrtValueGetString(arg0, &text))\n"
                "            value = xrtValueObjectTake(collection, text);\n"
                "        break;\n"
                "    case XLIR_DYNAMIC_COLLECTION_CLEAR:\n"
                "        if (type >= 9 && type <= 12) ok = xrtValueClear(collection);\n"
                "        break;\n"
                "    case XLIR_DYNAMIC_COLLECTION_RESERVE:\n"
                "        if (type == 9 && arg0 != NULL && xrtValueGetInt(arg0, &first) && first >= 0)\n"
                "            ok = xrtValueReserve(collection, (size_t)first);\n"
                "        break;\n"
                "    case XLIR_DYNAMIC_COLLECTION_SWAP:\n"
                "        if (type == 9 && arg0 != NULL && arg1 != NULL &&\n"
                "            xrtValueGetInt(arg0, &first) && xrtValueGetInt(arg1, &second) &&\n"
                "            first >= 0 && second >= 0)\n"
                "            ok = xrtValueArraySwap(collection, (size_t)first, (size_t)second);\n"
                "        break;\n"
                "    case XLIR_DYNAMIC_COLLECTION_ADD:\n"
                "        if (type == 11 && arg0 != NULL) { ok = xrtValueSetAddNew(collection, arg0); arg0 = NULL; }\n"
                "        break;\n"
                "    case XLIR_DYNAMIC_COLLECTION_CONTAINS:\n"
                "        if (arg0 != NULL) ok = xlir_xvalue_contains(collection, arg0);\n"
                "        break;\n"
                "    case XLIR_DYNAMIC_COLLECTION_INDEX_OF:\n"
                "        returnsInteger = 1;\n"
                "        if (type == 9 && arg0 != NULL) {\n"
                "            count = xrtValueCount(collection);\n"
                "            for (index = 0; index < count; ++index) {\n"
                "                value = xrtValueArrayGet(collection, index);\n"
                "                if (value != NULL && xrtValueEqual(value, arg0)) { integerResult = (int64_t)index; break; }\n"
                "            }\n"
                "            value = NULL;\n"
                "        }\n"
                "        break;\n"
                "    default: break;\n"
                "    }\n"
                "    if (arg0 != NULL) xrtValueRelease(arg0);\n"
                "    if (arg1 != NULL) xrtValueRelease(arg1);\n"
                "    if (returnsValue) return value != NULL ? value : xrtValueNull();\n"
                "    if (returnsInteger) return xrtValueInt(integerResult);\n"
                "    return xrtValueBool(ok);\n"
                "}\n", out);
    }
    return !xl6OutputHasFailed(out);
}
