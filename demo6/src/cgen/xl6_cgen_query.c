static void xl6CgenEmitQueryRuntime(XL6_Output* out)
{
    if ( out == NULL ) {
        return;
    }

    xl6OutputPuts("#ifndef XL6_QUERY_RUNTIME\n", out);
    xl6OutputPuts("#define XL6_QUERY_RUNTIME\n", out);
    xl6OutputPuts("#include <string.h>\n", out);
    xl6OutputPuts("void* xrtMalloc(size_t iSize);\n", out);
    xl6OutputPuts("void xrtFree(void* p);\n", out);
    xl6OutputPuts("bool xrtPercentEncodeTo(const char* sText, size_t iLen, char* sOut, size_t iOutCap, size_t* pOutLen, bool bSpaceAsPlus);\n", out);
    xl6OutputPuts("bool xrtFormUrlEncodedDecodeTo(const char* sText, size_t iLen, char* sOut, size_t iOutCap, size_t* pOutLen);\n", out);
    xl6OutputPuts("bool xrtFormUrlEncodedAppendFieldTo(char* sOut, size_t iOutCap, size_t* pOffset, const char* sName, size_t iNameLen, const char* sValue, size_t iValueLen, bool bHasValue);\n", out);
    xl6OutputPuts("bool xrtFormUrlEncodedValidateN(const char* sText, size_t iLen, const void* pLimits);\n\n", out);

    xl6OutputPuts("static void* xl6_query_empty_string(void)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    char* text = (char*)xrtMalloc(1);\n", out);
    xl6OutputPuts("    if ( text != 0 ) { text[0] = '\\0'; }\n", out);
    xl6OutputPuts("    return text;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static int xl6_query_mul3_add(size_t a, size_t b, size_t extra, size_t* out)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    size_t max = (size_t)-1;\n", out);
    xl6OutputPuts("    if ( out == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    if ( a > (max - extra) / 3u ) { return 0; }\n", out);
    xl6OutputPuts("    *out = a * 3u + extra;\n", out);
    xl6OutputPuts("    if ( b > (max - *out) / 3u ) { return 0; }\n", out);
    xl6OutputPuts("    *out += b * 3u;\n", out);
    xl6OutputPuts("    return 1;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_query_encode(const char* text, size_t size)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    char* outText;\n", out);
    xl6OutputPuts("    size_t cap;\n", out);
    xl6OutputPuts("    size_t outLen = 0;\n", out);
    xl6OutputPuts("    if ( text == 0 ) { text = \"\"; size = 0; }\n", out);
    xl6OutputPuts("    if ( !xl6_query_mul3_add(size, 0, 1, &cap) ) { return xl6_query_empty_string(); }\n", out);
    xl6OutputPuts("    outText = (char*)xrtMalloc(cap);\n", out);
    xl6OutputPuts("    if ( outText == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    if ( !xrtPercentEncodeTo(text, size, outText, cap, &outLen, true) ) {\n", out);
    xl6OutputPuts("        xrtFree(outText);\n", out);
    xl6OutputPuts("        return xl6_query_empty_string();\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    outText[outLen] = '\\0';\n", out);
    xl6OutputPuts("    return outText;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_query_decode(const char* text, size_t size)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    char* outText;\n", out);
    xl6OutputPuts("    size_t outLen = 0;\n", out);
    xl6OutputPuts("    if ( text == 0 ) { text = \"\"; size = 0; }\n", out);
    xl6OutputPuts("    outText = (char*)xrtMalloc(size + 1);\n", out);
    xl6OutputPuts("    if ( outText == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    if ( !xrtFormUrlEncodedDecodeTo(text, size, outText, size + 1, &outLen) ) {\n", out);
    xl6OutputPuts("        xrtFree(outText);\n", out);
    xl6OutputPuts("        return xl6_query_empty_string();\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    outText[outLen] = '\\0';\n", out);
    xl6OutputPuts("    return outText;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("bool xl6_xrt_query_valid(const char* text, size_t size)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( text == 0 ) { text = \"\"; size = 0; }\n", out);
    xl6OutputPuts("    return xrtFormUrlEncodedValidateN(text, size, 0);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_query_pair(const char* name, size_t nameSize, const char* value, size_t valueSize)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    char* outText;\n", out);
    xl6OutputPuts("    size_t cap;\n", out);
    xl6OutputPuts("    size_t off = 0;\n", out);
    xl6OutputPuts("    if ( name == 0 ) { name = \"\"; nameSize = 0; }\n", out);
    xl6OutputPuts("    if ( value == 0 ) { value = \"\"; valueSize = 0; }\n", out);
    xl6OutputPuts("    if ( !xl6_query_mul3_add(nameSize, valueSize, 2, &cap) ) { return xl6_query_empty_string(); }\n", out);
    xl6OutputPuts("    outText = (char*)xrtMalloc(cap);\n", out);
    xl6OutputPuts("    if ( outText == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    outText[0] = '\\0';\n", out);
    xl6OutputPuts("    if ( !xrtFormUrlEncodedAppendFieldTo(outText, cap, &off, name, nameSize, value, valueSize, true) ) {\n", out);
    xl6OutputPuts("        xrtFree(outText);\n", out);
    xl6OutputPuts("        return xl6_query_empty_string();\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    outText[off] = '\\0';\n", out);
    xl6OutputPuts("    return outText;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_query_append(const char* query, size_t querySize, const char* name, size_t nameSize, const char* value, size_t valueSize)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    char* outText;\n", out);
    xl6OutputPuts("    size_t pairCap;\n", out);
    xl6OutputPuts("    size_t cap;\n", out);
    xl6OutputPuts("    size_t off;\n", out);
    xl6OutputPuts("    if ( query == 0 ) { query = \"\"; querySize = 0; }\n", out);
    xl6OutputPuts("    if ( name == 0 ) { name = \"\"; nameSize = 0; }\n", out);
    xl6OutputPuts("    if ( value == 0 ) { value = \"\"; valueSize = 0; }\n", out);
    xl6OutputPuts("    if ( !xl6_query_mul3_add(nameSize, valueSize, 2, &pairCap) ) { return xl6_query_empty_string(); }\n", out);
    xl6OutputPuts("    if ( querySize > ((size_t)-1) - pairCap - 1 ) { return xl6_query_empty_string(); }\n", out);
    xl6OutputPuts("    cap = querySize + pairCap + 1;\n", out);
    xl6OutputPuts("    outText = (char*)xrtMalloc(cap);\n", out);
    xl6OutputPuts("    if ( outText == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    if ( querySize > 0 ) { memcpy(outText, query, querySize); }\n", out);
    xl6OutputPuts("    off = querySize;\n", out);
    xl6OutputPuts("    outText[off] = '\\0';\n", out);
    xl6OutputPuts("    if ( !xrtFormUrlEncodedAppendFieldTo(outText, cap, &off, name, nameSize, value, valueSize, true) ) {\n", out);
    xl6OutputPuts("        xrtFree(outText);\n", out);
    xl6OutputPuts("        return xl6_query_empty_string();\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    outText[off] = '\\0';\n", out);
    xl6OutputPuts("    return outText;\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("#endif\n\n", out);
}
