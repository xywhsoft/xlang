#include "src/cgen/xl6_cgen_xlang.h"

void xl6EmitXlangRuntimeComponent(XL6_Output* out)
{
    if ( out == NULL ) {
        return;
    }

    xl6OutputPuts("#ifndef XL6_XLANG_EMBED_RUNTIME\n", out);
    xl6OutputPuts("#define XL6_XLANG_EMBED_RUNTIME\n", out);
    xl6OutputPuts("#include <stddef.h>\n", out);
    xl6OutputPuts("#include <stdbool.h>\n", out);
    xl6OutputPuts("#include <string.h>\n", out);
    xl6OutputPuts(
        "#if defined(XL6_COMPILER_SOURCE_VFS)\n"
        "#include <__xl_compiler/include/xl.h>\n"
        "#else\n"
        "#include <xl.h>\n"
        "#endif\n", out);
    xl6OutputPuts("\n", out);

    xl6OutputPuts("void* xl6_xlang_context_create(void)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xlCreateContext(0);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void xl6_xlang_context_destroy(void* ctx)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( ctx != 0 ) { xlDestroyContext((XL_Context*)ctx); }\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xlang_context_last_error_copy(void* ctx)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    const char* text = ctx != 0 ? xlGetLastError((XL_Context*)ctx) : \"\";\n", out);
    xl6OutputPuts("    return xrtStrDup(text != 0 ? text : \"\");\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("bool xl6_xlang_context_set_work_dir(void* ctx, const char* path)\n", out);
    xl6OutputPuts("{ return ctx != 0 && xlSetWorkDir((XL_Context*)ctx, path) != 0; }\n\n", out);
    xl6OutputPuts("bool xl6_xlang_context_set_temp_dir(void* ctx, const char* path)\n", out);
    xl6OutputPuts("{ return ctx != 0 && xlSetTempDir((XL_Context*)ctx, path) != 0; }\n\n", out);
    xl6OutputPuts("bool xl6_xlang_context_set_save_temps(void* ctx, bool enabled)\n", out);
    xl6OutputPuts("{ return ctx != 0 && xlSetSaveTemps((XL_Context*)ctx, enabled ? 1 : 0) != 0; }\n\n", out);
    xl6OutputPuts("bool xl6_xlang_context_run_source(void* ctx, const char* source)\n", out);
    xl6OutputPuts("{ return ctx != 0 && xlRunScriptString((XL_Context*)ctx, source) != 0; }\n\n", out);
    xl6OutputPuts("bool xl6_xlang_context_run_file(void* ctx, const char* path)\n", out);
    xl6OutputPuts("{ return ctx != 0 && xlRunScriptFile((XL_Context*)ctx, path) != 0; }\n\n", out);
    xl6OutputPuts("bool xl6_xlang_context_run_source_args(void* ctx, const char* source, xvalue* args)\n", out);
    xl6OutputPuts("{ return ctx != 0 && xlRunScriptStringArg((XL_Context*)ctx, source, args) != 0; }\n\n", out);
    xl6OutputPuts("bool xl6_xlang_context_run_file_args(void* ctx, const char* path, xvalue* args)\n", out);
    xl6OutputPuts("{ return ctx != 0 && xlRunScriptFileArg((XL_Context*)ctx, path, args) != 0; }\n\n", out);
    xl6OutputPuts("void xl6_xlang_context_clear_last_error(void* ctx)\n", out);
    xl6OutputPuts("{ if ( ctx != 0 ) xlClearLastError((XL_Context*)ctx); }\n\n", out);

    xl6OutputPuts("typedef struct xl6_xlang_handler { XL_Context* ctx; XL_Module* module; xmutex* lock; } XL6_XlangHandler;\n\n", out);

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

    xl6OutputPuts("void xl6_xlang_handler_destroy(void* value)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_XlangHandler* handler = (XL6_XlangHandler*)value;\n", out);
    xl6OutputPuts("    if ( handler != 0 ) {\n", out);
    xl6OutputPuts("        if ( handler->module != 0 ) { xlDestroyModule(handler->module); }\n", out);
    xl6OutputPuts("        if ( handler->ctx != 0 ) { xl6_xlang_context_destroy(handler->ctx); }\n", out);
    xl6OutputPuts("        if ( handler->lock != 0 ) { xrtMutexDestroy(handler->lock); }\n", out);
    xl6OutputPuts("        xrtFree(handler);\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("bool xl6_xlang_handler_run_args(void* value, xvalue* args)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_XlangHandler* handler = (XL6_XlangHandler*)value;\n", out);
    xl6OutputPuts("    int result;\n", out);
    xl6OutputPuts("    if ( handler == 0 || handler->ctx == 0 || handler->module == 0 || handler->lock == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    xrtMutexLock(handler->lock);\n", out);
    xl6OutputPuts("    result = xlRunModuleMain(handler->ctx, handler->module, args);\n", out);
    xl6OutputPuts("    xrtMutexUnlock(handler->lock);\n", out);
    xl6OutputPuts("    return result != 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xlang_handler_last_error_copy(void* value)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_XlangHandler* handler = (XL6_XlangHandler*)value;\n", out);
    xl6OutputPuts("    return xl6_xlang_context_last_error_copy(handler != 0 ? handler->ctx : 0);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("int xl6_xlang_run_source(const char* source)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL_Context* ctx = (XL_Context*)xl6_xlang_context_create();\n", out);
    xl6OutputPuts("    int ok;\n", out);
    xl6OutputPuts("    if ( ctx == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    ok = xlRunScriptString(ctx, source != 0 ? source : \"\");\n", out);
    xl6OutputPuts("    xl6_xlang_context_destroy(ctx);\n", out);
    xl6OutputPuts("    return ok;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("int xl6_xlang_run_file(const char* path)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL_Context* ctx = (XL_Context*)xl6_xlang_context_create();\n", out);
    xl6OutputPuts("    int ok;\n", out);
    xl6OutputPuts("    if ( ctx == 0 || path == 0 || path[0] == '\\0' ) { xl6_xlang_context_destroy(ctx); return 0; }\n", out);
    xl6OutputPuts("    ok = xlRunScriptFile(ctx, path);\n", out);
    xl6OutputPuts("    xl6_xlang_context_destroy(ctx);\n", out);
    xl6OutputPuts("    return ok;\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("#endif\n\n", out);
}
