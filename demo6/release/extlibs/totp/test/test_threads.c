#define XRT_MODULE_THREAD
#include "xrt.h"
#include "../include/totp_native.h"
#include <stdio.h>
#include <string.h>

static int run(void* unused)
{
    static const char* secret = "GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ";
    int i;
    (void)unused;
    for (i = 0; i < 500; ++i) {
        char* code = (char*)xtotp_generate_at(secret, 32, 59, 30, 8);
        int valid = code != NULL && strcmp(code, "94287082") == 0 &&
            xtotp_verify_at(secret, 32, code, 8, 59, 30, 8, 1) == 1;
        xrtFree(code);
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
    if (valid) puts("totp threads: PASS");
    return valid ? 0 : 1;
}
