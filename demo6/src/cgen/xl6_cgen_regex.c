static void xl6CgenEmitRegexRuntime(XL6_Output* out)
{
    if ( out == NULL ) {
        return;
    }

    xl6OutputPuts("#ifndef XL6_REGEX_RUNTIME\n", out);
    xl6OutputPuts("#define XL6_REGEX_RUNTIME\n", out);
    xl6OutputPuts("typedef struct xrt_regex xregex;\n", out);
    xl6OutputPuts("typedef struct { size_t iBegin; size_t iEnd; } xregexspan;\n", out);
    xl6OutputPuts("xregex* xrtRegexCreate(const char* sPatternNt);\n", out);
    xl6OutputPuts("void xrtRegexDestroy(xregex* pRegex);\n", out);
    xl6OutputPuts("int xrtRegexIsMatch(xregex* pRegex, const char* sText, size_t iTextSize);\n", out);
    xl6OutputPuts("int xrtRegexFind(xregex* pRegex, const char* sText, size_t iTextSize, xregexspan* pOutSpan);\n\n", out);

    xl6OutputPuts("bool xl6_xrt_regex_is_match(const char* text, size_t textSize, const char* pattern)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xregex* regex;\n", out);
    xl6OutputPuts("    int matched;\n", out);
    xl6OutputPuts("    if ( text == 0 ) { text = \"\"; textSize = 0; }\n", out);
    xl6OutputPuts("    if ( pattern == 0 ) { pattern = \"\"; }\n", out);
    xl6OutputPuts("    regex = xrtRegexCreate(pattern);\n", out);
    xl6OutputPuts("    if ( regex == 0 ) { return false; }\n", out);
    xl6OutputPuts("    matched = xrtRegexIsMatch(regex, text, textSize);\n", out);
    xl6OutputPuts("    xrtRegexDestroy(regex);\n", out);
    xl6OutputPuts("    return matched > 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("xl6_ssize_t xl6_xrt_regex_find(const char* text, size_t textSize, const char* pattern)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xregex* regex;\n", out);
    xl6OutputPuts("    xregexspan span;\n", out);
    xl6OutputPuts("    int found;\n", out);
    xl6OutputPuts("    if ( text == 0 ) { text = \"\"; textSize = 0; }\n", out);
    xl6OutputPuts("    if ( pattern == 0 ) { pattern = \"\"; }\n", out);
    xl6OutputPuts("    span.iBegin = 0;\n", out);
    xl6OutputPuts("    span.iEnd = 0;\n", out);
    xl6OutputPuts("    regex = xrtRegexCreate(pattern);\n", out);
    xl6OutputPuts("    if ( regex == 0 ) { return (xl6_ssize_t)-1; }\n", out);
    xl6OutputPuts("    found = xrtRegexFind(regex, text, textSize, &span);\n", out);
    xl6OutputPuts("    xrtRegexDestroy(regex);\n", out);
    xl6OutputPuts("    if ( found <= 0 ) { return (xl6_ssize_t)-1; }\n", out);
    xl6OutputPuts("    return (xl6_ssize_t)span.iBegin;\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("#endif\n\n", out);
}
