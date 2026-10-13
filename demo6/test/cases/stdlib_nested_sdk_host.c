/* Default contexts must use the SDK beside the loaded compiler/executable.
 * No xlAddIncludePath or inherited outer compiler options are used here. */
#include "include/xl.h"
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#define change_directory _chdir
#else
#include <unistd.h>
#define change_directory chdir
#endif

static int run_context(const char* work)
{
    XL_Context* context = xlCreateContext(NULL);
    int ok = 0;
    if (!context) return 0;
    if (!xlSetWorkDir(context, work) ||
        !xlRunScriptString(context, "assert 6 * 7 == 42\n")) goto done;
    if (xlRunScriptString(context, "int invalid = missingSdkHostName\n") ||
        !xlGetLastError(context) || !*xlGetLastError(context)) goto done;
    if (!xlRunScriptString(context,
            "string text = \"a\\0b\"\nassert text.length == 3\n") ||
        xlGetLastExitCode(context) != 0 ||
        (xlGetLastError(context) && *xlGetLastError(context))) goto done;
    ok = 1;
done:
    if (!ok) fprintf(stderr, "default SDK context: %s\n",
        xlGetLastError(context) ? xlGetLastError(context) : "missing error");
    xlDestroyContext(context);
    return ok;
}

int main(int argc, char** argv)
{
    if (argc != 2 || change_directory(argv[1]) != 0) return 2;
    for (unsigned i = 0; i < 3; ++i) if (!run_context(argv[1])) return 1;
    puts("default SDK host: PASS (isolated cwd; 3 create/fail/recover/destroy rounds)");
    return 0;
}
