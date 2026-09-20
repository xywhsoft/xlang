/* 只消费已验证的桥接需求。算法交给 XRT，适配层只负责签名和拥有权。 */
static int xl6IrCgenEmitStringBridgeRuntime(
    XL6_Output* out, const XL6_IrCgenNeeds* needs)
{
    if ( out == NULL || needs == NULL ) return 0;

    if ( needs->needsStringRepeatBridgeRuntime )
        xl6OutputPuts(
            "void* xl6_xrt_string_repeat(const char* text, int64_t count)\n"
            "{ return count > 0 ? xrtStrRepeat(xrtStrView(text), (size_t)count) : xrtStrDup(\"\"); }\n\n", out);

    if ( needs->needsStringByteLenBridgeRuntime )
        xl6OutputPuts(
            "size_t xl6_xrt_string_byte_len(const char* text)\n"
            "{ return xrtStrView(text).Size; }\n\n", out);

    if ( needs->needsStringByteAtBridgeRuntime )
        xl6OutputPuts(
            "int xl6_xrt_string_byte_at(const char* text, int64_t index)\n"
            "{\n"
            "    xstrview view = xrtStrView(text); uint64_t position;\n"
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
            "void* xl6_xrt_string_slice(const char* text, int64_t start, int64_t count)\n"
            "{ return xrtUtf8Substr(xrtStrView(text), start, count); }\n\n", out);

    if ( needs->needsStringReplaceBridgeRuntime )
        xl6OutputPuts(
            "extern char* xrtStrReplace(xstrview, xstrview, xstrview);\n"
            "void* xl6_xrt_string_replace(const char* text, const char* oldText, const char* newText)\n"
            "{ return xrtStrReplace(xrtStrView(text), xrtStrView(oldText), xrtStrView(newText)); }\n\n", out);

    if ( needs->needsStringStartsWithBridgeRuntime )
        xl6OutputPuts(
            "extern bool xrtStrStarts(xstrview, xstrview);\n"
            "extern bool xrtStrCaseStarts(xstrview, xstrview);\n"
            "bool xl6_xrt_string_starts_with(const char* text, const char* prefix, bool ignoreCase)\n"
            "{ xstrview value = xrtStrView(text), part = xrtStrView(prefix); return ignoreCase ? xrtStrCaseStarts(value, part) : xrtStrStarts(value, part); }\n\n", out);

    if ( needs->needsStringEndsWithBridgeRuntime )
        xl6OutputPuts(
            "extern bool xrtStrEnds(xstrview, xstrview);\n"
            "extern bool xrtStrCaseEnds(xstrview, xstrview);\n"
            "bool xl6_xrt_string_ends_with(const char* text, const char* suffix, bool ignoreCase)\n"
            "{ xstrview value = xrtStrView(text), part = xrtStrView(suffix); return ignoreCase ? xrtStrCaseEnds(value, part) : xrtStrEnds(value, part); }\n\n", out);

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
            "xvalue* xl6_xrt_string_split(const char* text, const char* separator)\n"
            "{ return xlir_string_list_take(xrtStrSplit(xrtStrView(text), xrtStrView(separator))); }\n\n", out);
    if ( needs->needsStringSplitLinesRuntime )
        xl6OutputPuts(
            "xvalue* xl6_xrt_string_split_lines(const char* text)\n"
            "{ return xlir_string_list_take(xrtStrSplitLines(xrtStrView(text))); }\n\n", out);
    return 1;
}
