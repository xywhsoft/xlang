#define XRT_MODULE_THREAD
#define XRT_MODULE_STRING
#include "xrt.h"
#include "../include/fuzzy_native.h"
#include <stdio.h>
#include <string.h>

static int run(void* unused)
{
    int i;
    (void)unused;
    for (i = 0; i < 250; ++i) {
        char* choice = (char*)xfuzzy_best("pc", "src/main.c\nlib/parser.c");
        int valid = choice != NULL && strcmp(choice, "lib/parser.c") == 0 &&
            xfuzzy_levenshtein("你好", "你们") == 1;
        xrtFree(choice);
        if (!valid) return 1;
    }
    return 0;
}

int main(void)
{
    xthread* threads[4] = {0};
    int i, valid = 1;
    for (i = 0; i < 4; ++i) {
        threads[i] = xrtThreadCreate(run, NULL, 0);
        if (threads[i] == NULL) valid = 0;
    }
    for (i = 0; i < 4; ++i) {
        if (threads[i] == NULL) continue;
        if (xrtThreadWait(threads[i]) != XWAIT_OK ||
            xrtThreadExitCode(threads[i]) != 0) valid = 0;
        xrtThreadDestroy(threads[i]);
    }
    if (valid) puts("fuzzy threads: PASS");
    return valid ? 0 : 1;
}
