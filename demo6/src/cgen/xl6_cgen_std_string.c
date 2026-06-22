static void xl6CgenEmitStdStringRuntime(XL6_Output* out)
{
    if ( out == NULL ) {
        return;
    }

    xl6OutputPuts("#ifndef XL6_STD_STRING_RUNTIME\n", out);
    xl6OutputPuts("#define XL6_STD_STRING_RUNTIME\n", out);
    xl6OutputPuts("#ifndef XL6_XVALUE_TYPEDEF\n", out);
    xl6OutputPuts("#define XL6_XVALUE_TYPEDEF\n", out);
    xl6OutputPuts("typedef struct xvalue_struct* xvalue;\n", out);
    xl6OutputPuts("#endif\n", out);
    xl6OutputPuts("void xrtFree(void* p);\n", out);
    xl6OutputPuts("char** xrtSplit(const char* sText, size_t iSize, const char* sSepText, size_t iSepSize, bool bSrcRevise, size_t* iRetSize);\n", out);
    xl6OutputPuts("xvalue xvoCreateArray(void);\n", out);
    xl6OutputPuts("xvalue xvoCreateText(void* sVal, unsigned int iSize, bool bColloc);\n", out);
    xl6OutputPuts("bool xvoArrayAppendValue(xvalue pArr, xvalue pVal, bool bColloc);\n", out);
    xl6OutputPuts("void xvoUnref(xvalue pVal);\n\n", out);

    xl6OutputPuts("xvalue xl6_xrt_string_split(const char* text, const char* sep)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    size_t count = 0;\n", out);
    xl6OutputPuts("    size_t i;\n", out);
    xl6OutputPuts("    char** parts;\n", out);
    xl6OutputPuts("    xvalue result = xvoCreateArray();\n\n", out);
    xl6OutputPuts("    if ( result == 0 ) {\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    parts = xrtSplit(text, 0, sep, 0, false, &count);\n", out);
    xl6OutputPuts("    if ( parts == 0 ) {\n", out);
    xl6OutputPuts("        return result;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( count == 0 ) {\n", out);
    xl6OutputPuts("        xrtFree(parts);\n", out);
    xl6OutputPuts("        return result;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    for ( i = 0; i < count; ++i ) {\n", out);
    xl6OutputPuts("        xvalue item = xvoCreateText(parts[i], 0, false);\n", out);
    xl6OutputPuts("        if ( item != 0 && !xvoArrayAppendValue(result, item, true) ) {\n", out);
    xl6OutputPuts("            xvoUnref(item);\n", out);
    xl6OutputPuts("        }\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    xrtFree(parts);\n", out);
    xl6OutputPuts("    return result;\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("#endif\n\n", out);
}
