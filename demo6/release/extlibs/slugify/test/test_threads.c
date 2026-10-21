#define XRT_MODULE_THREAD
#include "xrt.h"
#include "../include/slugify_native.h"
#include <stdio.h>
#include <string.h>

static int run(void* unused)
{
    int i;
    (void)unused;
    for (i = 0; i < 500; ++i) {
        char* first = (char*)xsl_slugify("Caf\xc3\xa9 & \xe4\xbd\xa0\xe5\xa5\xbd", 14);
        char* second = (char*)xsl_slugify("Next Title", 10);
        int valid = first != NULL && second != NULL &&
            strcmp(first, "cafe-and-\xe4\xbd\xa0\xe5\xa5\xbd") == 0 &&
            strcmp(second, "next-title") == 0;
        xrtFree(first);
        xrtFree(second);
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
    if (valid) puts("slugify threads: PASS");
    return valid ? 0 : 1;
}
