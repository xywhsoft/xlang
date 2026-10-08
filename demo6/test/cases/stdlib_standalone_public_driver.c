/* Real public API, no synthetic runtime resolver/factory or generated module.
 * Save the exact C and selected XRT source used by the public TCC output, so
 * a separate GCC standalone link can audit defined symbols without linking
 * this compiler host or its complete set of providers into the artifact. */
#include "include/xlc.h"
#include <stdio.h>

int main(int argc, char** argv)
{
    if (argc != 7) return 2;
    XLC_ContextOptions options = xlcDefaultContextOptions();
    options.workDir = argv[1];
    options.tempDir = argv[2];
    options.tccDllPath = argv[3];
    options.saveTemps = 1;
    XLC_Context* context = xlcCreateContext(&options);
    if (!context) return 3;
    int ok = xlcAddIncludePath(context, argv[6]) &&
        xlcAddSourceFile(context, argv[4]) && xlcCompile(context) &&
        xlcEmitExe(context, argv[5]);
    if (!ok) fprintf(stderr, "%s\n", xlcGetLastError(context));
    xlcDestroyContext(context);
    return ok ? 0 : 1;
}
