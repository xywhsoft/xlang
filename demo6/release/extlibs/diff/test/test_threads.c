#define XRT_MODULE_THREAD
#include "xrt.h"
#include "../include/diff_native.h"
#include <stdio.h>
#include <string.h>

typedef struct worker {
    int id;
} worker;

static int run(void* context)
{
    worker* item = (worker*)context;
    int i;
    for (i = 0; i < 300; ++i) {
        char* lines = (char*)xdiff_lines("a\nb", "a\nB");
        char* stats = (char*)xdiff_stats("a\nb", "a\nB");
        int valid = lines != NULL && stats != NULL &&
            strcmp(lines, " a\n-b\n+B\n") == 0 && strcmp(stats, "1,1,1") == 0;
        xrtFree(lines);
        xrtFree(stats);
        if (!valid) return item->id;
    }
    return 0;
}

int main(void)
{
    xthread* threads[4] = {0};
    worker workers[4];
    int i, ok = 1;
    for (i = 0; i < 4; ++i) {
        workers[i].id = i + 1;
        threads[i] = xrtThreadCreate(run, &workers[i], 0);
        if (threads[i] == NULL) ok = 0;
    }
    for (i = 0; i < 4; ++i) {
        if (threads[i] == NULL) continue;
        if (xrtThreadWait(threads[i]) != XWAIT_OK ||
            xrtThreadExitCode(threads[i]) != 0) ok = 0;
        xrtThreadDestroy(threads[i]);
    }
    if (ok) puts("diff threads: PASS");
    return ok ? 0 : 1;
}
