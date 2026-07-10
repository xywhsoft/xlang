static void xl6CgenEmitPathRuntime(XL6_Output* out)
{
    if ( out == NULL ) {
        return;
    }

    xl6OutputPuts("#ifndef XL6_PATH_RUNTIME\n", out);
    xl6OutputPuts("#define XL6_PATH_RUNTIME\n", out);
    xl6OutputPuts("void* xrtPathJoin(unsigned int iCount, ...);\n", out);
    xl6OutputPuts("void xrtFree(void* pMem);\n", out);
    xl6OutputPuts("unsigned int xvoArrayItemCount(xvalue pArr);\n", out);
    xl6OutputPuts("xvalue xvoArrayGetValue(xvalue pArr, unsigned int index);\n", out);
    xl6OutputPuts("const char* xvoGetText(xvalue pVal);\n\n", out);

    xl6OutputPuts("void* xl6_xrt_path_join(const char* first, const char* second, xvalue rest)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    unsigned int count = rest != 0 ? xvoArrayItemCount(rest) : 0u;\n", out);
    xl6OutputPuts("    unsigned int i;\n", out);
    xl6OutputPuts("    char* joined;\n", out);
    xl6OutputPuts("    if ( first == 0 ) { first = \"\"; }\n", out);
    xl6OutputPuts("    if ( second == 0 ) { second = \"\"; }\n", out);
    xl6OutputPuts("    joined = (char*)xrtPathJoin(2u, first, second);\n", out);
    xl6OutputPuts("    if ( joined == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    for ( i = 0u; i < count; ++i ) {\n", out);
    xl6OutputPuts("        const char* part = xvoGetText(xvoArrayGetValue(rest, i));\n", out);
    xl6OutputPuts("        char* next = (char*)xrtPathJoin(2u, joined, part != 0 ? part : \"\");\n", out);
    xl6OutputPuts("        xrtFree(joined);\n", out);
    xl6OutputPuts("        if ( next == 0 ) { return 0; }\n", out);
    xl6OutputPuts("        joined = next;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return joined;\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("#endif\n\n", out);
}
