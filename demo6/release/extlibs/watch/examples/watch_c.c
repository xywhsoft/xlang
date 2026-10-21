#define XRT_MODULE_DIR
#define XRT_MODULE_PATH
#define XRT_MODULE_HASH64
#include "xrt.h"
#include "../watch_native.h"
#include <stdio.h>

int main(void) {
    const char *path = "release/extlibs/watch/src/watch.xl";
    void *watcher = xwatch_create();
    char *event;
    if (!watcher) return 1;
    if (xwatch_add(watcher, path) < 0) {
        xwatch_destroy(watcher);
        return 1;
    }
    event = xwatch_poll(watcher);
    if (!event || *event) { xrtFree(event); xwatch_destroy(watcher); return 1; }
    xrtFree(event);
    printf("count: %lld\n", (long long)xwatch_count(watcher));
    printf("exists: %s\n", xwatch_exists(path) ? "true" : "false");
    xwatch_destroy(watcher);
    return 0;
}

/* 示例自包含，可从 demo6 根目录直接 xl run。 */
#include "../watch_native.c"
