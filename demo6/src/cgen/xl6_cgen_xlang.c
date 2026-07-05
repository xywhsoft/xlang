static int xl6CgenNodeNeedsXlangRuntime(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( node->text != NULL ) {
        if ( (node->kind == XL6_AST_TYPE || node->kind == XL6_AST_NAME) &&
             xl6CTextEquals(node->text, "XLangContext") ) {
            return 1;
        }
        if ( node->kind == XL6_AST_NAME &&
             (strncmp(node->text, "__xl_std_xlang_", 16) == 0 ||
              strncmp(node->text, "xl6_xlang_", 10) == 0) ) {
            return 1;
        }
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsXlangRuntime(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

static void xl6CgenEmitXlangRuntime(XL6_Output* out)
{
    if ( out == NULL ) {
        return;
    }

    xl6OutputPuts("#ifndef XL6_XLANG_EMBED_RUNTIME\n", out);
    xl6OutputPuts("#define XL6_XLANG_EMBED_RUNTIME\n", out);
    xl6OutputPuts("#include <stddef.h>\n", out);
    xl6OutputPuts("void* xlCreateContext(const void* options);\n", out);
    xl6OutputPuts("void xlDestroyContext(void* ctx);\n", out);
    xl6OutputPuts("int xlSetWorkDir(void* ctx, const char* path);\n", out);
    xl6OutputPuts("int xlSetTempDir(void* ctx, const char* path);\n", out);
    xl6OutputPuts("int xlSetSaveTemps(void* ctx, int enabled);\n", out);
    xl6OutputPuts("int xlRunScriptString(void* ctx, const char* source);\n", out);
    xl6OutputPuts("int xlRunScriptFile(void* ctx, const char* path);\n", out);
    xl6OutputPuts("const char* xlGetLastError(void* ctx);\n", out);
    xl6OutputPuts("void xlClearLastError(void* ctx);\n", out);
    xl6OutputPuts("char* xrtCopyStr(const char* sText, size_t iSize);\n\n", out);

    xl6OutputPuts("void* xl6_xlang_context_create(void)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xlCreateContext(0);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("long long xl6_xlang_context_destroy(void* ctx)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( ctx != 0 ) { xlDestroyContext(ctx); }\n", out);
    xl6OutputPuts("    return 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xlang_context_last_error_copy(void* ctx)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    const char* text = ctx != 0 ? xlGetLastError(ctx) : \"\";\n", out);
    xl6OutputPuts("    return xrtCopyStr(text != 0 ? text : \"\", 0u);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("int xl6_xlang_run_source(const char* source)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    void* ctx = xl6_xlang_context_create();\n", out);
    xl6OutputPuts("    int ok;\n", out);
    xl6OutputPuts("    if ( ctx == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    ok = xlRunScriptString(ctx, source != 0 ? source : \"\");\n", out);
    xl6OutputPuts("    xl6_xlang_context_destroy(ctx);\n", out);
    xl6OutputPuts("    return ok;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("int xl6_xlang_run_file(const char* path)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    void* ctx = xl6_xlang_context_create();\n", out);
    xl6OutputPuts("    int ok;\n", out);
    xl6OutputPuts("    if ( ctx == 0 || path == 0 || path[0] == '\\0' ) { xl6_xlang_context_destroy(ctx); return 0; }\n", out);
    xl6OutputPuts("    ok = xlRunScriptFile(ctx, path);\n", out);
    xl6OutputPuts("    xl6_xlang_context_destroy(ctx);\n", out);
    xl6OutputPuts("    return ok;\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("#endif\n\n", out);
}
