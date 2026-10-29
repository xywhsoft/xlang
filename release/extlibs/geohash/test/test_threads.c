#define XRT_MODULE_THREAD
#define XRT_MODULE_STRING
#include "xrt.h"
#include "../include/geohash_native.h"
#include <stdio.h>
#include <string.h>

static int run(void* unused)
{
    int i;
    (void)unused;
    for (i = 0; i < 250; ++i) {
        char* hash = (char*)xgeohash_encode(42.6, -5.6, 5);
        char* next = hash != NULL ? (char*)xgeohash_neighbor(hash, 2) : NULL;
        int valid = hash != NULL && next != NULL &&
            strcmp(hash, "ezs42") == 0 && xgeohash_precision(next) == 5;
        xrtFree(next);
        xrtFree(hash);
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
    if (valid) puts("geohash threads: PASS");
    return valid ? 0 : 1;
}
