/* 字符串标准库的薄适配：编码、查找、匹配、构建和分配均使用 XRT。 */
static int xl6IrCgenEmitStringTextRuntime(
    XL6_Output* out, const XL6_IrCgenNeeds* needs)
{
    if (out == NULL || needs == NULL) return 0;
    if ( needs->needsStringCompareBridgeRuntime )
        xl6OutputPuts(
            "int xl6_xrt_string_compare(xlir_string left, xlir_string right, bool ignoreCase)\n"
            "{\n"
            "    return ignoreCase ? xrtStrCaseCompare(left, right) : xrtStrCompare(left, right);\n"
            "}\n"
            "\n", out);
    if ( needs->needsStringLastIndexOfBridgeRuntime )
        xl6OutputPuts(
            "int64_t xl6_xrt_string_last_index_of(xlir_string text, xlir_string part, bool ignoreCase)\n"
            "{\n"
            "    size_t index = ignoreCase ? xrtUtf8CaseRFind(text, part) : xrtUtf8RFind(text, part);\n"
            "    return index == XRT_NPOS ? -1 : (int64_t)index;\n"
            "}\n"
            "\n", out);
    if ( needs->needsStringCountBridgeRuntime )
        xl6OutputPuts(
            "int64_t xl6_xrt_string_count(xlir_string text, xlir_string part, bool ignoreCase)\n"
            "{\n"
            "    return (int64_t)(ignoreCase ? xrtStrCaseCount(text, part) : xrtStrCount(text, part));\n"
            "}\n"
            "\n", out);
    if ( needs->needsStringHasAnyBridgeRuntime )
        xl6OutputPuts(
            "bool xl6_xrt_string_has_any(xlir_string text, xlir_string chars)\n"
            "{\n"
            "    return xrtUtf8ContainsAny(text, chars);\n"
            "}\n"
            "\n", out);
    if ( needs->needsStringLikeBridgeRuntime )
        xl6OutputPuts(
            "bool xl6_xrt_string_like(xlir_string text, xlir_string pattern, bool ignoreCase)\n"
            "{\n"
            "    return xrtStrGlob(text, pattern, ignoreCase ? XSTR_GLOB_CASE_ASCII : 0);\n"
            "}\n"
            "\n", out);
    if ( needs->needsStringApproxBridgeRuntime )
        xl6OutputPuts(
            "bool xl6_xrt_string_approx(xlir_string left, xlir_string right)\n"
            "{\n"
            "    return xrtUtf8Similarity(left, right) >= 0.95;\n"
            "}\n"
            "\n", out);
    if ( needs->needsStringSimilarityBridgeRuntime )
        xl6OutputPuts(
            "double xl6_xrt_string_similarity(xlir_string left, xlir_string right)\n"
            "{\n"
            "    return xrtUtf8Similarity(left, right);\n"
            "}\n"
            "\n", out);
    if ( needs->needsStringIsUtf8BridgeRuntime )
        xl6OutputPuts(
            "bool xl6_xrt_string_is_utf8(xlir_string text)\n"
            "{\n"
            "    return xrtUtf8Valid(text, NULL);\n"
            "}\n"
            "\n", out);
    if ( needs->needsStringLowerBridgeRuntime )
        xl6OutputPuts(
            "void* xl6_xrt_string_lower(xlir_string text)\n"
            "{\n"
            "    return xrtStrLower(text);\n"
            "}\n"
            "\n", out);
    if ( needs->needsStringUpperBridgeRuntime )
        xl6OutputPuts(
            "void* xl6_xrt_string_upper(xlir_string text)\n"
            "{\n"
            "    return xrtStrUpper(text);\n"
            "}\n"
            "\n", out);
    if ( needs->needsStringTrimBridgeRuntime )
        xl6OutputPuts(
            "void* xl6_xrt_string_trim(xlir_string text, xlir_string chars)\n"
            "{\n"
            "    xstrview view = text, set = chars, result;\n"
            "    if (set.Size == 0) result = xrtStrTrim(view);\n"
            "    else if (!xrtUtf8TrimSet(view, set, &result)) return NULL;\n"
            "    return xrtStrDupView(result);\n"
            "}\n"
            "\n", out);
    if ( needs->needsStringLTrimBridgeRuntime )
        xl6OutputPuts(
            "void* xl6_xrt_string_ltrim(xlir_string text, xlir_string chars)\n"
            "{\n"
            "    xstrview view = text, set = chars, result;\n"
            "    if (set.Size == 0) result = xrtStrTrimLeft(view);\n"
            "    else if (!xrtUtf8TrimLeftSet(view, set, &result)) return NULL;\n"
            "    return xrtStrDupView(result);\n"
            "}\n"
            "\n", out);
    if ( needs->needsStringRTrimBridgeRuntime )
        xl6OutputPuts(
            "void* xl6_xrt_string_rtrim(xlir_string text, xlir_string chars)\n"
            "{\n"
            "    xstrview view = text, set = chars, result;\n"
            "    if (set.Size == 0) result = xrtStrTrimRight(view);\n"
            "    else if (!xrtUtf8TrimRightSet(view, set, &result)) return NULL;\n"
            "    return xrtStrDupView(result);\n"
            "}\n"
            "\n", out);
    if ( needs->needsStringFilterBridgeRuntime )
        xl6OutputPuts(
            "void* xl6_xrt_string_filter(xlir_string text, xlir_string chars)\n"
            "{\n"
            "    return xrtUtf8Filter(text, chars);\n"
            "}\n"
            "\n", out);
    if ( needs->needsStringLeftBridgeRuntime )
        xl6OutputPuts(
            "void* xl6_xrt_string_left(xlir_string text, int64_t count)\n"
            "{\n"
            "    return xrtUtf8Substr(text, 0, count > 0 ? count : 0);\n"
            "}\n"
            "\n", out);
    if ( needs->needsStringRightBridgeRuntime )
        xl6OutputPuts(
            "void* xl6_xrt_string_right(xlir_string text, int64_t count)\n"
            "{\n"
            "    return xrtUtf8Substr(text, count > 0 ? -count : 0, count > 0 ? count : 0);\n"
            "}\n"
            "\n", out);
    if ( needs->needsStringCharAtBridgeRuntime )
        xl6OutputPuts(
            "void* xl6_xrt_string_char_at(xlir_string text, int64_t index)\n"
            "{\n"
            "    return xrtUtf8Substr(text, index, 1);\n"
            "}\n"
            "\n", out);
    if ( needs->needsStringIsEmptyBridgeRuntime )
        xl6OutputPuts(
            "bool xl6_xrt_string_is_empty(xlir_string text)\n"
            "{\n"
            "    return xrtStrEmpty(text);\n"
            "}\n"
            "\n", out);
    if ( needs->needsStringIsBlankBridgeRuntime )
        xl6OutputPuts(
            "bool xl6_xrt_string_is_blank(xlir_string text)\n"
            "{\n"
            "    return xrtStrBlank(text);\n"
            "}\n"
            "\n", out);
    if ( needs->needsStringRemoveBridgeRuntime )
        xl6OutputPuts(
            "void* xl6_xrt_string_remove(xlir_string text, xlir_string part, bool ignoreCase)\n"
            "{\n"
            "    xstrview value = text, pattern = part;\n"
            "    xstrbuf output; size_t start = 0, found;\n"
            "    if (!ignoreCase) return xrtStrReplace(value, pattern, xrtStrView(\"\"));\n"
            "    if (pattern.Size == 0) return xrtStrDupView(value);\n"
            "    xrtStrBufInit(&output);\n"
            "    while ((found = xrtStrCaseFind(value, pattern, start)) != XRT_NPOS) {\n"
            "        if (!xrtStrBufAppend(&output, xrtStrSlice(value, start, found - start))) goto fail;\n"
            "        start = found + pattern.Size;\n"
            "    }\n"
            "    if (!xrtStrBufAppend(&output, xrtStrSlice(value, start, value.Size - start))) goto fail;\n"
            "    return xrtStrBufTake(&output);\n"
            "    fail:\n"
            "    xrtStrBufFree(&output); return NULL;\n"
            "}\n"
            "\n", out);
    if ( needs->needsStringRemoveAtBridgeRuntime )
        xl6OutputPuts(
            "void* xl6_xrt_string_remove_at(xlir_string text, int64_t start, int64_t count)\n"
            "{\n"
            "    return xrtUtf8Remove(text, start, count);\n"
            "}\n"
            "\n", out);
    if ( needs->needsStringInsertBridgeRuntime )
        xl6OutputPuts(
            "void* xl6_xrt_string_insert(xlir_string text, int64_t start, xlir_string part)\n"
            "{\n"
            "    return xrtUtf8Insert(text, start, part);\n"
            "}\n"
            "\n", out);
    if ( needs->needsStringPadLeftBridgeRuntime )
        xl6OutputPuts(
            "void* xl6_xrt_string_pad_left(xlir_string text, int64_t width, xlir_string pad)\n"
            "{\n"
            "    return xrtUtf8PadLeft(text, width > 0 ? (size_t)width : 0, pad);\n"
            "}\n"
            "\n", out);
    if ( needs->needsStringPadRightBridgeRuntime )
        xl6OutputPuts(
            "void* xl6_xrt_string_pad_right(xlir_string text, int64_t width, xlir_string pad)\n"
            "{\n"
            "    return xrtUtf8PadRight(text, width > 0 ? (size_t)width : 0, pad);\n"
            "}\n"
            "\n", out);
    if ( needs->needsStringPadCenterBridgeRuntime )
        xl6OutputPuts(
            "void* xl6_xrt_string_pad_center(xlir_string text, int64_t width, xlir_string pad)\n"
            "{\n"
            "    return xrtUtf8PadCenter(text, width > 0 ? (size_t)width : 0, pad);\n"
            "}\n"
            "\n", out);
    if ( needs->needsStringReverseBridgeRuntime )
        xl6OutputPuts(
            "void* xl6_xrt_string_reverse(xlir_string text)\n"
            "{\n"
            "    return xrtUtf8Reverse(text);\n"
            "}\n"
            "\n", out);
    return 1;
}
