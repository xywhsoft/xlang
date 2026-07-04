#include "xrt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void fail(const char* name)
{
    fprintf(stderr, "%s=fail\n", name);
    xrtUnit();
    exit(1);
}

static void require_line(const char* name, int ok)
{
    if ( !ok ) {
        fail(name);
    }
    printf("%s=ok\n", name);
}

static unsigned long long wait_file_io(const char* name, xfuture* future)
{
    xasyncfileio* info;
    unsigned long long value;
    int owned;

    if ( future == NULL || !xFutureWait(future) ) {
        fail(name);
    }
    info = (xasyncfileio*)xFutureValue(future);
    if ( info == NULL ) {
        xFutureRelease(future);
        fail(name);
    }
    value = (unsigned long long)info->iValue;
    owned = xFutureValueIsOwned(future);
    if ( !owned ) {
        xrtAsyncFileIoDestroy(info);
    }
    xFutureRelease(future);
    return value;
}

static int wait_file_text(xfuture* future, const char* expected)
{
    xasyncfilebuf* buf;
    const char* text;
    int ok;
    int owned;

    if ( future == NULL || !xFutureWait(future) ) {
        if ( future != NULL ) { xFutureRelease(future); }
        return 0;
    }
    buf = (xasyncfilebuf*)xFutureValue(future);
    text = (buf != NULL && buf->pData != NULL) ? (const char*)buf->pData : "";
    ok = strcmp(text, expected) == 0;
    owned = xFutureValueIsOwned(future);
    if ( buf != NULL && !owned ) {
        xrtAsyncFileBufDestroy(buf);
    }
    xFutureRelease(future);
    return ok;
}

int main(void)
{
    const char* filePath = "release/tmp/example_file_async.txt";
    const char* copyPath = "release/tmp/example_file_async.copy.txt";
    const char* movePath = "release/tmp/example_file_async.move.txt";

    xrtInit();

    xrtFileDelete((str)filePath);
    xrtFileDelete((str)copyPath);
    xrtFileDelete((str)movePath);

    require_line(
        "write_read",
        wait_file_io("write_read", xrtFileWriteAllAsync((str)filePath, (str)"hello", 0, XRT_CP_UTF8)) > 0 &&
        wait_file_text(xrtFileReadAllAsync((str)filePath, XRT_CP_UTF8), "hello"));

    require_line(
        "append_copy",
        wait_file_io("append_copy", xrtFileAppendAsync((str)filePath, (str)" async", 0, XRT_CP_UTF8)) > 0 &&
        wait_file_io("append_copy", xrtFileCopyAsync((str)filePath, (str)copyPath, true)) > 0 &&
        wait_file_text(xrtFileReadAllAsync((str)copyPath, XRT_CP_UTF8), "hello async"));

    require_line(
        "move_delete",
        wait_file_io("move_delete", xrtFileMoveAsync((str)copyPath, (str)movePath, true)) > 0 &&
        !xrtFileExists((str)copyPath) &&
        wait_file_io("move_delete", xrtFileDeleteAsync((str)filePath)) > 0 &&
        wait_file_io("move_delete", xrtFileDeleteAsync((str)movePath)) > 0);

    xrtUnit();
    return 0;
}
