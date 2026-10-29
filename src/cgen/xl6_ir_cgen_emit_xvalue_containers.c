static int xl6IrCgenEmitXValueContainerRuntime(
    XL6_Output* out, const XL6_IrModule* module,
    const XL6_IrCgenNeeds* needs, XL6_DiagList* diags)
{
    size_t i;
    size_t parameter;
    if(needs->namedValueOperations&1u) xl6OutputPuts("#include <__xl_runtime/xl6_value_named_read.h>\n",out);
    if(needs->namedValueOperations&2u) xl6OutputPuts("#include <__xl_runtime/xl6_value_named_write.h>\n",out);
    if ( needs->needsXValueRuntime || module->abi.entryKind == XL6_IR_MODULE_ENTRY_XLANG_MAIN ) {
        if ( needs->needsXValueCollectionRemoveRuntime )
            xl6OutputPuts("#include <__xl_runtime/xl6_array_methods.h>\n"
                "extern bool xrtValueArrayRemove(xvalue*, size_t, size_t);\n"
                "extern bool xrtValueIntMapRemove(xvalue*, int64_t);\n"
                "extern bool xrtValueObjectRemove(xvalue*, xlir_string_view);\n",
                out);
        if ( needs->needsXValueObjectRuntime ) {
            if ( !needs->needsStringViews )
                xl6OutputPuts(
                    "extern xstrview xrtStrView(const char*);\n", out);
            xl6OutputPuts("extern xvalue* xrtValueObject(void);\n"
                           "extern xvalue* xrtValueObjectGet(const xvalue*, xlir_string_view);\n"
                           "extern bool xrtValueObjectSetNewPostCommit(xvalue*, xlir_string_view, xvalue*);\n"
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
                xl6OutputPuts(
                    "#include <__xl_runtime/xl6_value_object_fields.h>\n"
                    "static xvalue* xlir_xvalue_dynamic_field_find(const xvalue* source, xlir_string_view key)\n"
                    "{\n"
                    "    return xl6ValueObjectFieldFindV1(source, key.Data, key.Size);\n"
                    "}\n"
                    "static bool xlir_xvalue_dynamic_field_set_new(xvalue* target, xlir_string_view key, xvalue* item)\n"
                    "{\n"
                    "    return xl6ValueObjectFieldSetTakeV1(target, key.Data, key.Size, item);\n"
                    "}\n", out);
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
            xl6OutputPuts("#include <__xl_runtime/xl6_value_contains.h>\n"
                "#include <__xl_runtime/xl6_array_methods.h>\n",out);
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
                "extern bool xrtValueArrayInsert(xvalue*, size_t, const xvalue*);\n",
                out);
        if ( needs->needsXValueTypedArrayResizeRuntime )
            xl6OutputPuts("#include <__xl_runtime/xl6_array_default.h>\n",out);
        if ( needs->needsArrayExtractRuntime )
            xl6OutputPuts("#include <__xl_runtime/xl6_array_extract.h>\n",out);
        if ( needs->needsXValueDynamicCollectionRuntime )
            xl6OutputPuts("#include <__xl_runtime/xl6_dynamic_collection.h>\n",out);
    }
    return !xl6OutputHasFailed(out);
}
