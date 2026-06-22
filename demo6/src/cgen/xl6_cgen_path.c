static void xl6CgenEmitPathRuntime(XL6_Output* out)
{
    if ( out == NULL ) {
        return;
    }

    xl6OutputPuts("#ifndef XL6_PATH_RUNTIME\n", out);
    xl6OutputPuts("#define XL6_PATH_RUNTIME\n", out);
    xl6OutputPuts("void* xrtPathJoin(unsigned int iCount, ...);\n\n", out);

    xl6OutputPuts("void* xl6_xrt_path_join(const char* first, const char* second, const char* third, const char* fourth)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( first == 0 ) { first = \"\"; }\n", out);
    xl6OutputPuts("    if ( second == 0 ) { second = \"\"; }\n", out);
    xl6OutputPuts("    if ( third == 0 ) { third = \"\"; }\n", out);
    xl6OutputPuts("    if ( fourth == 0 ) { fourth = \"\"; }\n", out);
    xl6OutputPuts("    if ( third[0] == '\\0' && fourth[0] == '\\0' ) {\n", out);
    xl6OutputPuts("        return xrtPathJoin(2u, first, second);\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( fourth[0] == '\\0' ) {\n", out);
    xl6OutputPuts("        return xrtPathJoin(3u, first, second, third);\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( third[0] == '\\0' ) {\n", out);
    xl6OutputPuts("        return xrtPathJoin(3u, first, second, fourth);\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return xrtPathJoin(4u, first, second, third, fourth);\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("#endif\n\n", out);
}
