#define XRT_MODULE_DIR
#define XRT_MODULE_PATH
#define XRT_MODULE_HASH64
#include "xrt.h"
#include "../watch_native.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    const char *path = "release/extlibs/watch/src/watch.xl";
    void *watcher = xwatch_create();
    char *event;
    int64_t index, reused;
    if (!watcher) return 1;
    index = xwatch_add(watcher, path);
    if (index < 0) {
        xwatch_destroy(watcher);
        return 1;
    }
    event = xwatch_poll(watcher);
    if (!event || *event) { xrtFree(event); xwatch_destroy(watcher); return 1; }
    xrtFree(event);
    if (xwatch_count(watcher) != 1 || !xwatch_exists(path)) goto fail;
    printf("count: %lld\n", (long long)xwatch_count(watcher));
    printf("exists: %s\n", xwatch_exists(path) ? "true" : "false");
    if (xwatch_path(watcher, index) == NULL ||
        strcmp(xwatch_path(watcher, index), path) != 0) goto fail;
    puts("path matches: true");
    if (!xwatch_remove(watcher, index) || xwatch_count(watcher) != 0) goto fail;
    puts("after remove: 0");
    reused = xwatch_add(watcher, path);
    if (reused != index || xwatch_count(watcher) != 1) goto fail;
    puts("slot reused: true");
    xwatch_destroy(watcher);
    return 0;
fail:
    xwatch_destroy(watcher);
    return 1;
}

/* 示例自包含，可从 demo6 根目录直接 xl run。 */
#include "../watch_native.c"
