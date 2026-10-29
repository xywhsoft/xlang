#define XRT_MODULE_THREAD
#include "xrt.h"
#include "../include/css_native.h"

#include <stdio.h>
#include <string.h>

static int runWorker(void* context)
{
    const char* sheet = "h1 { color: navy; content: \"stable\"; }";
    void* held = xcss_get(sheet, "h1", "content");
    int i;
    (void)context;

    if (held == NULL) return 1;
    for (i = 0; i < 500; ++i) {
        void* current = xcss_get(sheet, "h1", "color");
        bool ok = current != NULL &&
            strcmp((const char*)current, "navy") == 0 &&
            strcmp((const char*)held, "\"stable\"") == 0 &&
            xcss_count_props(sheet, "h1") == 2;
        xrtFree(current);
        if (!ok) {
            xrtFree(held);
            return 1;
        }
    }
    xrtFree(held);
    return 0;
}

int main(void)
{
    xthread* threads[4];
    int i;
    int ok = 1;

    for (i = 0; i < 4; ++i) {
        threads[i] = xrtThreadCreate(runWorker, NULL, 0);
        if (threads[i] == NULL) ok = 0;
    }
    for (i = 0; i < 4; ++i) {
        if (threads[i] == NULL) continue;
        if (xrtThreadWait(threads[i]) != XWAIT_OK ||
            xrtThreadExitCode(threads[i]) != 0) ok = 0;
        xrtThreadDestroy(threads[i]);
    }
    if (ok) puts("css threads: PASS");
    return ok ? 0 : 2;
}
