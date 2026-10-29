/* 通过公开 DLL API 运行扩展库 C 文件用例，不借用编译器内部结构。 */
#define XL_RUNTIME_ONLY_API 1
#include "xl.h"
#include <stdio.h>

int main(int argc, char** argv)
{
    XL_ContextOptions options = xlDefaultContextOptions();
    XL_Context* context;
    char* mainArgs[2];
    int ok;
    int i;
    if (argc < 8) {
        fprintf(stderr, "usage: host workdir xrt-dir xruntime-dir case-dir test-dir source...\n");
        return 2;
    }
    options.workDir = argv[1];
    options.saveTemps = 0;
    context = xlCreateContext(&options);
    if (context == NULL) { return 3; }
    ok = xlAddIncludePath(context, argv[2]) &&
         xlAddIncludePath(context, argv[3]) &&
         xlAddIncludePath(context, argv[4]);
    mainArgs[0] = argv[6];
    mainArgs[1] = argv[5];
    if (ok) {
        const char* sources[64];
        int count = argc - 6;
        if (count > 64) { ok = 0; }
        for (i = 0; ok && i < count; ++i) { sources[i] = argv[i + 6]; }
        if (ok) { ok = xlRunCSourceFiles(context, sources, count, 2, mainArgs); }
    }
    if (!ok || xlGetLastExitCode(context) != 0) {
        fprintf(stderr, "DLL C script failed: %s\n",
            xlGetLastError(context) != NULL ? xlGetLastError(context) : "nonzero exit");
        xlDestroyContext(context);
        return 1;
    }
    xlDestroyContext(context);
    return 0;
}
