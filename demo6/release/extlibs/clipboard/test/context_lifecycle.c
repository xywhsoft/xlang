#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include "xl.h"

static int available_fls_slots(void)
{
    DWORD slots[8192];
    DWORD slot;
    int count = 0;
    int i;
    while (count < (int)(sizeof(slots) / sizeof(slots[0]))) {
        slot = FlsAlloc(NULL);
        if (slot == FLS_OUT_OF_INDEXES) break;
        slots[count++] = slot;
    }
    for (i = 0; i < count; ++i) FlsFree(slots[i]);
    return count;
}

static int run_script(const XL_ContextOptions *options, const char *path)
{
    XL_Context *context = xlCreateContext(options);
    int ok;
    if (context == NULL) {
        fprintf(stderr, "clipboard context: create failed\n");
        return 0;
    }
    ok = xlRunScriptFile(context, path) && xlGetLastExitCode(context) == 0;
    if (!ok) fprintf(stderr, "clipboard context: %s\n", xlGetLastError(context));
    xlDestroyContext(context);
    return ok;
}

int main(int argc, char **argv)
{
    XL_ContextOptions options;
    int initial, plain, imported;
    int i;
    if (argc != 4) return 2;
    options = xlDefaultContextOptions();
    options.workDir = argv[1];
    if (!run_script(&options, argv[2]) || !run_script(&options, argv[3])) return 1;
    initial = available_fls_slots();
    for (i = 0; i < 30; ++i) if (!run_script(&options, argv[2])) return 1;
    plain = available_fls_slots();
    for (i = 0; i < 30; ++i) if (!run_script(&options, argv[3])) return 1;
    imported = available_fls_slots();
    printf("clipboard FLS slots: warm=%d plain=%d imported=%d\n",
           initial, plain, imported);
    if (initial != plain || plain != imported) return 1;
    puts("clipboard repeated XL_Context lifecycle: PASS");
    return 0;
}
