static int xl6CgenNodeNeedsXlangRuntime(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( node->text != NULL ) {
        if ( node->kind == XL6_AST_IMPORT &&
             (xl6CTextEquals(node->text, "xlang") ||
              xl6CTextEquals(node->text, "xlang.compiler")) ) {
            return 1;
        }
        if ( (node->kind == XL6_AST_TYPE || node->kind == XL6_AST_NAME) &&
             (xl6CTextEquals(node->text, "XLangContext") ||
              xl6CTextEquals(node->text, "XLangHandler")) ) {
            return 1;
        }
        if ( node->kind == XL6_AST_NAME &&
             (strncmp(node->text, "__xl_std_xlang_", 15) == 0 ||
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
    xl6OutputPuts("#include <string.h>\n", out);
    xl6OutputPuts("typedef struct xvalue xvalue;\n", out);
    xl6OutputPuts("void* xlCreateContext(const void* options);\n", out);
    xl6OutputPuts("void xlDestroyContext(void* ctx);\n", out);
    xl6OutputPuts("int xlSetWorkDir(void* ctx, const char* path);\n", out);
    xl6OutputPuts("int xlSetTempDir(void* ctx, const char* path);\n", out);
    xl6OutputPuts("int xlSetSaveTemps(void* ctx, int enabled);\n", out);
    xl6OutputPuts("int xlRunScriptString(void* ctx, const char* source);\n", out);
    xl6OutputPuts("int xlRunScriptFile(void* ctx, const char* path);\n", out);
    xl6OutputPuts("int xlRunScriptStringArg(void* ctx, const char* source, xvalue* arg);\n", out);
    xl6OutputPuts("int xlRunScriptFileArg(void* ctx, const char* path, xvalue* arg);\n", out);
    xl6OutputPuts("void* xlCompileScriptFileToModule(void* ctx, const char* path);\n", out);
    xl6OutputPuts("int xlRunModuleMain(void* ctx, void* module, xvalue* arg);\n", out);
    xl6OutputPuts("void xlDestroyModule(void* module);\n", out);
    xl6OutputPuts("const char* xlGetLastError(void* ctx);\n", out);
    xl6OutputPuts("void xlClearLastError(void* ctx);\n", out);
    xl6OutputPuts("static inline char* xl6_xrt_string_dup(const char* sText, size_t iSize);\n", out);
    xl6OutputPuts("\n", out);

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
    xl6OutputPuts("    return xl6_xrt_string_dup(text != 0 ? text : \"\", 0u);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("typedef struct xl6_xlang_handler { void* ctx; void* module; xmutex* lock; } XL6_XlangHandler;\n\n", out);

    xl6OutputPuts("void* xl6_xlang_handler_wrap(void* value)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return value;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xlang_handler_create(const char* path, const char* workDir)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_XlangHandler* handler;\n", out);
    xl6OutputPuts("    if ( path == 0 || path[0] == '\\0' ) { return 0; }\n", out);
    xl6OutputPuts("    handler = (XL6_XlangHandler*)xrtMalloc(sizeof(XL6_XlangHandler));\n", out);
    xl6OutputPuts("    if ( handler == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    memset(handler, 0, sizeof(XL6_XlangHandler));\n", out);
    xl6OutputPuts("    handler->ctx = xl6_xlang_context_create();\n", out);
    xl6OutputPuts("    handler->lock = xrtMutexCreate();\n", out);
    xl6OutputPuts("    if ( handler->ctx == 0 || handler->lock == 0 ) {\n", out);
    xl6OutputPuts("        if ( handler->ctx != 0 ) { xl6_xlang_context_destroy(handler->ctx); }\n", out);
    xl6OutputPuts("        if ( handler->lock != 0 ) { xrtMutexDestroy(handler->lock); }\n", out);
    xl6OutputPuts("        xrtFree(handler);\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( workDir != 0 && workDir[0] != '\\0' ) { (void)xlSetWorkDir(handler->ctx, workDir); }\n", out);
    xl6OutputPuts("    handler->module = xlCompileScriptFileToModule(handler->ctx, path);\n", out);
    xl6OutputPuts("    if ( handler->module == 0 ) {\n", out);
    xl6OutputPuts("        xrtMutexDestroy(handler->lock);\n", out);
    xl6OutputPuts("        xl6_xlang_context_destroy(handler->ctx);\n", out);
    xl6OutputPuts("        xrtFree(handler);\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return handler;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("long long xl6_xlang_handler_destroy(void* value)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_XlangHandler* handler = (XL6_XlangHandler*)value;\n", out);
    xl6OutputPuts("    if ( handler != 0 ) {\n", out);
    xl6OutputPuts("        if ( handler->module != 0 ) { xlDestroyModule(handler->module); }\n", out);
    xl6OutputPuts("        if ( handler->ctx != 0 ) { xl6_xlang_context_destroy(handler->ctx); }\n", out);
    xl6OutputPuts("        if ( handler->lock != 0 ) { xrtMutexDestroy(handler->lock); }\n", out);
    xl6OutputPuts("        xrtFree(handler);\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("int xl6_xlang_handler_run_args(void* value, xvalue* args)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_XlangHandler* handler = (XL6_XlangHandler*)value;\n", out);
    xl6OutputPuts("    int result;\n", out);
    xl6OutputPuts("    if ( handler == 0 || handler->ctx == 0 || handler->module == 0 || handler->lock == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    xrtMutexLock(handler->lock);\n", out);
    xl6OutputPuts("    result = xlRunModuleMain(handler->ctx, handler->module, args);\n", out);
    xl6OutputPuts("    xrtMutexUnlock(handler->lock);\n", out);
    xl6OutputPuts("    return result;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xlang_handler_last_error_copy(void* value)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_XlangHandler* handler = (XL6_XlangHandler*)value;\n", out);
    xl6OutputPuts("    return xl6_xlang_context_last_error_copy(handler != 0 ? handler->ctx : 0);\n", out);
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
