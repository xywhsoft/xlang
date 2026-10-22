#define XRT_MODULE_DIR
#define XRT_MODULE_DIR_TEMP
#define XRT_MODULE_FILE_TREE
#define XRT_MODULE_PATH
#define XRT_MODULE_HASH64
#include "xrt.h"
#include "../watch_native.h"
#include <stdio.h>
#include <string.h>

static int event_matches(void *event, const char *kind, const char *path) {
    size_t length = strlen(kind);
    int matches = event != NULL && strncmp(event, kind, length) == 0 &&
        ((char *)event)[length] == ':' && strcmp((char *)event + length + 1, path) == 0;
    xrtFree(event);
    return matches;
}

static int event_example(void) {
    char *root = xrtDirTemp(NULL, "xlang-watch-example-", NULL);
    char *target = NULL;
    void *watcher = NULL;
    char *event;
    int result = 1;
    if (!root) return 1;
    target = xrtPathJoin(root, "event.txt");
    watcher = xwatch_create();
    if (!target || !watcher || xwatch_add(watcher, target) != 0) goto done;
    event = xwatch_poll(watcher);
    if (!event || *event) { xrtFree(event); goto done; }
    xrtFree(event);
    if (!xrtFileWriteAll(target, (xbytesview){(const unsigned char *)"a", 1}) ||
        !event_matches(xwatch_poll(watcher), "CREATED", target)) goto done;
    puts("created: true");
    if (!xrtFileWriteAll(target, (xbytesview){(const unsigned char *)"longer", 6}) ||
        !event_matches(xwatch_poll(watcher), "MODIFIED", target)) goto done;
    puts("modified: true");
    if (!xrtFileDelete(target) ||
        !event_matches(xwatch_poll(watcher), "DELETED", target)) goto done;
    puts("deleted: true");
    result = 0;
done:
    xwatch_destroy(watcher);
    if (!xrtDirRemoveAll(root)) result = 1;
    xrtFree(target);
    xrtFree(root);
    return result;
}

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
    return event_example();
fail:
    xwatch_destroy(watcher);
    return 1;
}

/* 示例自包含，可从 demo6 根目录直接 xl run。 */
#include "../watch_native.c"
