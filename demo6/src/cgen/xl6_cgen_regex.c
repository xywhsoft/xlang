static void xl6CgenEmitRegexRuntime(XL6_Output* out)
{
    if ( out == NULL ) {
        return;
    }

    xl6OutputPuts("#ifndef XL6_REGEX_RUNTIME\n", out);
    xl6OutputPuts("#define XL6_REGEX_RUNTIME\n", out);
    xl6OutputPuts("#ifndef XL6_XVALUE_TYPEDEF\n", out);
    xl6OutputPuts("#define XL6_XVALUE_TYPEDEF\n", out);
    xl6OutputPuts("typedef struct xvalue_struct* xvalue;\n", out);
    xl6OutputPuts("#endif\n", out);
    xl6OutputPuts("typedef unsigned int xregexflags;\n", out);
    xl6OutputPuts("typedef struct xrt_regex xregex;\n", out);
    xl6OutputPuts("typedef struct xrt_regex_match xregexmatch;\n", out);
    xl6OutputPuts("typedef struct xrt_regex_set xregexset;\n", out);
    xl6OutputPuts("typedef struct { size_t iBegin; size_t iEnd; } xregexspan;\n", out);
    xl6OutputPuts("void* xrtMalloc(size_t iSize);\n", out);
    xl6OutputPuts("void xrtFree(void* p);\n", out);
    xl6OutputPuts("char* xrtCopyStr(const char* sText, size_t iSize);\n", out);
    xl6OutputPuts("xvalue xvoCreateArray(void);\n", out);
    xl6OutputPuts("xvalue xvoCreateInt(long long iVal);\n", out);
    xl6OutputPuts("bool xvoArrayAppendValue(xvalue pArr, xvalue pVal, bool bColloc);\n", out);
    xl6OutputPuts("unsigned int xvoArrayItemCount(xvalue pArr);\n", out);
    xl6OutputPuts("xvalue xvoArrayGetValue(xvalue pArr, unsigned int index);\n", out);
    xl6OutputPuts("const char* xvoGetText(xvalue pVal);\n", out);
    xl6OutputPuts("void xvoUnref(xvalue pVal);\n", out);
    xl6OutputPuts("xregex* xrtRegexCreate(const char* sPatternNt);\n", out);
    xl6OutputPuts("xregex* xrtRegexCreateEx(const char* sPattern, size_t iPatternSize, xregexflags iFlags);\n", out);
    xl6OutputPuts("void xrtRegexDestroy(xregex* pRegex);\n", out);
    xl6OutputPuts("int xrtRegexIsMatch(xregex* pRegex, const char* sText, size_t iTextSize);\n", out);
    xl6OutputPuts("int xrtRegexFullMatch(xregex* pRegex, const char* sText, size_t iTextSize);\n", out);
    xl6OutputPuts("int xrtRegexFind(xregex* pRegex, const char* sText, size_t iTextSize, xregexspan* pOutSpan);\n", out);
    xl6OutputPuts("xregexmatch* xrtRegexFindMatchAt(xregex* pRegex, const char* sText, size_t iTextSize, size_t iPos);\n", out);
    xl6OutputPuts("void xrtRegexMatchDestroy(xregexmatch* pMatch);\n", out);
    xl6OutputPuts("int xrtRegexMatchOk(const xregexmatch* pMatch);\n", out);
    xl6OutputPuts("long long xrtRegexMatchByteStart(const xregexmatch* pMatch);\n", out);
    xl6OutputPuts("long long xrtRegexMatchByteEnd(const xregexmatch* pMatch);\n", out);
    xl6OutputPuts("long long xrtRegexMatchStart(xregexmatch* pMatch);\n", out);
    xl6OutputPuts("long long xrtRegexMatchEnd(xregexmatch* pMatch);\n", out);
    xl6OutputPuts("char* xrtRegexMatchTextCopy(xregexmatch* pMatch);\n", out);
    xl6OutputPuts("unsigned int xrtRegexCaptureCount(const xregex* pRegex);\n", out);
    xl6OutputPuts("const char* xrtRegexCaptureName(const xregex* pRegex, unsigned int iCaptureIndex, size_t* pOutNameSize);\n", out);
    xl6OutputPuts("xregexset* xrtRegexSetCreateEx(const char* const* arrPatternsNt, size_t iPatternCount, xregexflags iFlags);\n", out);
    xl6OutputPuts("void xrtRegexSetDestroy(xregexset* pSet);\n", out);
    xl6OutputPuts("int xrtRegexSetIsMatchAt(xregexset* pSet, const char* sText, size_t iTextSize, size_t iPos);\n", out);
    xl6OutputPuts("int xrtRegexSetFirstAt(xregexset* pSet, const char* sText, size_t iTextSize, size_t iPos);\n", out);
    xl6OutputPuts("int xrtRegexSetMatchesAt(xregexset* pSet, const char* sText, size_t iTextSize, size_t iPos, unsigned int* pOutIndexes, unsigned int iMaxIndexes, unsigned int* pOutIndexCount);\n\n", out);

    xl6OutputPuts("void* xl6_xrt_regex_create_default(const char* pattern)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xrtRegexCreate(pattern != 0 ? pattern : \"\");\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_regex_create_flags(const char* pattern, long long flags)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xrtRegexCreateEx(pattern != 0 ? pattern : \"\", 0, (xregexflags)flags);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("long long xl6_xrt_regex_destroy(void* regex)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xrtRegexDestroy((xregex*)regex);\n", out);
    xl6OutputPuts("    return 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("int xl6_xrt_regex_is_match_native(void* regex, const char* text, size_t textSize)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( text == 0 ) { text = \"\"; textSize = 0; }\n", out);
    xl6OutputPuts("    return xrtRegexIsMatch((xregex*)regex, text, textSize);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("int xl6_xrt_regex_full_match_native(void* regex, const char* text, size_t textSize)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( text == 0 ) { text = \"\"; textSize = 0; }\n", out);
    xl6OutputPuts("    return xrtRegexFullMatch((xregex*)regex, text, textSize);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_regex_match_at(void* regex, const char* text, size_t textSize, size_t start)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( text == 0 ) { text = \"\"; textSize = 0; }\n", out);
    xl6OutputPuts("    return xrtRegexFindMatchAt((xregex*)regex, text, textSize, start);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("unsigned int xl6_xrt_regex_capture_count(void* regex)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xrtRegexCaptureCount((const xregex*)regex);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("long long xl6_xrt_regex_match_destroy(void* match)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xrtRegexMatchDestroy((xregexmatch*)match);\n", out);
    xl6OutputPuts("    return 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("int xl6_xrt_regex_match_ok(void* match)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xrtRegexMatchOk((const xregexmatch*)match) != 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("long long xl6_xrt_regex_match_byte_start(void* match)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xrtRegexMatchByteStart((const xregexmatch*)match);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("long long xl6_xrt_regex_match_byte_end(void* match)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xrtRegexMatchByteEnd((const xregexmatch*)match);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("long long xl6_xrt_regex_match_start(void* match)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xrtRegexMatchStart((xregexmatch*)match);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("long long xl6_xrt_regex_match_end(void* match)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xrtRegexMatchEnd((xregexmatch*)match);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_regex_match_text_copy(void* match)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xrtRegexMatchTextCopy((xregexmatch*)match);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_regex_capture_name_copy(void* regex, unsigned int index)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    size_t size = 0;\n", out);
    xl6OutputPuts("    const char* name = xrtRegexCaptureName((xregex*)regex, index, &size);\n", out);
    xl6OutputPuts("    return name != 0 ? xrtCopyStr(name, size) : 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("bool xl6_xrt_regex_is_match(const char* text, size_t textSize, const char* pattern, int flags)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xregex* regex;\n", out);
    xl6OutputPuts("    int matched;\n", out);
    xl6OutputPuts("    if ( text == 0 ) { text = \"\"; textSize = 0; }\n", out);
    xl6OutputPuts("    if ( pattern == 0 ) { pattern = \"\"; }\n", out);
    xl6OutputPuts("    regex = xrtRegexCreateEx(pattern, 0, (xregexflags)flags);\n", out);
    xl6OutputPuts("    if ( regex == 0 ) { return false; }\n", out);
    xl6OutputPuts("    matched = xrtRegexIsMatch(regex, text, textSize);\n", out);
    xl6OutputPuts("    xrtRegexDestroy(regex);\n", out);
    xl6OutputPuts("    return matched > 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("bool xl6_xrt_regex_full_match(const char* text, size_t textSize, const char* pattern, int flags)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xregex* regex;\n", out);
    xl6OutputPuts("    int matched;\n", out);
    xl6OutputPuts("    if ( text == 0 ) { text = \"\"; textSize = 0; }\n", out);
    xl6OutputPuts("    if ( pattern == 0 ) { pattern = \"\"; }\n", out);
    xl6OutputPuts("    regex = xrtRegexCreateEx(pattern, 0, (xregexflags)flags);\n", out);
    xl6OutputPuts("    if ( regex == 0 ) { return false; }\n", out);
    xl6OutputPuts("    matched = xrtRegexFullMatch(regex, text, textSize);\n", out);
    xl6OutputPuts("    xrtRegexDestroy(regex);\n", out);
    xl6OutputPuts("    return matched > 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("xl6_ssize_t xl6_xrt_regex_find(const char* text, size_t textSize, const char* pattern, int flags)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xregex* regex;\n", out);
    xl6OutputPuts("    xregexspan span;\n", out);
    xl6OutputPuts("    int found;\n", out);
    xl6OutputPuts("    if ( text == 0 ) { text = \"\"; textSize = 0; }\n", out);
    xl6OutputPuts("    if ( pattern == 0 ) { pattern = \"\"; }\n", out);
    xl6OutputPuts("    span.iBegin = 0;\n", out);
    xl6OutputPuts("    span.iEnd = 0;\n", out);
    xl6OutputPuts("    regex = xrtRegexCreateEx(pattern, 0, (xregexflags)flags);\n", out);
    xl6OutputPuts("    if ( regex == 0 ) { return (xl6_ssize_t)-1; }\n", out);
    xl6OutputPuts("    found = xrtRegexFind(regex, text, textSize, &span);\n", out);
    xl6OutputPuts("    xrtRegexDestroy(regex);\n", out);
    xl6OutputPuts("    if ( found <= 0 ) { return (xl6_ssize_t)-1; }\n", out);
    xl6OutputPuts("    return (xl6_ssize_t)span.iBegin;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_regex_match(const char* text, size_t textSize, const char* pattern, int flags, size_t start)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xregex* regex;\n", out);
    xl6OutputPuts("    xregexmatch* match;\n", out);
    xl6OutputPuts("    if ( text == 0 ) { text = \"\"; textSize = 0; }\n", out);
    xl6OutputPuts("    if ( pattern == 0 ) { pattern = \"\"; }\n", out);
    xl6OutputPuts("    regex = xrtRegexCreateEx(pattern, 0, (xregexflags)flags);\n", out);
    xl6OutputPuts("    if ( regex == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    match = xrtRegexFindMatchAt(regex, text, textSize, start);\n", out);
    xl6OutputPuts("    xrtRegexDestroy(regex);\n", out);
    xl6OutputPuts("    return match;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_regex_set_create(xvalue patterns, int flags)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    unsigned int count = patterns != 0 ? xvoArrayItemCount(patterns) : 0;\n", out);
    xl6OutputPuts("    const char** texts;\n", out);
    xl6OutputPuts("    unsigned int i;\n", out);
    xl6OutputPuts("    xregexset* set;\n\n", out);
    xl6OutputPuts("    if ( count == 0 ) {\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    texts = (const char**)xrtMalloc(sizeof(char*) * count);\n", out);
    xl6OutputPuts("    if ( texts == 0 ) {\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    for ( i = 0; i < count; ++i ) {\n", out);
    xl6OutputPuts("        const char* item = xvoGetText(xvoArrayGetValue(patterns, i));\n", out);
    xl6OutputPuts("        texts[i] = item != 0 ? item : \"\";\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    set = xrtRegexSetCreateEx(texts, count, (xregexflags)flags);\n", out);
    xl6OutputPuts("    xrtFree(texts);\n", out);
    xl6OutputPuts("    return set;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("long long xl6_xrt_regex_set_destroy(void* set)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xrtRegexSetDestroy((xregexset*)set);\n", out);
    xl6OutputPuts("    return 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("int xl6_xrt_regex_set_is_match(void* set, const char* text, size_t textSize, size_t start)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( text == 0 ) { text = \"\"; textSize = 0; }\n", out);
    xl6OutputPuts("    return xrtRegexSetIsMatchAt((xregexset*)set, text, textSize, start) > 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("long long xl6_xrt_regex_set_first(void* set, const char* text, size_t textSize, size_t start)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( text == 0 ) { text = \"\"; textSize = 0; }\n", out);
    xl6OutputPuts("    return xrtRegexSetFirstAt((xregexset*)set, text, textSize, start);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("xvalue xl6_xrt_regex_set_matches(void* set, const char* text, size_t textSize, size_t start, unsigned int maxCount)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    unsigned int stackIndexes[32];\n", out);
    xl6OutputPuts("    unsigned int* indexes = stackIndexes;\n", out);
    xl6OutputPuts("    unsigned int count = 0;\n", out);
    xl6OutputPuts("    unsigned int capacity = maxCount > 0 ? maxCount : 32u;\n", out);
    xl6OutputPuts("    unsigned int i;\n", out);
    xl6OutputPuts("    xvalue result = xvoCreateArray();\n\n", out);
    xl6OutputPuts("    if ( result == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    if ( text == 0 ) { text = \"\"; textSize = 0; }\n", out);
    xl6OutputPuts("    if ( capacity > 32u ) {\n", out);
    xl6OutputPuts("        indexes = (unsigned int*)xrtMalloc(sizeof(unsigned int) * capacity);\n", out);
    xl6OutputPuts("        if ( indexes == 0 ) { return result; }\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( xrtRegexSetMatchesAt((xregexset*)set, text, textSize, start, indexes, capacity, &count) > 0 ) {\n", out);
    xl6OutputPuts("        for ( i = 0; i < count; ++i ) {\n", out);
    xl6OutputPuts("            xvalue item = xvoCreateInt((long long)indexes[i]);\n", out);
    xl6OutputPuts("            if ( item != 0 && !xvoArrayAppendValue(result, item, true) ) { xvoUnref(item); }\n", out);
    xl6OutputPuts("        }\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( indexes != stackIndexes ) { xrtFree(indexes); }\n", out);
    xl6OutputPuts("    return result;\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("#endif\n\n", out);
}
