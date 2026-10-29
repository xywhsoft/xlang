/* Compile once through the public compiler API, inspect the verified output,
 * then execute the same unit with JIT and an explicit module. No source-object
 * overrides, private callbacks or library ABI re-declarations. */
#include "xlc.h"
#include <stdio.h>

static int output_path(char* path, size_t capacity, const char* directory,
    const char* name)
{
    int size = snprintf(path, capacity, "%s/%s", directory, name);
    return size >= 0 && (size_t)size < capacity;
}

static int phase(int ok, XLC_Context* compiler, const char* name)
{
    if (!ok) {
        const char* error = xlcGetLastError(compiler);
        fprintf(stderr, "%s failed: %s\n", name, error ? error : "no diagnostic");
        return 0;
    }
    printf("joint-host: %s\n", name);
    fflush(stdout);
    return 1;
}

int main(int argc, char** argv)
{
    if (argc != 3) return 2;
    char path[8192], temporary[8192];
    if (!output_path(temporary, sizeof(temporary), argv[2], "compiler-temps")) return 2;
    XLC_ContextOptions options = xlcDefaultContextOptions();
    options.tempDir = temporary;
    XLC_Context* compiler = xlcCreateContext(&options);
    if (!compiler) return 3;
    int ok = phase(xlcAddSourceFile(compiler, argv[1]) && xlcCompile(compiler), compiler, "compile");
    if (ok) ok = output_path(path, sizeof(path), argv[2], "joint.typed.json") &&
        phase(xlcEmitIr(compiler, path, 1), compiler, "typed");
    if (ok) ok = output_path(path, sizeof(path), argv[2], "joint.lowered.json") &&
        phase(xlcEmitLoweredIr(compiler, path, 1), compiler, "lowered");
    if (ok) ok = output_path(path, sizeof(path), argv[2], "joint.c") &&
        phase(xlcEmitC(compiler, path), compiler, "cgen");
    if (ok) ok = output_path(path, sizeof(path), argv[2], "joint.exe") &&
        phase(xlcSetSaveTemps(compiler, 1) && xlcEmitExe(compiler, path), compiler, "aot-build");
    if (ok) ok = phase(xlcRun(compiler, 0, NULL) && !xlcGetLastExitCode(compiler), compiler, "jit");
    if (ok) {
        XL_Module* module = xlcRelocateModule(compiler);
        ok = phase(module && xlRunModuleMain(xlcRuntimeContext(compiler), module, NULL) &&
            !xlGetLastExitCode(xlcRuntimeContext(compiler)), compiler, "module");
        xlDestroyModule(module);
    }
    xlcDestroyContext(compiler);
    XL_RuntimeDrainResult drain = xlRuntimeDrainRetiringChecked(30000);
    if (!drain.ready || !drain.ok) {
        fputs("joint-host checked retirement failed\n", stderr);
        ok = 0;
    }
    if (ok) puts("joint-host: checked-retirement");
    return ok ? 0 : 1;
}
