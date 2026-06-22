static void xl6CgenEmitStringEdgeRuntime(XL6_Output* out)
{
    if ( out == NULL ) {
        return;
    }

    xl6OutputPuts("#ifndef XL6_STRING_EDGE_RUNTIME\n", out);
    xl6OutputPuts("#define XL6_STRING_EDGE_RUNTIME\n", out);
    xl6OutputPuts("int xrtStrComp(const char* s1, const char* s2, size_t iSize, bool bCase);\n\n", out);

    xl6OutputPuts("bool xl6_xrt_string_starts_with(const char* text, size_t textSize, const char* prefix, size_t prefixSize, bool ignoreCase)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( text == 0 ) { text = \"\"; textSize = 0; }\n", out);
    xl6OutputPuts("    if ( prefix == 0 ) { prefix = \"\"; prefixSize = 0; }\n", out);
    xl6OutputPuts("    if ( prefixSize == 0u ) { return true; }\n", out);
    xl6OutputPuts("    if ( prefixSize > textSize ) { return false; }\n", out);
    xl6OutputPuts("    return xrtStrComp(text, prefix, prefixSize, ignoreCase) == 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("bool xl6_xrt_string_ends_with(const char* text, size_t textSize, const char* suffix, size_t suffixSize, bool ignoreCase)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( text == 0 ) { text = \"\"; textSize = 0; }\n", out);
    xl6OutputPuts("    if ( suffix == 0 ) { suffix = \"\"; suffixSize = 0; }\n", out);
    xl6OutputPuts("    if ( suffixSize == 0u ) { return true; }\n", out);
    xl6OutputPuts("    if ( suffixSize > textSize ) { return false; }\n", out);
    xl6OutputPuts("    return xrtStrComp(text + textSize - suffixSize, suffix, suffixSize, ignoreCase) == 0;\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("#endif\n\n", out);
}
