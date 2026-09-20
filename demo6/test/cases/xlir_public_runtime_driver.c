/* Runtime SDK only, linked with XL_ENABLE_EXE_COMPILER=0. No xlc entrypoints,
 * API implementation includes, symbol replacement or pre-generated program. */
#define XL_RUNTIME_ONLY_API 1
#include "include/xl.h"
#include <stdio.h>
#include <string.h>
#include <windows.h>

static int recovery(XL_Context* runtime)
{
    unsigned round;
    for (round = 0; round < 10; ++round) {
        XL_Module* module;
        if (!xlRunScriptString(runtime, "assert 6 * 7 == 42\n")) return 0;
        if (xlRunScriptString(runtime, "assert missingRuntimeName\n") ||
            !xlGetLastError(runtime) || !*xlGetLastError(runtime)) return 0;
        if (xlRunScriptString(runtime, "assert false\n") || !xlGetLastError(runtime)) return 0;
        module = xlCompileScriptStringToModule(runtime, "assert 21 * 2 == 42\n");
        if (!module) return 0;
        if (!xlRunModuleMain(runtime, module, NULL)) { xlDestroyModule(module); return 0; }
        xlDestroyModule(module);
        if (xlGetLastExitCode(runtime) != 0 ||
            (xlGetLastError(runtime) && *xlGetLastError(runtime))) return 0;
    }
    printf("runtime recovery: 10 rounds, semantic/runtime failure and successful reuse\n");
    return 1;
}

int main(int argc, char** argv)
{
    XL_ContextOptions options = xlDefaultContextOptions();
    XL_Context* runtime;
    char root[2048], dll[2200], libraries[2200], headers[2400];
    char* slash;
    int ok = 0, code = 1;
    if (argc != 3) { fprintf(stderr, "usage: runtime-driver source.xl run|module|recovery\n"); return 2; }
    if (!GetCurrentDirectoryA(sizeof(root), root)) return 3;
    snprintf(dll, sizeof(dll), "%s/release/xl.dll", root);
    snprintf(libraries, sizeof(libraries), "%s/res/tcc/win64/x86_64", root);
    options.workDir = root;
    options.tempDir = "build/test/xlir_public_runtime_temps";
    options.tccDllPath = dll; options.tccLibPath = libraries;
    runtime = xlCreateContext(&options);
    if (!runtime) { fprintf(stderr, "runtime context creation failed\n"); return 4; }
    if (!GetModuleFileNameA(NULL, headers, sizeof(headers))) goto done;
    slash = strrchr(headers, '\\'); if (!slash) goto done;
    strcpy(slash, "\\include");
    if (!xlAddIncludePath(runtime, headers)) goto done;
    if (!strcmp(argv[2], "run")) ok = xlRunScriptFile(runtime, argv[1]);
    else if (!strcmp(argv[2], "module")) {
        XL_Module* module = xlCompileScriptFileToModule(runtime, argv[1]);
        if (!module) goto done;
        ok = xlRunModuleMain(runtime, module, NULL);
        xlDestroyModule(module);
    } else if (!strcmp(argv[2], "recovery")) ok = recovery(runtime);
    code = ok ? xlGetLastExitCode(runtime) : 1;
done:
    if (!ok) fprintf(stderr, "runtime source %s failed: %s\n", argv[2],
        xlGetLastError(runtime) ? xlGetLastError(runtime) : "without diagnostic");
    xlDestroyContext(runtime);
    if (ok && code == 0) printf("runtime source %s complete\n", argv[2]);
    return code;
}
