#define XRT_MODULE_FILE_WHOLE
#define XRT_MODULE_DIR_TEMP
#define XRT_MODULE_MEMORY_DEBUG
#include "../xrt_csv_internal.h"
#include <stdio.h>
#include <string.h>

/* 真实文件 API：目标是带标记文件的目录，Close 的改名自然失败。
 * 此时 Writer 句柄已关闭，再向析构回滚注入一次 OOM，不替换系统调用。
 * 正例要求零残留；当前 Windows XRT 未满足时返回 1 并保留独占现场。 */
int main(int argc, char** argv)
{
    cstr parent = argc > 1 ? argv[1] : "build/test";
    str directory = NULL, target = NULL, marker = NULL, temporary = NULL;
    xrtcsvwriter* writer = NULL;
    xerror* first = NULL;
    xmemdebugsnapshot before, after;
    bytes content = NULL;
    size_t size = 0;
    const str fields[] = {"new"};
    bool cleanup_oom = false, remains = false;
    int result = 2;
    if (!xrtMemDebugEnable(true)) return 2;
    xrtClearError();
    xrtMemDebugSnapshot(&before);
    if (!xrtDirCreateAll(parent)) goto done;
    directory = xrtDirTemp(parent, "csv-closed-", "");
    target = directory ? xrtPathJoin(directory, "target.csv") : NULL;
    marker = target ? xrtPathJoin(target, "original") : NULL;
    if (!marker || !xrtDirCreateAll(target) ||
        !xrtFileWriteAll(marker, (xbytesview){(cbytes)"original", 8})) goto done;
    writer = xrtCsvWriterOpenFile(target, NULL);
    if (!writer || !xrtCsvWriterWrite(writer, 1, fields)) goto done;
    temporary = xrtStrDup(writer->Temporary);
    if (!temporary || xrtCsvWriterClose(writer) || writer->Writer != NULL ||
        xrtErrorKind(xrtGetError()) != XERR_IO) goto done;
    first = xrtErrorRef(xrtGetError());
    if (!xrtMemDebugFailAfter(0)) goto done;
    xrtCsvWriterDestroy(writer);
    writer = NULL;
    cleanup_oom = xrtMemDebugFailTriggered();
    xrtMemDebugFailClear();
    if (xrtGetError() != first) goto done;
    remains = xrtPathExists(temporary);
    content = xrtFileReadAll(marker, &size);
    if (!content || size != 8 || memcmp(content, "original", 8)) goto done;
    printf("csv closed rollback: cleanup-oom=%d temporary=%d original=1\n", cleanup_oom, remains);
    result = remains ? 1 : 0;
done:
    xrtMemDebugFailClear();
    xrtCsvWriterDestroy(writer);
    xrtFree(content);
    if (result == 0) {
        if (!xrtFileDelete(marker) || !xrtDirRemove(target) || !xrtDirRemove(directory)) result = 2;
    }
    if (result != 0 && directory) fprintf(stderr, "CSV closed rollback evidence: %s\n", directory);
    xrtFree(temporary);
    xrtFree(marker);
    xrtFree(target);
    xrtFree(directory);
    xrtErrorFree(first);
    xrtClearError();
    xrtMemDebugSnapshot(&after);
    if (after.LiveCount != before.LiveCount || after.LiveBytes != before.LiveBytes ||
        after.InvalidFreeCount != before.InvalidFreeCount || after.DoubleFreeCount != before.DoubleFreeCount ||
        after.OverflowCount != before.OverflowCount || after.UnderflowCount != before.UnderflowCount) {
        fputs("CSV closed rollback memory balance failed\n", stderr);
        result = 2;
    }
    return result;
}
