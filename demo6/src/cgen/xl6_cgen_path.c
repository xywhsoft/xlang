static void xl6CgenEmitPathRuntime(XL6_Output* out)
{
    if ( out == NULL ) {
        return;
    }

    xl6OutputPuts("#ifndef XL6_PATH_RUNTIME\n", out);
    xl6OutputPuts("#define XL6_PATH_RUNTIME\n", out);
    /* xl_runtime.h 是唯一的 XRT 路径 ABI 来源。 */

    xl6OutputPuts("void* xl6_xrt_path_random(const char* head, size_t headSize, const char* foot, size_t footSize, size_t length)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    char* randomText;\n", out);
    xl6OutputPuts("    char* result;\n", out);
    xl6OutputPuts("    size_t total;\n", out);
    xl6OutputPuts("    if ( head == 0 ) { head = \"\"; headSize = 0u; } else if ( headSize == 0u ) { headSize = strlen(head); }\n", out);
    xl6OutputPuts("    if ( foot == 0 ) { foot = \"\"; footSize = 0u; } else if ( footSize == 0u ) { footSize = strlen(foot); }\n", out);
    xl6OutputPuts("    if ( headSize > (SIZE_MAX - length) || (headSize + length) > (SIZE_MAX - footSize) || (headSize + length + footSize) == SIZE_MAX ) { return 0; }\n", out);
    xl6OutputPuts("    randomText = (char*)xrtRandString(length);\n", out);
    xl6OutputPuts("    if ( randomText == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    total = headSize + length + footSize;\n", out);
    xl6OutputPuts("    result = (char*)xrtMalloc(total + 1u);\n", out);
    xl6OutputPuts("    if ( result == 0 ) { xrtFree(randomText); return 0; }\n", out);
    xl6OutputPuts("    if ( headSize > 0u ) { memcpy(result, head, headSize); }\n", out);
    xl6OutputPuts("    if ( length > 0u ) { memcpy(result + headSize, randomText, length); }\n", out);
    xl6OutputPuts("    if ( footSize > 0u ) { memcpy(result + headSize + length, foot, footSize); }\n", out);
    xl6OutputPuts("    result[total] = '\\0';\n", out);
    xl6OutputPuts("    xrtFree(randomText);\n", out);
    xl6OutputPuts("    return result;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_path_sep_text(void)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    char* result = (char*)xrtMalloc(2u);\n", out);
    xl6OutputPuts("    if ( result == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    result[0] = xrtPathSep();\n", out);
    xl6OutputPuts("    result[1] = '\\0';\n", out);
    xl6OutputPuts("    return result;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_path_clean(const char* path)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( path == 0 ) { path = \"\"; }\n", out);
    xl6OutputPuts("    return xrtPathClean((xstrview){ path, strlen(path) }, 0);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_path_join(const char* first, const char* second, xvalue* rest)\n", out);
    xl6OutputPuts("{\n", out);
	xl6OutputPuts("    size_t count = rest != 0 ? xrtValueCount(rest) : 0u;\n", out);
	xl6OutputPuts("    size_t i;\n", out);
    xl6OutputPuts("    char* joined;\n", out);
    xl6OutputPuts("    if ( first == 0 ) { first = \"\"; }\n", out);
    xl6OutputPuts("    if ( second == 0 ) { second = \"\"; }\n", out);
    xl6OutputPuts("    joined = (char*)xrtPathJoin(first, second);\n", out);
    xl6OutputPuts("    if ( joined == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    for ( i = 0u; i < count; ++i ) {\n", out);
    xl6OutputPuts("        const char* part = xl6_xrt_value_text(xrtValueArrayGet(rest, i));\n", out);
    xl6OutputPuts("        char* next = (char*)xrtPathJoin(joined, part != 0 ? part : \"\");\n", out);
    xl6OutputPuts("        xrtFree(joined);\n", out);
    xl6OutputPuts("        if ( next == 0 ) { return 0; }\n", out);
    xl6OutputPuts("        joined = next;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return joined;\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("#endif\n\n", out);
}
