#define XRT_MODULE_THREAD
#include "xrt.h"
#include "../include/hyperloglog_native.h"
#include <stdio.h>

static int run(void* unused)
{
    void* state = xhll_create(10);
    char item[32];
    int i, valid = state != NULL;
    (void)unused;
    if (!valid) return 1;
    for (i = 0; i < 2000; ++i) {
        snprintf(item, sizeof(item), "item:%d", i);
        if (!xhll_add(state, item, -1)) { valid = 0; break; }
    }
    if (xhll_count(state) < 1600 || xhll_count(state) > 2400) valid = 0;
    xhll_destroy(state);
    return valid ? 0 : 1;
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
    if (valid) puts("hyperloglog threads: PASS");
    return valid ? 0 : 1;
}
