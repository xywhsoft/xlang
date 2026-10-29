#define XRT_MODULE_THREAD
#include "xrt.h"
#include "../include/consistent_hash_native.h"
#include <stdio.h>
#include <string.h>

typedef struct worker {
    void* ring;
    int id;
} worker;

static int read_ring(void* context)
{
    worker* item = (worker*)context;
    int i;
    for (i = 0; i < 1000; ++i) {
        char* node = (char*)xchash_get_node(item->ring, "shared-key");
        int valid = node != NULL &&
            (!strcmp(node, "node-A") || !strcmp(node, "node-B") ||
             !strcmp(node, "node-C") || !strcmp(node, "temporary"));
        xrtFree(node);
        if (!valid) return item->id;
    }
    return 0;
}

int main(void)
{
    void* ring = xchash_create(64);
    xthread* threads[4] = {0};
    worker workers[4];
    int i, ok = ring != NULL;
    if (!ok) return 1;
    ok = xchash_add_node(ring, "node-A") &&
         xchash_add_node(ring, "node-B") &&
         xchash_add_node(ring, "node-C");
    if (!ok) { xchash_destroy(ring); return 1; }
    for (i = 0; i < 4; ++i) {
        workers[i].ring = ring;
        workers[i].id = i + 1;
        threads[i] = xrtThreadCreate(read_ring, &workers[i], 0);
        if (threads[i] == NULL) ok = 0;
    }
    for (i = 0; i < 100; ++i) {
        if (!xchash_add_node(ring, "temporary") ||
            !xchash_remove_node(ring, "temporary")) ok = 0;
    }
    for (i = 0; i < 4; ++i) {
        if (threads[i] == NULL) continue;
        if (xrtThreadWait(threads[i]) != XWAIT_OK ||
            xrtThreadExitCode(threads[i]) != 0) ok = 0;
        xrtThreadDestroy(threads[i]);
    }
    xchash_destroy(ring);
    if (ok) puts("consistent_hash threads: PASS");
    return ok ? 0 : 1;
}
