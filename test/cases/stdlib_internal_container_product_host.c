/* Observe the real compiler through its public API only. Saved implementation
 * text is the production artifact input, not a test-authored native provider. */
#include "xlc.h"
#include <stdio.h>
#include <string.h>

int main(int argc, char** argv)
{
    if (argc != 6 || (strcmp(argv[3], "run") && strcmp(argv[3], "module") && strcmp(argv[3], "aot")))
        return 2;
    XLC_ContextOptions options = xlcDefaultContextOptions();
    options.workDir = "."; options.tempDir = argv[4]; options.tccDllPath = argv[1]; options.saveTemps = 1;
    XLC_Context* compiler = xlcCreateContext(&options);
    if (!compiler) { fputs("internal-container compiler context creation failed\n", stderr); return 3; }
    int ok = xlcAddSourceFile(compiler, argv[2]) && xlcCompile(compiler), code = 0;
    if (ok && !strcmp(argv[3], "run")) {
        ok = xlcRun(compiler, 0, NULL);
        if (ok) code = xlcGetLastExitCode(compiler);
    } else if (ok && !strcmp(argv[3], "module")) {
        XL_Module* module = xlcRelocateModule(compiler); ok = module != NULL;
        if (ok) {
            ok = xlRunModuleMain(xlcRuntimeContext(compiler), module, NULL);
            if (ok) code = xlGetLastExitCode(xlcRuntimeContext(compiler));
        }
        if (module) xlDestroyModule(module);
    } else if (ok) ok = xlcEmitExe(compiler, argv[5]);
    if (!ok) {
        const char* message = xlcGetLastError(compiler);
        if (!message || !*message) message = xlGetLastError(xlcRuntimeContext(compiler));
        fprintf(stderr, "internal-container %s failed: %s\n", argv[3], message && *message ? message : "without diagnostic");
    }
    xlcDestroyContext(compiler);
    if (ok && !code) printf("internal-container %s complete\n", argv[3]);
    return ok ? code : 1;
}
