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
    xl6OutputPuts("#if defined(XL6_COMPILER_SOURCE_VFS)\n#include <__xl_compiler/include/xl_package.h>\n#else\n#include <xl_package.h>\n#endif\n",out);
    xl6OutputPuts("static const XL_CompiledPackage* xlir_compiled_package_lookup(const char*);\n",out);
    xl6OutputPuts("void* xl6_xlang_compiled_package(xlir_string name) { const char* text = xlir_string_cstr(name); return text ? (void*)xlir_compiled_package_lookup(text) : NULL; }\n",out);
    xl6OutputPuts("bool xl6_xlang_context_import_package(void* context, void* package) { return context && xlImportCompiledPackage((XL_Context*)context,(const XL_CompiledPackage*)package) != 0; }\n",out);

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

    xl6OutputPuts("bool xl6_xlang_context_set_work_dir(void* ctx, xlir_string path)\n", out);
    xl6OutputPuts("{ const char* text = xlir_string_cstr(path); return text != 0 && ctx != 0 && xlSetWorkDir((XL_Context*)ctx, text) != 0; }\n\n", out);
    xl6OutputPuts("bool xl6_xlang_context_set_temp_dir(void* ctx, xlir_string path)\n", out);
    xl6OutputPuts("{ const char* text = xlir_string_cstr(path); return text != 0 && ctx != 0 && xlSetTempDir((XL_Context*)ctx, text) != 0; }\n\n", out);
    xl6OutputPuts("bool xl6_xlang_context_set_save_temps(void* ctx, bool enabled)\n", out);
    xl6OutputPuts("{ return ctx != 0 && xlSetSaveTemps((XL_Context*)ctx, enabled ? 1 : 0) != 0; }\n\n", out);
    xl6OutputPuts("bool xl6_xlang_context_run_source(void* ctx, xlir_string source)\n", out);
    xl6OutputPuts("{ const char* text = xlir_string_cstr(source); return text != 0 && ctx != 0 && xlRunScriptString((XL_Context*)ctx, text) != 0; }\n\n", out);
    xl6OutputPuts("bool xl6_xlang_context_run_file(void* ctx, xlir_string path)\n", out);
    xl6OutputPuts("{ const char* text = xlir_string_cstr(path); return text != 0 && ctx != 0 && xlRunScriptFile((XL_Context*)ctx, text) != 0; }\n\n", out);
    xl6OutputPuts("bool xl6_xlang_context_run_source_args(void* ctx, xlir_string source, xvalue* args)\n", out);
    xl6OutputPuts("{ const char* text = xlir_string_cstr(source); return text != 0 && ctx != 0 && xlRunScriptStringArg((XL_Context*)ctx, text, args) != 0; }\n\n", out);
    xl6OutputPuts("bool xl6_xlang_context_run_file_args(void* ctx, xlir_string path, xvalue* args)\n", out);
    xl6OutputPuts("{ const char* text = xlir_string_cstr(path); return text != 0 && ctx != 0 && xlRunScriptFileArg((XL_Context*)ctx, text, args) != 0; }\n\n", out);
    xl6OutputPuts("void xl6_xlang_context_clear_last_error(void* ctx)\n", out);
    xl6OutputPuts("{ if ( ctx != 0 ) xlClearLastError((XL_Context*)ctx); }\n\n", out);

    xl6OutputPuts("#include <__xl_runtime/xl6_xlang_handler.h>\n",out);

    xl6OutputPuts("int xl6_xlang_run_source(xlir_string source)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    const char* text = xlir_string_cstr(source);\n", out);
    xl6OutputPuts("    XL_Context* ctx = text != 0 ? (XL_Context*)xl6_xlang_context_create() : 0;\n", out);
    xl6OutputPuts("    int ok;\n", out);
    xl6OutputPuts("    if ( ctx == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    ok = xlRunScriptString(ctx, text);\n", out);
    xl6OutputPuts("    xl6_xlang_context_destroy(ctx);\n", out);
    xl6OutputPuts("    return ok;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("int xl6_xlang_run_file(xlir_string pathValue)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    const char* path = xlir_string_cstr(pathValue);\n", out);
    xl6OutputPuts("    XL_Context* ctx = path != 0 ? (XL_Context*)xl6_xlang_context_create() : 0;\n", out);
    xl6OutputPuts("    int ok;\n", out);
    xl6OutputPuts("    if ( ctx == 0 || path == 0 || path[0] == '\\0' ) { xl6_xlang_context_destroy(ctx); return 0; }\n", out);
    xl6OutputPuts("    ok = xlRunScriptFile(ctx, path);\n", out);
    xl6OutputPuts("    xl6_xlang_context_destroy(ctx);\n", out);
    xl6OutputPuts("    return ok;\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("#endif\n\n", out);
}
