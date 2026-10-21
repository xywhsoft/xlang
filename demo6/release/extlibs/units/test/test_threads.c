#define XRT_MODULE_THREAD
#include "xrt.h"
#include "../include/units_native.h"
#include <stdio.h>
#include <string.h>

static int run(void* unused)
{
    int i;
    (void)unused;
    for (i = 0; i < 1000; ++i) {
        char* bytes = (char*)xu_bytes(1536.0);
        char* duration = (char*)xu_duration(3725.0);
        int valid = bytes != NULL && duration != NULL &&
            strcmp(bytes, "1.50 KiB") == 0 &&
            strcmp(duration, "1h 2m 5s") == 0 &&
            xu_parse_bytes("1.50 KiB", 8) == 1536.0;
        xrtFree(bytes);
        xrtFree(duration);
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
    if (valid) puts("units threads: PASS");
    return valid ? 0 : 1;
}
