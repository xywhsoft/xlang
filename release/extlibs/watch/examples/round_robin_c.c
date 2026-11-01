/* 真实文件上的三路径轮转；独占临时目录，默认不接触用户已有文件。 */
#define XRT_MODULE_DIR
#define XRT_MODULE_DIR_TEMP
#define XRT_MODULE_FILE_TREE
#define XRT_MODULE_FILE_WHOLE
#define XRT_MODULE_PATH
#define XRT_MODULE_HASH64
#include "xrt.h"
#include "../watch_native.h"
#include <stdio.h>
#include <string.h>

static bool event_is(void* watcher, cstr kind, cstr path)
{
    char* event = xwatch_poll(watcher);
    size_t size = strlen(kind);
    bool ok = event != NULL && strncmp(event, kind, size) == 0 && event[size] == ':' && strcmp(event + size + 1, path) == 0;
    xrtFree(event); return ok;
}

int main(void)
{
    char* root = xrtDirTemp(NULL, "xlang-watch-round-robin-", NULL);
    char* a = root != NULL ? xrtPathJoin(root, "a.txt") : NULL;
    char* b = root != NULL ? xrtPathJoin(root, "b.txt") : NULL;
    char* c = root != NULL ? xrtPathJoin(root, "c.txt") : NULL;
    void* watcher = NULL;
    char* event = NULL;
    int result = 1;
    if (a == NULL || b == NULL || c == NULL) goto done;
    watcher = xwatch_create();
    if (watcher == NULL || xwatch_add(watcher, a) != 0 || xwatch_add(watcher, b) != 1 || xwatch_add(watcher, c) != 2) goto done;
    cstr borrowed = xwatch_path(watcher, 0);
    if (!xrtFileWriteAll(a, (xbytesview){(cbytes)"1", 1}) ||
        !xrtFileWriteAll(b, (xbytesview){(cbytes)"1", 1}) ||
        !xrtFileWriteAll(c, (xbytesview){(cbytes)"1", 1}) || !event_is(watcher, "CREATED", a)) goto done;
    puts("first: a");
    if (!xrtFileWriteAll(a, (xbytesview){(cbytes)"12", 2}) || !event_is(watcher, "CREATED", b)) goto done;
    puts("busy path: b");
    if (!event_is(watcher, "CREATED", c)) goto done;
    puts("next: c");
    if (!event_is(watcher, "MODIFIED", a)) goto done;
    puts("catch up: a");
    if (!xwatch_remove(watcher, 1) || xwatch_add(watcher, b) != 1 || xwatch_count(watcher) != 3 ||
        xwatch_path(watcher, 0) != borrowed || strcmp(borrowed, a) != 0) goto done;
    puts("slot reused: true");
    if (!xrtFileDelete(b) || !event_is(watcher, "DELETED", b)) goto done;
    puts("deleted: b");
    event = xwatch_poll(watcher);
    if (event == NULL || *event != 0) goto done;
    puts("no repeats: true");
    result = 0;
done:
    if (result != 0) fputs("watch round-robin example failed\n", stderr);
    xrtFree(event); xwatch_destroy(watcher);
    xrtFree(a); xrtFree(b); xrtFree(c);
    if (root != NULL && !xrtDirRemoveAll(root)) result = 1;
    xrtFree(root); return result;
}

#include "../watch_native.c"
