/* Compile the consumer through the real runtime/compiler DLL and its VFS.
 * The system compiler builds only this public API host, not the consumer. */
#include "xl.h"
#include <stdio.h>

int main(int argc, char** argv)
{
    if (argc != 5) return 2;
    XL_ContextOptions options = xlDefaultContextOptions();
    options.workDir = "."; options.tempDir = argv[4]; options.tccDllPath = argv[1];
    XL_Context* context = xlCreateContext(&options);
    if (!context) return 3;
    int ok = xlRunCSourceFile(context, argv[2], 0, NULL) && !xlGetLastExitCode(context);
    XL_Module* module = ok ? xlCompileCSourceFileToModule(context, argv[2]) : NULL;
    if (ok && !module) ok = 0;
    if (module) {
        long long (*probe)(void) = (long long (*)(void))xlModuleGetSymbol(module, "xl6StdlibWinNlsProbe");
        ok = probe && probe() == 0;
        xlDestroyModule(module);
    }
#ifdef XL6_NLS_COMPILER_HOST
    if (ok) ok = xlCompileCSourceFileToExe(context, argv[2], argv[3]);
#endif
    if (!ok) {
        const char* message = xlGetLastError(context);
        fprintf(stderr, "Windows NLS SDK host: %s\n",
            message && *message ? message : "consumer returned failure");
    }
    xlDestroyContext(context);
    if (ok) puts("Windows NLS SDK host: PASS (actual product JIT/module)");
    return ok ? 0 : 1;
}
