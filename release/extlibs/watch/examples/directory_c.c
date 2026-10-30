#define XRT_MODULE_DIR
#define XRT_MODULE_DIR_TEMP
#define XRT_MODULE_FILE_TREE
#define XRT_MODULE_PATH
#define XRT_MODULE_HASH64
#include "xrt.h"
#include "../watch_native.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
    char* root = xrtDirTemp(NULL, "xlang-watch-directory-", NULL);
    char* child = root ? xrtPathJoin(root, "same-size.txt") : NULL;
    void* watcher = NULL;
    char* event = NULL;
    xfileinfo before, after;
    int result = 1;
    if (!root || !child || !xrtFileWriteAll(child, (xbytesview){(cbytes)"old!", 4}) ||
        !xrtPathStat(child, true, &before)) goto done;
    /* 和语言范例一样调早初始mtime，不依赖机器的时钟精度或 sleep。 */
    xtime modified = before.Modified - 2000000;
    if (!xrtPathSetTimes(child, true, NULL, &modified)) goto done;
    watcher = xwatch_create();
    if (!watcher || xwatch_add(watcher, root) != 0) goto done;
    event = xwatch_poll(watcher);
    if (!event || *event) goto done;
    xrtFree(event); event = NULL;
    if (!xrtFileWriteAll(child, (xbytesview){(cbytes)"new!", 4}) ||
        !xrtPathStat(child, true, &after) || after.Size != before.Size) goto done;
    puts("same size: true");
    event = xwatch_poll(watcher);
    if (!event || strncmp(event, "MODIFIED:", 9) || strcmp(event + 9, root)) goto done;
    puts("directory modified: true");
    xrtFree(event); event = NULL;
    event = xwatch_poll(watcher);
    if (!event || *event) goto done;
    puts("no repeated event: true");
    result = 0;
done:
    xrtFree(event);
    xwatch_destroy(watcher);
    xrtFree(child);
    if (root && !xrtDirRemoveAll(root)) result = 1;
    xrtFree(root);
    return result;
}

#include "../watch_native.c"
