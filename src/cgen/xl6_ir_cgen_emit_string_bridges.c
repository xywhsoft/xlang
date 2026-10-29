/* 只消费已验证的桥接需求。算法交给 XRT，适配层只负责签名和拥有权。 */
static int xl6IrCgenEmitStringBridgeRuntime(
    XL6_Output* out, const XL6_IrCgenNeeds* needs)
{
    if ( out == NULL || needs == NULL ) return 0;

    if ( needs->needsStringRepeatBridgeRuntime )
        xl6OutputPuts(
            "void* xl6_xrt_string_repeat(xlir_string text, int64_t count)\n"
            "{ return count > 0 ? xrtStrRepeat(text, (size_t)count) : xrtStrDup(\"\"); }\n\n", out);

    if ( needs->needsStringByteLenBridgeRuntime )
        xl6OutputPuts(
            "size_t xl6_xrt_string_byte_len(xlir_string text)\n"
            "{ return text.Size; }\n\n", out);

    if ( needs->needsStringByteAtBridgeRuntime )
        xl6OutputPuts(
            "int xl6_xrt_string_byte_at(xlir_string text, int64_t index)\n"
            "{\n"
            "    xstrview view = text; uint64_t position;\n"
            "    if (index < 0) {\n"
            "        uint64_t distance = UINT64_C(0) - (uint64_t)index;\n"
            "        if (distance > (uint64_t)view.Size) return -1;\n"
            "        position = (uint64_t)view.Size - distance;\n"
            "    } else { position = (uint64_t)index; }\n"
            "    return position < (uint64_t)view.Size ? (int)(unsigned char)view.Data[(size_t)position] : -1;\n"
            "}\n\n", out);

    if ( needs->needsStringSliceBridgeRuntime )
        xl6OutputPuts(
            "extern char* xrtUtf8Substr(xstrview, int64_t, int64_t);\n"
            "void* xl6_xrt_string_slice(xlir_string text, int64_t start, int64_t count)\n"
            "{ return xrtUtf8Substr(text, start, count); }\n\n", out);

    if ( needs->needsStringReplaceBridgeRuntime )
        xl6OutputPuts(
            "extern char* xrtStrReplace(xstrview, xstrview, xstrview);\n"
            "void* xl6_xrt_string_replace(xlir_string text, xlir_string oldText, xlir_string newText)\n"
            "{ return xrtStrReplace(text, oldText, newText); }\n\n", out);

    if ( needs->needsStringStartsWithBridgeRuntime )
        xl6OutputPuts(
            "extern bool xrtStrStarts(xstrview, xstrview);\n"
            "extern bool xrtStrCaseStarts(xstrview, xstrview);\n"
            "bool xl6_xrt_string_starts_with(xlir_string text, xlir_string prefix, bool ignoreCase)\n"
            "{ xstrview value = text, part = prefix; return ignoreCase ? xrtStrCaseStarts(value, part) : xrtStrStarts(value, part); }\n\n", out);

    if ( needs->needsStringEndsWithBridgeRuntime )
        xl6OutputPuts(
            "extern bool xrtStrEnds(xstrview, xstrview);\n"
            "extern bool xrtStrCaseEnds(xstrview, xstrview);\n"
            "bool xl6_xrt_string_ends_with(xlir_string text, xlir_string suffix, bool ignoreCase)\n"
            "{ xstrview value = text, part = suffix; return ignoreCase ? xrtStrCaseEnds(value, part) : xrtStrEnds(value, part); }\n\n", out);

    /* 字符串元素借用完整视图；其他值复用统一转换，失败也释放已转换的前缀。
     * 拼接及长度溢出检查由 XRT 负责，调用者只接管最终缓冲区。 */
    if ( needs->needsStringJoinBridgeRuntime )
        xl6OutputPuts(
            "extern xlir_string xl6ValueTextV1(const xvalue*);\n"
            "void* xl6_xrt_string_join(xvalue* values, xlir_string separator)\n"
            "{\n"
            "    size_t count, i; xstrview* parts; char** owned; char* result = NULL;\n"
            "    if (!xrtValueIs(values, XVALUE_ARRAY)) { xrtSetErrorKind(XERR_TYPE); return NULL; }\n"
            "    count = xrtValueCount(values);\n"
            "    if (count == 0) return xrtStrJoin(separator, NULL, 0);\n"
            "    parts = (xstrview*)xrtCalloc(count, sizeof(*parts));\n"
            "    if (parts == NULL) return NULL;\n"
            "    owned = (char**)xrtCalloc(count, sizeof(*owned));\n"
            "    if (owned == NULL) { xrtFree(parts); return NULL; }\n"
            "    for (i = 0; i < count; ++i) {\n"
            "        xvalue* item = xrtValueArrayGet(values, i);\n"
            "        if (xrtValueIs(item, XVALUE_STRING)) {\n"
            "            if (!xrtValueGetString(item, &parts[i])) goto done;\n"
            "        } else {\n"
            "            parts[i] = xl6ValueTextV1(item);\n"
            "            owned[i] = (char*)parts[i].Data;\n"
            "            if (owned[i] == NULL) goto done;\n"
            "        }\n"
            "    }\n"
            "    result = xrtStrJoin(separator, parts, count);\n"
            "done:\n"
            "    for (i = 0; i < count; ++i) xrtFree(owned[i]);\n"
            "    xrtFree(owned); xrtFree(parts); return result;\n"
            "}\n\n", out);

    /* 数组桥只负责元素装箱；字节保持无符号，字符解码使用 XRT。
     * 先预留容量，任何失败都释放整个结果，不发布部分数组。 */
    if ( needs->needsStringBytesBridgeRuntime )
        xl6OutputPuts(
            "xvalue* xl6_xrt_string_bytes(xlir_string text)\n"
            "{\n"
            "    xstrview view = text; size_t i;\n"
            "    xvalue* result = xrtValueArray();\n"
            "    if (result == NULL) return NULL;\n"
            "    if (!xrtValueReserve(result, view.Size)) goto fail;\n"
            "    for (i = 0; i < view.Size; ++i) {\n"
            "        xvalue* item = xrtValueInt((unsigned char)view.Data[i]);\n"
            "        if (item == NULL || !xrtValueArrayAppendNew(result, item)) goto fail;\n"
            "    }\n"
            "    return result;\n"
            "fail:\n"
            "    xrtValueRelease(result); return NULL;\n"
            "}\n\n", out);
    if ( needs->needsStringCharsBridgeRuntime )
        xl6OutputPuts(
            "xvalue* xl6_xrt_string_chars(xlir_string text)\n"
            "{\n"
            "    xstrview rest = text; size_t count = xrtUtf8Count(rest);\n"
            "    xvalue* result;\n"
            "    if (count == XRT_NPOS) { xrtSetErrorKind(XERR_VALUE); return NULL; }\n"
            "    result = xrtValueArray();\n"
            "    if (result == NULL) return NULL;\n"
            "    if (!xrtValueReserve(result, count)) goto fail;\n"
            "    while (rest.Size != 0) {\n"
            "        uint32 scalar; size_t size; xvalue* item;\n"
            "        if (xrtUtf8Decode(rest, &scalar, &size) != XUTF_OK) {\n"
            "            xrtSetErrorKind(XERR_VALUE); goto fail;\n"
            "        }\n"
            "        item = xrtValueString((xstrview){rest.Data, size});\n"
            "        if (item == NULL || !xrtValueArrayAppendNew(result, item)) goto fail;\n"
            "        rest.Data += size; rest.Size -= size;\n"
            "    }\n"
            "    return result;\n"
            "fail:\n"
            "    xrtValueRelease(result); return NULL;\n"
            "}\n\n", out);

    /* 使用 XRT 的公开 xstrlist 定义，统一拆分结果的接管与失败清理。
     * 片段复制进 xvalue 后可立即释放列表，不借用输入临时字符串。 */
    if ( needs->needsStringSplitBridgeRuntime || needs->needsStringSplitLinesRuntime )
        xl6OutputPuts(
            "static xvalue* xlir_string_list_take(xstrlist* parts)\n"
            "{\n"
            "    xvalue* result; size_t i;\n"
            "    if (parts == NULL) return NULL;\n"
            "    result = xrtValueArray();\n"
            "    if (result == NULL) { xrtStrListFree(parts); return NULL; }\n"
            "    for (i = 0; i < parts->Count; ++i) {\n"
            "        xvalue* item = xrtValueString(parts->Items[i]);\n"
            "        if (item == NULL || !xrtValueArrayAppendNew(result, item)) {\n"
            "            xrtValueRelease(result); xrtStrListFree(parts); return NULL;\n"
            "        }\n"
            "    }\n"
            "    xrtStrListFree(parts); return result;\n"
            "}\n\n", out);
    if ( needs->needsStringSplitBridgeRuntime )
        xl6OutputPuts(
            "xvalue* xl6_xrt_string_split(xlir_string text, xlir_string separator)\n"
            "{ return xlir_string_list_take(xrtStrSplit(text, separator)); }\n\n", out);
    if ( needs->needsStringSplitLinesRuntime )
        xl6OutputPuts(
            "xvalue* xl6_xrt_string_split_lines(xlir_string text)\n"
            "{ return xlir_string_list_take(xrtStrSplitLines(text)); }\n\n", out);
    return 1;
}
