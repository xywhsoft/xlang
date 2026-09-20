static void xl6CgenEmitStringEdgeRuntime(XL6_Output* out)
{
    if ( out == NULL ) {
        return;
    }

    xl6OutputPuts("#ifndef XL6_STRING_EDGE_RUNTIME\n", out);
    xl6OutputPuts("#define XL6_STRING_EDGE_RUNTIME\n", out);
    xl6OutputPuts("xstrview xrtStrView(const char* text);\n", out);
    xl6OutputPuts("bool xrtStrStarts(xstrview text, xstrview part);\n", out);
    xl6OutputPuts("bool xrtStrCaseStarts(xstrview text, xstrview part);\n", out);
    xl6OutputPuts("bool xrtStrEnds(xstrview text, xstrview part);\n", out);
    xl6OutputPuts("bool xrtStrCaseEnds(xstrview text, xstrview part);\n\n", out);

    xl6OutputPuts("bool xl6_xrt_string_starts_with(const char* text, const char* prefix, bool ignoreCase)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xstrview textView = xrtStrView(text);\n", out);
    xl6OutputPuts("    xstrview prefixView = xrtStrView(prefix);\n\n", out);
    xl6OutputPuts("    return ignoreCase ? xrtStrCaseStarts(textView, prefixView) : xrtStrStarts(textView, prefixView);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("bool xl6_xrt_string_ends_with(const char* text, const char* suffix, bool ignoreCase)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xstrview textView = xrtStrView(text);\n", out);
    xl6OutputPuts("    xstrview suffixView = xrtStrView(suffix);\n\n", out);
    xl6OutputPuts("    return ignoreCase ? xrtStrCaseEnds(textView, suffixView) : xrtStrEnds(textView, suffixView);\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("#endif\n\n", out);
}
