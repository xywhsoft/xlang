static int xl6CgenNodeNeedsTccRuntime(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( node->text != NULL ) {
        if ( (node->kind == XL6_AST_TYPE || node->kind == XL6_AST_NAME) &&
             (xl6CTextEquals(node->text, "TccContext") ||
              xl6CTextEquals(node->text, "TccHandler")) ) {
            return 1;
        }
        if ( node->kind == XL6_AST_NAME &&
             (strncmp(node->text, "__xl_std_tcc_", 13) == 0 ||
              strncmp(node->text, "xl6_tcc_", 8) == 0) ) {
            return 1;
        }
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsTccRuntime(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

static void xl6CgenEmitTccRuntime(XL6_Output* out)
{
    if ( out == NULL ) {
        return;
    }

    xl6OutputPuts("#ifndef XL6_TCC_RUNTIME\n", out);
    xl6OutputPuts("#define XL6_TCC_RUNTIME\n", out);
    xl6OutputPuts("#include <stddef.h>\n", out);
    xl6OutputPuts("#include <string.h>\n", out);
    xl6OutputPuts("void* tcc_new(void);\n", out);
    xl6OutputPuts("void tcc_delete(void* state);\n", out);
    xl6OutputPuts("int tcc_set_output_type(void* state, int outputType);\n", out);
    xl6OutputPuts("int tcc_compile_string(void* state, const char* source);\n", out);
    xl6OutputPuts("int tcc_add_file(void* state, const char* path);\n", out);
    xl6OutputPuts("int tcc_relocate(void* state);\n", out);
    xl6OutputPuts("void* tcc_get_symbol(void* state, const char* name);\n", out);
    xl6OutputPuts("int tcc_output_file(void* state, const char* path);\n", out);
    xl6OutputPuts("int tcc_vfs_mount_memory(const char* path, const void* data, size_t size);\n", out);
    xl6OutputPuts("void tcc_vfs_clear_dynamic(void);\n", out);
    xl6OutputPuts("int xlTccRegisterXrtEnvironment(void* state, int outputType);\n\n", out);

    xl6OutputPuts("static void* xl6_tcc_context_create_with_output(int outputType)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    void* state = tcc_new();\n", out);
    xl6OutputPuts("    if ( state == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    if ( tcc_set_output_type(state, outputType) != 0 ) { tcc_delete(state); return 0; }\n", out);
    xl6OutputPuts("    if ( !xlTccRegisterXrtEnvironment(state, outputType) ) { tcc_delete(state); return 0; }\n", out);
    xl6OutputPuts("    return state;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_tcc_context_create(void)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xl6_tcc_context_create_with_output(1);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_tcc_context_create_output(int outputType)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xl6_tcc_context_create_with_output(outputType);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("long long xl6_tcc_context_destroy(void* state)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( state != 0 ) { tcc_delete(state); }\n", out);
    xl6OutputPuts("    return 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("typedef int (*XL6_TccMainProc)(int, char**);\n", out);
    xl6OutputPuts("typedef struct xl6_tcc_handler { void* state; XL6_TccMainProc mainProc; } XL6_TccHandler;\n\n", out);

    xl6OutputPuts("static int xl6_tcc_run_state(void* state, xvalue* args)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    size_t count = args != 0 ? xrtValueCount(args) : 0u;\n", out);
    xl6OutputPuts("    char** argv = 0;\n", out);
    xl6OutputPuts("    size_t i;\n", out);
    xl6OutputPuts("    typedef int (*xl6_tcc_main_proc)(int, char**);\n", out);
    xl6OutputPuts("    xl6_tcc_main_proc mainProc;\n", out);
    xl6OutputPuts("    int rc;\n", out);
    xl6OutputPuts("    if ( state == 0 ) { return -1; }\n", out);
    xl6OutputPuts("    if ( count > 0u ) {\n", out);
    xl6OutputPuts("        argv = (char**)xrtCalloc(count, sizeof(char*));\n", out);
    xl6OutputPuts("        if ( argv == 0 ) { return -1; }\n", out);
    xl6OutputPuts("        for ( i = 0u; i < count; ++i ) {\n", out);
    xl6OutputPuts("            const char* item = xl6_xrt_value_text(xrtValueArrayGet(args, i));\n", out);
    xl6OutputPuts("            argv[i] = (char*)(item != 0 ? item : \"\");\n", out);
    xl6OutputPuts("        }\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    rc = tcc_relocate(state);\n", out);
    xl6OutputPuts("    if ( rc == 0 ) {\n", out);
    xl6OutputPuts("        mainProc = (xl6_tcc_main_proc)tcc_get_symbol(state, \"main\");\n", out);
    xl6OutputPuts("        rc = mainProc != 0 ? mainProc((int)count, argv) : -1;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( argv != 0 ) { xrtFree(argv); }\n", out);
    xl6OutputPuts("    return rc;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("int xl6_tcc_context_run_args(void* state, xvalue* args)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xl6_tcc_run_state(state, args);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_tcc_handler_wrap(void* value)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return value;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_tcc_handler_create(const char* path)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_TccHandler* handler;\n", out);
    xl6OutputPuts("    if ( path == 0 || path[0] == '\\0' ) { return 0; }\n", out);
    xl6OutputPuts("    handler = (XL6_TccHandler*)xrtCalloc(1u, sizeof(XL6_TccHandler));\n", out);
    xl6OutputPuts("    if ( handler == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    handler->state = xl6_tcc_context_create();\n", out);
    xl6OutputPuts("    if ( handler->state == 0 ) { xrtFree(handler); return 0; }\n", out);
    xl6OutputPuts("    if ( tcc_add_file(handler->state, path) != 0 || tcc_relocate(handler->state) != 0 ) {\n", out);
    xl6OutputPuts("        xl6_tcc_context_destroy(handler->state);\n", out);
    xl6OutputPuts("        xrtFree(handler);\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    handler->mainProc = (XL6_TccMainProc)tcc_get_symbol(handler->state, \"main\");\n", out);
    xl6OutputPuts("    if ( handler->mainProc == 0 ) {\n", out);
    xl6OutputPuts("        xl6_tcc_context_destroy(handler->state);\n", out);
    xl6OutputPuts("        xrtFree(handler);\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return handler;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("long long xl6_tcc_handler_destroy(void* value)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_TccHandler* handler = (XL6_TccHandler*)value;\n", out);
    xl6OutputPuts("    if ( handler != 0 ) {\n", out);
    xl6OutputPuts("        if ( handler->state != 0 ) { xl6_tcc_context_destroy(handler->state); }\n", out);
    xl6OutputPuts("        xrtFree(handler);\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("int xl6_tcc_handler_run_args(void* value, xvalue* args)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_TccHandler* handler = (XL6_TccHandler*)value;\n", out);
    xl6OutputPuts("    size_t count = args != 0 ? xrtValueCount(args) : 0u;\n", out);
    xl6OutputPuts("    char** argv = 0;\n", out);
    xl6OutputPuts("    size_t i;\n", out);
    xl6OutputPuts("    int rc;\n", out);
    xl6OutputPuts("    if ( handler == 0 || handler->mainProc == 0 ) { return -1; }\n", out);
    xl6OutputPuts("    if ( count > 0u ) {\n", out);
    xl6OutputPuts("        argv = (char**)xrtCalloc(count, sizeof(char*));\n", out);
    xl6OutputPuts("        if ( argv == 0 ) { return -1; }\n", out);
    xl6OutputPuts("        for ( i = 0u; i < count; ++i ) {\n", out);
    xl6OutputPuts("            const char* item = xl6_xrt_value_text(xrtValueArrayGet(args, i));\n", out);
    xl6OutputPuts("            argv[i] = (char*)(item != 0 ? item : \"\");\n", out);
    xl6OutputPuts("        }\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    rc = handler->mainProc((int)count, argv);\n", out);
    xl6OutputPuts("    if ( argv != 0 ) { xrtFree(argv); }\n", out);
    xl6OutputPuts("    return rc;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("int xl6_tcc_context_output_file(void* state, const char* path)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("#ifdef XL_RUNTIME_ONLY_DLL\n", out);
    xl6OutputPuts("    (void)state; (void)path; return -1;\n", out);
    xl6OutputPuts("#else\n", out);
    xl6OutputPuts("    if ( state == 0 || path == 0 || path[0] == '\\0' ) { return -1; }\n", out);
    xl6OutputPuts("    return tcc_output_file(state, path);\n", out);
    xl6OutputPuts("#endif\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("int xl6_tcc_run_source(const char* source)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    void* state = xl6_tcc_context_create();\n", out);
    xl6OutputPuts("    int rc;\n", out);
    xl6OutputPuts("    if ( state == 0 ) { return -1; }\n", out);
    xl6OutputPuts("    rc = tcc_compile_string(state, source != 0 ? source : \"\");\n", out);
    xl6OutputPuts("    if ( rc == 0 ) { rc = xl6_tcc_run_state(state, 0); }\n", out);
    xl6OutputPuts("    xl6_tcc_context_destroy(state);\n", out);
    xl6OutputPuts("    return rc;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("int xl6_tcc_run_file(const char* path)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    void* state = xl6_tcc_context_create();\n", out);
    xl6OutputPuts("    int rc;\n", out);
    xl6OutputPuts("    if ( state == 0 || path == 0 || path[0] == '\\0' ) { xl6_tcc_context_destroy(state); return -1; }\n", out);
    xl6OutputPuts("    rc = tcc_add_file(state, path);\n", out);
    xl6OutputPuts("    if ( rc == 0 ) { rc = xl6_tcc_run_state(state, 0); }\n", out);
    xl6OutputPuts("    xl6_tcc_context_destroy(state);\n", out);
    xl6OutputPuts("    return rc;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("int xl6_tcc_vfs_mount_text(const char* path, const char* text)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return tcc_vfs_mount_memory(path != 0 ? path : \"\", text != 0 ? text : \"\", text != 0 ? strlen(text) : 0u);\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("#endif\n\n", out);
}
