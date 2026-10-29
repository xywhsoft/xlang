#ifndef XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_MEMORY_DEBUG
#endif
#define XRT_MODULE_DIR
#define XRT_MODULE_STRING_FORMAT
#include "../include/xrt_tar.h"
#include <stdio.h>
#include <string.h>

/* 红色边界探针：文件已关闭、提交改名失败后，再让回滚分配失败。 */
int main(int argc, char** argv)
{
    xrttarwriter* writer = NULL;
    xrttarentry entry;
    xdir dir = NULL;
    xdirentry item;
    xdirnext next = XDIR_NEXT_ERROR;
    str target = NULL;
    unsigned temporary = 0;
    bool allocated, original;
    int result = 2;
    if (argc != 2 || !xrtMemDebugEnabled()) return 2;
    target = xrtFormat("%s/commit-target", argv[1]);
    if (target == NULL || !xrtDirCreate(target)) goto done;
    writer = xrtTarWriterOpenFile(target, NULL);
    xrtTarEntryInit(&entry);
    entry.sName = "data.txt";
    if (writer == NULL || !xrtTarWriterAdd(writer, &entry, "data", 4u) ||
        xrtTarWriterClose(writer) || xrtTarWriterError(writer)->iCode != XRT_TAR_ERROR_IO) goto done;
    xrtClearError();
    if (!xrtMemDebugFailAfter(0)) goto done;
    xrtTarWriterDestroy(writer);
    writer = NULL;
    allocated = xrtMemDebugFailTriggered();
    xrtMemDebugFailClear();
    xrtClearError();
    original = xrtDirExists(target);
    dir = xrtDirOpen(argv[1], 0);
    if (dir == NULL) goto done;
    while ((next = xrtDirNext(dir, &item)) == XDIR_NEXT_ITEM) {
        if (strncmp(item.Name.Data, ".xrt-tar-", 9u) == 0) ++temporary;
    }
    if (next != XDIR_NEXT_END) goto done;
    printf("closed TAR rollback: allocated=%d temporary=%u original=%d\n",
        (int)allocated, temporary, (int)original);
    result = !allocated && temporary == 0 && original ? 0 : 1;
done:
    xrtMemDebugFailClear();
    (void)xrtDirClose(dir);
    xrtTarWriterDestroy(writer);
    xrtFree(target);
    return result;
}
