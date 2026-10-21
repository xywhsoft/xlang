#define XRT_MODULE_THREAD
#include "xrt.h"
#include "../include/lru_native.h"
#include <stdio.h>
#include <string.h>

typedef struct worker {
    void* cache;
    int id;
} worker;

static int run(void* context)
{
    worker* item = (worker*)context;
    char key[32];
    char value[32];
    int i;
    snprintf(key, sizeof(key), "worker-%d", item->id);
    snprintf(value, sizeof(value), "value-%d", item->id);
    for (i = 0; i < 1000; ++i) {
        char* result;
        if (!xlru_put(item->cache, key, value)) return item->id;
        result = (char*)xlru_get(item->cache, key, "miss");
        if (result == NULL || (strcmp(result, value) != 0 &&
            strcmp(result, "miss") != 0)) {
            xrtFree(result);
            return item->id;
        }
        xrtFree(result);
    }
    return 0;
}

int main(void)
{
    void* cache = xlru_create(2, 0);
    xthread* threads[4] = {0};
    worker workers[4];
    int i, ok = cache != NULL;
    if (!ok) return 1;
    for (i = 0; i < 4; ++i) {
        workers[i].cache = cache;
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
    if (xlru_count(cache) > 2) ok = 0;
    xlru_destroy(cache);
    if (ok) puts("lru threads: PASS");
    return ok ? 0 : 1;
}
