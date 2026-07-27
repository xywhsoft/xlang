static void xl6CgenEmitXValueTypedContainerRuntime(XL6_CgenContext* ctx)
{
    XL6_Output* out = ctx->out;

    xl6OutputPuts("static xvalue xl6_xrt_typed_box_or_null(const xrt_type_desc* type, const void* item)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( item == 0 ) {\n", out);
    xl6OutputPuts("        return xvoCreateNull();\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    {\n", out);
    xl6OutputPuts("        xvalue boxed = xrtTypeBoxValue(type, item);\n", out);
    xl6OutputPuts("        return boxed != 0 ? boxed : xvoCreateNull();\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("}\n\n", out);

    /*
        静态泛型容器进入 var 时保存同一个 XRT 句柄。
        xvalue 只负责持有句柄和类型描述，真正的数据与生命周期仍由 XRT 管理。
    */
    xl6OutputPuts("static unsigned int xl6_xrt_typed_container_kind(xvalue value)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    const xrt_type_desc* type;\n", out);
    xl6OutputPuts("    if ( value == 0 || xvoType(value) != XL6_XVO_DT_NATIVE ) { return XL6_XRT_TYPE_KIND_INVALID; }\n", out);
    xl6OutputPuts("    type = xvoTypeDesc(value);\n", out);
    xl6OutputPuts("    if ( type == 0 || type->Kind < XL6_XRT_TYPE_KIND_ARRAY || type->Kind > XL6_XRT_TYPE_KIND_DICT ) {\n", out);
    xl6OutputPuts("        return XL6_XRT_TYPE_KIND_INVALID;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return type->Kind;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static const xrt_type_desc* xl6_xrt_typed_container_item_type(xvalue value)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    const xrt_type_desc* type = value != 0 ? xvoTypeDesc(value) : 0;\n", out);
    xl6OutputPuts("    const xrt_generic_type* generic = type != 0 ? (const xrt_generic_type*)type->Extra : 0;\n", out);
    xl6OutputPuts("    return generic != 0 && generic->ArgCount == 1u ? generic->Args[0] : 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static xvalue xl6_xrt_typed_container_box_ref(const xrt_type_desc* type, void* handle)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    void* owned = 0;\n", out);
    xl6OutputPuts("    xvalue result;\n", out);
    xl6OutputPuts("    if ( handle == 0 ) { return xvoCreateNull(); }\n", out);
    xl6OutputPuts("    if ( type == 0 || type->Ops == 0 || type->Ops->copy == 0 || type->Ops->drop == 0 ) {\n", out);
    xl6OutputPuts("        return xvoCreateNull();\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    type->Ops->copy(&owned, &handle);\n", out);
    xl6OutputPuts("    if ( owned == 0 ) { return xvoCreateNull(); }\n", out);
    xl6OutputPuts("    result = xvoCreateHandle(type, owned, XL6_XRT_HANDLE_FLAG_OWNED | XL6_XRT_HANDLE_FLAG_NULLABLE);\n", out);
    xl6OutputPuts("    if ( result == 0 ) { type->Ops->drop(&owned); return xvoCreateNull(); }\n", out);
    xl6OutputPuts("    return result;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("typedef xvalue (*xl6_xrt_typed_item_box_fn)(void* item);\n\n", out);

    xl6OutputPuts("static void* xl6_xrt_typed_require_item(void* item, const xrt_type_desc* type, const char* file, int line, int column)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( item == 0 ) {\n", out);
    xl6OutputPuts("        size_t size = type != 0 && type->Size > 0 ? type->Size : sizeof(void*);\n", out);
    xl6OutputPuts("        void* fallback = xrtTempMemory(size);\n", out);
    xl6OutputPuts("        if ( fallback == 0 ) { xl6_runtime_fatal(2); }\n", out);
    xl6OutputPuts("        memset(fallback, 0, size);\n", out);
    xl6OutputPuts("        xl6_error_raise(\"RangeError\", \"typed container index is out of range\", file, line, column);\n", out);
    xl6OutputPuts("        return fallback;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return item;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static bool xl6_xrt_typed_array_resolve_index(xtarray values, long long index, unsigned int* resolved)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    unsigned int count;\n", out);
    xl6OutputPuts("    if ( values == 0 || resolved == 0 ) { return false; }\n", out);
    xl6OutputPuts("    if ( index < 0 ) {\n", out);
    xl6OutputPuts("        count = xrtTypedArrayCount(values);\n", out);
    xl6OutputPuts("        if ( index < -(long long)count ) { return false; }\n", out);
    xl6OutputPuts("        index += (long long)count;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( index < 0 || (unsigned long long)index > 0xFFFFFFFFULL ) { return false; }\n", out);
    xl6OutputPuts("    *resolved = (unsigned int)index;\n", out);
    xl6OutputPuts("    return true;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static void* xl6_xrt_typed_array_get_at(xtarray values, long long index)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    unsigned int resolved;\n", out);
    xl6OutputPuts("    return xl6_xrt_typed_array_resolve_index(values, index, &resolved) ? xrtTypedArrayGet(values, resolved + 1u) : 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static xvalue xl6_xrt_typed_array_to_xvalue_map(xtarray values, xl6_xrt_typed_item_box_fn box, bool classHandle)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xvalue result = xvoCreateArray();\n", out);
    xl6OutputPuts("    unsigned int count = xrtTypedArrayCount(values);\n", out);
    xl6OutputPuts("    unsigned int i;\n", out);
    xl6OutputPuts("    if ( result == 0 ) {\n", out);
    xl6OutputPuts("        return xvoCreateNull();\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    for ( i = 0; i < count; ++i ) {\n", out);
    xl6OutputPuts("        void* rawItem = xl6_xrt_typed_array_get_at(values, i);\n", out);
    xl6OutputPuts("        void* boxItem = rawItem;\n", out);
    xl6OutputPuts("        if ( classHandle && rawItem != 0 ) {\n", out);
    xl6OutputPuts("            xvalue handle = *(xvalue*)rawItem;\n", out);
    xl6OutputPuts("            boxItem = handle != 0 && !xvoIsNull(handle) ? xvoGetRecordData(handle) : 0;\n", out);
    xl6OutputPuts("        }\n", out);
    xl6OutputPuts("        xvalue item = boxItem != 0 && box != 0 ? box(boxItem) : xvoCreateNull();\n", out);
    xl6OutputPuts("        if ( !xvoArrayAppendValue(result, item, true) && item != 0 ) {\n", out);
    xl6OutputPuts("            xvoUnref(item);\n", out);
    xl6OutputPuts("        }\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return result;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static bool xl6_xrt_typed_array_set_at(xtarray values, long long index, xvalue value)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    unsigned int resolved;\n", out);
    xl6OutputPuts("    return xl6_xrt_typed_array_resolve_index(values, index, &resolved) && xrtTypedArraySetValue(values, resolved + 1u, value);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static xvalue xl6_xrt_typed_array_to_xvalue(const xrt_type_desc* type, xtarray values)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xvalue result = xvoCreateArray();\n", out);
    xl6OutputPuts("    unsigned int count = xrtTypedArrayCount(values);\n", out);
    xl6OutputPuts("    unsigned int i;\n", out);
    xl6OutputPuts("    if ( result == 0 ) {\n", out);
    xl6OutputPuts("        return xvoCreateNull();\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    for ( i = 0; i < count; ++i ) {\n", out);
    xl6OutputPuts("        xvalue item = xl6_xrt_typed_box_or_null(type, xl6_xrt_typed_array_get_at(values, i));\n", out);
    xl6OutputPuts("        if ( !xvoArrayAppendValue(result, item, true) && item != 0 ) {\n", out);
    xl6OutputPuts("            xvoUnref(item);\n", out);
    xl6OutputPuts("        }\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return result;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static xtarray xl6_xrt_typed_array_copy_mode(const xrt_type_desc* type, xtarray values, unsigned int mode)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xtarray result = xrtTypedArrayCreate(type, mode);\n", out);
    xl6OutputPuts("    unsigned int count = xrtTypedArrayCount(values);\n", out);
    xl6OutputPuts("    unsigned int i;\n", out);
    xl6OutputPuts("    if ( result == 0 ) {\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    for ( i = 0; i < count; ++i ) {\n", out);
    xl6OutputPuts("        xvalue item = xl6_xrt_typed_box_or_null(type, xl6_xrt_typed_array_get_at(values, i));\n", out);
    xl6OutputPuts("        if ( !xrtTypedArrayAppendValue(result, item) && item != 0 ) {\n", out);
    xl6OutputPuts("            xvoUnref(item);\n", out);
    xl6OutputPuts("        } else if ( item != 0 ) {\n", out);
    xl6OutputPuts("            xvoUnref(item);\n", out);
    xl6OutputPuts("        }\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return result;\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("static xtarray xl6_xrt_typed_array_copy(const xrt_type_desc* type, xtarray values)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xl6_xrt_typed_array_copy_mode(type, values, XRT_OBJMODE_LOCAL);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static xvalue xl6_xrt_typed_list_to_xvalue(const xrt_type_desc* type, xtlist values)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xvalue result = xvoCreateList();\n", out);
    xl6OutputPuts("    unsigned int count = xrtTypedListCount(values);\n", out);
    xl6OutputPuts("    unsigned int i;\n", out);
    xl6OutputPuts("    if ( result == 0 ) {\n", out);
    xl6OutputPuts("        return xvoCreateNull();\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    for ( i = 0; i < count; ++i ) {\n", out);
    xl6OutputPuts("        long long key = 0;\n", out);
    xl6OutputPuts("        xvalue item = xl6_xrt_typed_box_or_null(type, xrtTypedListItemAt(values, i, &key));\n", out);
    xl6OutputPuts("        if ( !xvoListSetValue(result, key, item, true) && item != 0 ) {\n", out);
    xl6OutputPuts("            xvoUnref(item);\n", out);
    xl6OutputPuts("        }\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return result;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static xtlist xl6_xrt_typed_list_copy_mode(const xrt_type_desc* type, xtlist values, unsigned int mode)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xtlist result = xrtTypedListCreate(type, mode);\n", out);
    xl6OutputPuts("    unsigned int count = xrtTypedListCount(values);\n", out);
    xl6OutputPuts("    unsigned int i;\n", out);
    xl6OutputPuts("    if ( result == 0 ) {\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    for ( i = 0; i < count; ++i ) {\n", out);
    xl6OutputPuts("        long long key = 0;\n", out);
    xl6OutputPuts("        xvalue item = xl6_xrt_typed_box_or_null(type, xrtTypedListItemAt(values, i, &key));\n", out);
    xl6OutputPuts("        if ( !xrtTypedListSetValue(result, key, item) && item != 0 ) {\n", out);
    xl6OutputPuts("            xvoUnref(item);\n", out);
    xl6OutputPuts("        } else if ( item != 0 ) {\n", out);
    xl6OutputPuts("            xvoUnref(item);\n", out);
    xl6OutputPuts("        }\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return result;\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("static xtlist xl6_xrt_typed_list_copy(const xrt_type_desc* type, xtlist values)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xl6_xrt_typed_list_copy_mode(type, values, XRT_OBJMODE_LOCAL);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static xvalue xl6_xrt_typed_set_to_xvalue(const xrt_type_desc* type, xtset values)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xvalue result = xvoCreateSet();\n", out);
    xl6OutputPuts("    unsigned int count = xrtTypedSetCount(values);\n", out);
    xl6OutputPuts("    unsigned int i;\n", out);
    xl6OutputPuts("    if ( result == 0 ) {\n", out);
    xl6OutputPuts("        return xvoCreateNull();\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    for ( i = 0; i < count; ++i ) {\n", out);
    xl6OutputPuts("        xvalue item = xl6_xrt_typed_box_or_null(type, xrtTypedSetItemAt(values, i));\n", out);
    xl6OutputPuts("        if ( !xvoCollSetValue(result, item, true) && item != 0 ) {\n", out);
    xl6OutputPuts("            xvoUnref(item);\n", out);
    xl6OutputPuts("        }\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return result;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static xtset xl6_xrt_typed_set_copy_mode(const xrt_type_desc* type, xtset values, unsigned int mode)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xtset result = xrtTypedSetCreate(type, mode);\n", out);
    xl6OutputPuts("    unsigned int count = xrtTypedSetCount(values);\n", out);
    xl6OutputPuts("    unsigned int i;\n", out);
    xl6OutputPuts("    if ( result == 0 ) {\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    for ( i = 0; i < count; ++i ) {\n", out);
    xl6OutputPuts("        xvalue item = xl6_xrt_typed_box_or_null(type, xrtTypedSetItemAt(values, i));\n", out);
    xl6OutputPuts("        if ( !xrtTypedSetAddValue(result, item) && item != 0 ) {\n", out);
    xl6OutputPuts("            xvoUnref(item);\n", out);
    xl6OutputPuts("        } else if ( item != 0 ) {\n", out);
    xl6OutputPuts("            xvoUnref(item);\n", out);
    xl6OutputPuts("        }\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return result;\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("static xtset xl6_xrt_typed_set_copy(const xrt_type_desc* type, xtset values)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xl6_xrt_typed_set_copy_mode(type, values, XRT_OBJMODE_LOCAL);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static xvalue xl6_xrt_typed_dict_to_xvalue(const xrt_type_desc* type, xtdict values)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xvalue result = xvoCreateTable();\n", out);
    xl6OutputPuts("    unsigned int count = xrtTypedDictCount(values);\n", out);
    xl6OutputPuts("    unsigned int i;\n", out);
    xl6OutputPuts("    if ( result == 0 ) {\n", out);
    xl6OutputPuts("        return xvoCreateNull();\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    for ( i = 0; i < count; ++i ) {\n", out);
    xl6OutputPuts("        const char* key = 0;\n", out);
    xl6OutputPuts("        unsigned int keyLen = 0;\n", out);
    xl6OutputPuts("        xvalue item = xl6_xrt_typed_box_or_null(type, xrtTypedDictItemAt(values, i, &key, &keyLen));\n", out);
    xl6OutputPuts("        if ( !xvoTableSetValue(result, key, keyLen, item, true) && item != 0 ) {\n", out);
    xl6OutputPuts("            xvoUnref(item);\n", out);
    xl6OutputPuts("        }\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return result;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static xtdict xl6_xrt_typed_dict_copy_mode(const xrt_type_desc* type, xtdict values, unsigned int mode)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xtdict result = xrtTypedDictCreate(type, mode);\n", out);
    xl6OutputPuts("    unsigned int count = xrtTypedDictCount(values);\n", out);
    xl6OutputPuts("    unsigned int i;\n", out);
    xl6OutputPuts("    if ( result == 0 ) {\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    for ( i = 0; i < count; ++i ) {\n", out);
    xl6OutputPuts("        const char* key = 0;\n", out);
    xl6OutputPuts("        unsigned int keyLen = 0;\n", out);
    xl6OutputPuts("        xvalue item = xl6_xrt_typed_box_or_null(type, xrtTypedDictItemAt(values, i, &key, &keyLen));\n", out);
    xl6OutputPuts("        if ( !xrtTypedDictSetValue(result, key, keyLen, item) && item != 0 ) {\n", out);
    xl6OutputPuts("            xvoUnref(item);\n", out);
    xl6OutputPuts("        } else if ( item != 0 ) {\n", out);
    xl6OutputPuts("            xvoUnref(item);\n", out);
    xl6OutputPuts("        }\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return result;\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("static xtdict xl6_xrt_typed_dict_copy(const xrt_type_desc* type, xtdict values)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xl6_xrt_typed_dict_copy_mode(type, values, XRT_OBJMODE_LOCAL);\n", out);
    xl6OutputPuts("}\n\n", out);

    /*
        动态操作只在读取元素时装箱，容器自身始终保持原来的静态存储。
        这样 array<T>/list<T>/set<T>/dict<T> 放入 var 后仍共享同一份数据。
    */
    xl6OutputPuts("static xvalue xl6_xrt_typed_container_box_item(xvalue values, const void* item)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xl6_xrt_typed_box_or_null(xl6_xrt_typed_container_item_type(values), item);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static xvalue xl6_xrt_typed_container_snapshot(xvalue values)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    unsigned int kind = xl6_xrt_typed_container_kind(values);\n", out);
    xl6OutputPuts("    void* handle = values != 0 ? xvoGetHandleData(values) : 0;\n", out);
    xl6OutputPuts("    const xrt_type_desc* itemType = xl6_xrt_typed_container_item_type(values);\n", out);
    xl6OutputPuts("    xvalue result;\n", out);
    xl6OutputPuts("    if ( kind == XL6_XRT_TYPE_KIND_INVALID ) { return values; }\n", out);
    xl6OutputPuts("    if ( kind == XL6_XRT_TYPE_KIND_ARRAY ) { result = xl6_xrt_typed_array_to_xvalue(itemType, (xtarray)handle); }\n", out);
    xl6OutputPuts("    else if ( kind == XL6_XRT_TYPE_KIND_LIST ) { result = xl6_xrt_typed_list_to_xvalue(itemType, (xtlist)handle); }\n", out);
    xl6OutputPuts("    else if ( kind == XL6_XRT_TYPE_KIND_SET ) { result = xl6_xrt_typed_set_to_xvalue(itemType, (xtset)handle); }\n", out);
    xl6OutputPuts("    else { result = xl6_xrt_typed_dict_to_xvalue(itemType, (xtdict)handle); }\n", out);
    xl6OutputPuts("    if ( values != 0 ) { xvoUnref(values); }\n", out);
    xl6OutputPuts("    return result != 0 ? result : xvoCreateNull();\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static unsigned int xl6_xrt_typed_container_count(xvalue values)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    unsigned int kind = xl6_xrt_typed_container_kind(values);\n", out);
    xl6OutputPuts("    void* handle = values != 0 ? xvoGetHandleData(values) : 0;\n", out);
    xl6OutputPuts("    if ( kind == XL6_XRT_TYPE_KIND_ARRAY ) { return xrtTypedArrayCount((xtarray)handle); }\n", out);
    xl6OutputPuts("    if ( kind == XL6_XRT_TYPE_KIND_LIST ) { return xrtTypedListCount((xtlist)handle); }\n", out);
    xl6OutputPuts("    if ( kind == XL6_XRT_TYPE_KIND_SET ) { return xrtTypedSetCount((xtset)handle); }\n", out);
    xl6OutputPuts("    if ( kind == XL6_XRT_TYPE_KIND_DICT ) { return xrtTypedDictCount((xtdict)handle); }\n", out);
    xl6OutputPuts("    return 0u;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static xvalue xl6_xrt_typed_container_get_i64(xvalue values, long long index)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    unsigned int kind = xl6_xrt_typed_container_kind(values);\n", out);
    xl6OutputPuts("    void* handle = values != 0 ? xvoGetHandleData(values) : 0;\n", out);
    xl6OutputPuts("    const void* item = 0;\n", out);
    xl6OutputPuts("    if ( kind == XL6_XRT_TYPE_KIND_ARRAY ) { item = xl6_xrt_typed_array_get_at((xtarray)handle, index); }\n", out);
    xl6OutputPuts("    else if ( kind == XL6_XRT_TYPE_KIND_LIST ) { item = xrtTypedListGet((xtlist)handle, index); }\n", out);
    xl6OutputPuts("    else if ( kind == XL6_XRT_TYPE_KIND_SET && index >= 0 && (unsigned long long)index <= 0xFFFFFFFFULL ) {\n", out);
    xl6OutputPuts("        item = xrtTypedSetItemAt((xtset)handle, (unsigned int)index);\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return item != 0 ? xl6_xrt_typed_container_box_item(values, item) : xvoCreateNull();\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static xvalue xl6_xrt_typed_container_get_key(xvalue values, const char* key)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    void* item;\n", out);
    xl6OutputPuts("    if ( xl6_xrt_typed_container_kind(values) != XL6_XRT_TYPE_KIND_DICT ) { return xvoCreateNull(); }\n", out);
    xl6OutputPuts("    item = xrtTypedDictGet((xtdict)xvoGetHandleData(values), key != 0 ? key : \"\", 0);\n", out);
    xl6OutputPuts("    return item != 0 ? xl6_xrt_typed_container_box_item(values, item) : xvoCreateNull();\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static bool xl6_xrt_typed_container_set_i64_owned(xvalue values, long long index, xvalue item)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    unsigned int kind = xl6_xrt_typed_container_kind(values);\n", out);
    xl6OutputPuts("    void* handle = values != 0 ? xvoGetHandleData(values) : 0;\n", out);
    xl6OutputPuts("    unsigned int resolved = 0;\n", out);
    xl6OutputPuts("    bool ok = false;\n", out);
    xl6OutputPuts("    if ( kind == XL6_XRT_TYPE_KIND_ARRAY && xl6_xrt_typed_array_resolve_index((xtarray)handle, index, &resolved) ) {\n", out);
    xl6OutputPuts("        ok = xrtTypedArraySetValue((xtarray)handle, resolved + 1u, item);\n", out);
    xl6OutputPuts("    } else if ( kind == XL6_XRT_TYPE_KIND_LIST ) {\n", out);
    xl6OutputPuts("        ok = xrtTypedListSetValue((xtlist)handle, index, item);\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( ok && item != 0 ) { xvoUnref(item); }\n", out);
    xl6OutputPuts("    return ok;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static bool xl6_xrt_typed_container_set_key_owned(xvalue values, const char* key, xvalue item)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    bool ok = xl6_xrt_typed_container_kind(values) == XL6_XRT_TYPE_KIND_DICT &&\n", out);
    xl6OutputPuts("              xrtTypedDictSetValue((xtdict)xvoGetHandleData(values), key != 0 ? key : \"\", 0, item);\n", out);
    xl6OutputPuts("    if ( ok && item != 0 ) { xvoUnref(item); }\n", out);
    xl6OutputPuts("    return ok;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static bool xl6_xrt_typed_container_set_key_borrow(xvalue values, const char* key, xvalue item)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xl6_xrt_typed_container_kind(values) == XL6_XRT_TYPE_KIND_DICT &&\n", out);
    xl6OutputPuts("           xrtTypedDictSetValue((xtdict)xvoGetHandleData(values), key != 0 ? key : \"\", 0, item);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static int xl6_xrt_typed_container_contains(xvalue values, xvalue item)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    unsigned int kind = xl6_xrt_typed_container_kind(values);\n", out);
    xl6OutputPuts("    void* handle = values != 0 ? xvoGetHandleData(values) : 0;\n", out);
    xl6OutputPuts("    if ( kind == XL6_XRT_TYPE_KIND_ARRAY ) { return xrtTypedArrayContainsValue((xtarray)handle, item) ? 1 : 0; }\n", out);
    xl6OutputPuts("    if ( kind == XL6_XRT_TYPE_KIND_LIST ) { return xrtTypedListContainsValue((xtlist)handle, item) ? 1 : 0; }\n", out);
    xl6OutputPuts("    if ( kind == XL6_XRT_TYPE_KIND_SET ) { return xrtTypedSetExistsValue((xtset)handle, item) ? 1 : 0; }\n", out);
    xl6OutputPuts("    if ( kind == XL6_XRT_TYPE_KIND_DICT && item != 0 && xvoType(item) == XL6_XVO_DT_TEXT ) {\n", out);
    xl6OutputPuts("        return xrtTypedDictExists((xtdict)handle, xvoGetText(item), 0) ? 1 : 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static int xl6_xrt_typed_container_clear(xvalue values)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    unsigned int kind = xl6_xrt_typed_container_kind(values);\n", out);
    xl6OutputPuts("    void* handle = values != 0 ? xvoGetHandleData(values) : 0;\n", out);
    xl6OutputPuts("    if ( kind == XL6_XRT_TYPE_KIND_ARRAY ) { return xrtTypedArrayClear((xtarray)handle) ? 1 : 0; }\n", out);
    xl6OutputPuts("    if ( kind == XL6_XRT_TYPE_KIND_LIST ) { return xrtTypedListClear((xtlist)handle) ? 1 : 0; }\n", out);
    xl6OutputPuts("    if ( kind == XL6_XRT_TYPE_KIND_SET ) { return xrtTypedSetClear((xtset)handle) ? 1 : 0; }\n", out);
    xl6OutputPuts("    if ( kind == XL6_XRT_TYPE_KIND_DICT ) { return xrtTypedDictClear((xtdict)handle) ? 1 : 0; }\n", out);
    xl6OutputPuts("    return 0;\n", out);
    xl6OutputPuts("}\n\n", out);
}
