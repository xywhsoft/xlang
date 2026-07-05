#ifndef XL6_EXAMPLE_BOUNDARY_TEST
#include "include/xlc.h"

#include <stdio.h>

int main(void)
{
    XLC_ContextOptions options = xlcDefaultContextOptions();
    XLC_Context* ctx = xlcCreateContext(&options);
    XL_Module* module;

    if ( ctx == NULL ) {
        fprintf(stderr, "cannot create xlc context\n");
        return 1;
    }
    if ( !xlcAddSourceString(ctx, "__xlc_api_example.xl", "int value = 40 + 2\nassert value == 42, \"xlc api failed\"\nprintln(\"xlc api embed ok\")\n") ||
         !xlcCompile(ctx) ) {
        fprintf(stderr, "%s\n", xlcGetLastError(ctx));
        xlcDestroyContext(ctx);
        return 2;
    }
    module = xlcRelocateModule(ctx);
    if ( module == NULL ) {
        fprintf(stderr, "%s\n", xlcGetLastError(ctx));
        xlcDestroyContext(ctx);
        return 3;
    }
    if ( !xlRunModuleMain(xlcRuntimeContext(ctx), module, NULL) ) {
        fprintf(stderr, "%s\n", xlcGetLastError(ctx));
        xlDestroyModule(module);
        xlcDestroyContext(ctx);
        return 4;
    }
    xlDestroyModule(module);
    xlcDestroyContext(ctx);
    return 0;
}
#else
#include "include/xlc.h"

#include <stdio.h>

static int write_text_file(const char* path, const char* text)
{
    FILE* fp = fopen(path, "wb");
    if ( fp == NULL ) {
        return 0;
    }
    if ( fputs(text, fp) < 0 ) {
        fclose(fp);
        return 0;
    }
    return fclose(fp) == 0;
}

static int fail_xlc(XLC_Context* ctx, const char* step)
{
    fprintf(stderr, "%s failed: %s\n", step, xlcGetLastError(ctx));
    return 1;
}

int main(void)
{
    XLC_ContextOptions options;
    XLC_Context* ctx;
    XL_Module* module;
    const char* source;
    const char* sharedSource;

    source =
        "func add(int a, int b) as int\n"
        "    return a + b\n"
        "end\n"
        "\n"
        "int value = add(20, 22)\n"
        "assert value == 42, \"xlc API run failed\"\n";
    sharedSource =
        "export \"xlc_api_answer\" func answer() as int\n"
        "    return 42\n"
        "end\n";

    options = xlcDefaultContextOptions();
    options.workDir = ".";
    options.tempDir = "release\\tmp\\xlc_api_temp";

    ctx = xlcCreateContext(&options);
    if ( ctx == NULL ) {
        fprintf(stderr, "cannot create xlc context\n");
        return 1;
    }
    if ( !xlcAddSourceString(ctx, "__xlc_api_string.xl", source) ) {
        return fail_xlc(ctx, "xlcAddSourceString");
    }
    if ( !xlcCompile(ctx) ) {
        return fail_xlc(ctx, "xlcCompile");
    }
    if ( !xlcCheck(ctx, "release\\tmp\\xlc_api_check.json", 1) ) {
        return fail_xlc(ctx, "xlcCheck");
    }
    if ( !xlcEmitAst(ctx, "release\\tmp\\xlc_api_ast.json", 1) ) {
        return fail_xlc(ctx, "xlcEmitAst");
    }
    if ( !xlcEmitIr(ctx, "release\\tmp\\xlc_api_ir.txt", 0) ) {
        return fail_xlc(ctx, "xlcEmitIr");
    }
    if ( !xlcEmitC(ctx, "release\\tmp\\xlc_api_string.c") ) {
        return fail_xlc(ctx, "xlcEmitC");
    }
    if ( !xlcEmitHeader(ctx, "release\\tmp\\xlc_api_string.h", "XLC_API_STRING_H") ) {
        return fail_xlc(ctx, "xlcEmitHeader");
    }
    if ( !xlcEmitAbi(ctx, "release\\tmp\\xlc_api_abi.json", 1) ) {
        return fail_xlc(ctx, "xlcEmitAbi");
    }
    if ( !xlcEmitXapi(ctx, "release\\tmp\\xlc_api_xapi.json") ) {
        return fail_xlc(ctx, "xlcEmitXapi");
    }
    module = xlcRelocateModule(ctx);
    if ( module == NULL ) {
        return fail_xlc(ctx, "xlcRelocateModule");
    }
    if ( !xlRunModuleMain(xlcRuntimeContext(ctx), module, NULL) ) {
        xlDestroyModule(module);
        return fail_xlc(ctx, "xlRunModuleMain");
    }
    xlDestroyModule(module);
    if ( !xlcEmitExe(ctx, "release\\tmp\\xlc_api_string.exe") ) {
        return fail_xlc(ctx, "xlcEmitExe");
    }
    if ( !xlcEmitDebugC(ctx, "release\\tmp\\xlc_api_debug.c", "__xlc_api_string.xl") ) {
        return fail_xlc(ctx, "xlcEmitDebugC");
    }
    if ( !xlcEmitDebugExe(ctx, "release\\tmp\\xlc_api_debug.exe", "__xlc_api_string.xl") ) {
        return fail_xlc(ctx, "xlcEmitDebugExe");
    }
    if ( !write_text_file("release\\tmp\\xlc_api_file.xl", source) ) {
        fprintf(stderr, "cannot write xlc API file source\n");
        xlcDestroyContext(ctx);
        return 1;
    }
    if ( !xlcAddSourceFile(ctx, "release\\tmp\\xlc_api_file.xl") ) {
        return fail_xlc(ctx, "xlcAddSourceFile");
    }
    if ( !xlcEmitTokens(ctx, "release\\tmp\\xlc_api_tokens.json", 1) ) {
        return fail_xlc(ctx, "xlcEmitTokens");
    }
    if ( !xlcRun(ctx, 0, NULL) ) {
        return fail_xlc(ctx, "xlcRun");
    }
    if ( !xlcAddSourceString(ctx, "__xlc_api_shared.xl", sharedSource) ) {
        return fail_xlc(ctx, "xlcAddSourceString shared");
    }
    if ( !xlcEmitHeader(ctx, "release\\tmp\\xlc_api_shared.h", "XLC_API_SHARED_H") ) {
        return fail_xlc(ctx, "xlcEmitHeader shared");
    }
    if ( !xlcEmitSharedLibrary(ctx, "release\\tmp\\xlc_api_shared.dll") ) {
        return fail_xlc(ctx, "xlcEmitSharedLibrary");
    }
    xlcDestroyContext(ctx);
    printf("xlc.dll compiler API ok\n");
    return 0;
}
#endif
