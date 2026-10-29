#define XRT_MODULE_THREAD
#include "xrt.h"
#include "../include/hcl_native.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    int id;
} hclworker;

static int runWorker(void* context)
{
    hclworker* worker = (hclworker*)context;
    const char* source = "name = \"persistent\"\nvalue = 42\n";
    void* held = xhcl_get(source, "name");
    int i;

    if (held == NULL) return 1;
    for (i = 0; i < 500; ++i) {
        void* current = xhcl_get(source, "name");
        bool ok = current != NULL &&
            strcmp((const char*)current, "persistent") == 0 &&
            strcmp((const char*)held, "persistent") == 0 &&
            xhcl_get_int(source, "value") == 42;
        xrtFree(current);
        if (!ok) {
            xrtFree(held);
            return worker->id;
        }
    }
    xrtFree(held);
    return 0;
}

int main(void)
{
    hclworker workers[4];
    xthread* threads[4];
    int i;
    int ok = 1;

    for (i = 0; i < 4; ++i) {
        workers[i].id = i + 1;
        threads[i] = xrtThreadCreate(runWorker, &workers[i], 0);
        if (threads[i] == NULL) ok = 0;
    }
    for (i = 0; i < 4; ++i) {
        if (threads[i] == NULL) continue;
        if (xrtThreadWait(threads[i]) != XWAIT_OK ||
            xrtThreadExitCode(threads[i]) != 0) ok = 0;
        xrtThreadDestroy(threads[i]);
    }
    if (ok) puts("hcl threads: PASS");
    return ok ? 0 : 2;
}
