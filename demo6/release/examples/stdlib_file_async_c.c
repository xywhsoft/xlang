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

static void require_ok(const char* name, int ok)
{
    if ( !ok ) {
        fail(name);
    }
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

static int wait_file_bytes(xfuture* future, const unsigned char* expected, size_t expectedSize)
{
    xasyncfilebuf* buf;
    int ok;
    int owned;

    if ( future == NULL || !xFutureWait(future) ) {
        if ( future != NULL ) { xFutureRelease(future); }
        return 0;
    }
    buf = (xasyncfilebuf*)xFutureValue(future);
    ok = buf != NULL &&
        buf->iSize == expectedSize &&
        (expectedSize == 0u || (buf->pData != NULL && memcmp(buf->pData, expected, expectedSize) == 0));
    owned = xFutureValueIsOwned(future);
    if ( buf != NULL && !owned ) {
        xrtAsyncFileBufDestroy(buf);
    }
    xFutureRelease(future);
    return ok;
}

static int wait_future_ok(xfuture* future)
{
    int ok;

    if ( future == NULL || !xFutureWait(future) ) {
        if ( future != NULL ) { xFutureRelease(future); }
        return 0;
    }
    ok = xFutureStatus(future) >= 0;
    xFutureRelease(future);
    return ok;
}

int main(void)
{
    const char* filePath = "examples_out/example_file_async.txt";
    const char* copyPath = "examples_out/example_file_async.copy.txt";
    const char* movePath = "examples_out/example_file_async.move.txt";
    const char* binaryPath = "examples_out/example_file_async.bin";
    const char* handlePath = "examples_out/example_file_async.handle.bin";
    const char* dirRoot = "examples_out/example_dir_async";
    const char* dirSource = "examples_out/example_dir_async/source";
    const char* dirNested = "examples_out/example_dir_async/source/nested";
    const char* dirValue = "examples_out/example_dir_async/source/nested/value.txt";
    const char* dirCopy = "examples_out/example_dir_async/copy";
    const char* dirMoved = "examples_out/example_dir_async/moved";
    const unsigned char bytes[] = { 0u, 127u, 255u };
    const unsigned char truncated[] = { 0u, 127u };
    xasyncfileconfig config;
    xasyncfile* asyncFile;

    xrtInit();
    xrtDirCreateAll((str)"examples_out");

    xrtFileDelete((str)filePath);
    xrtFileDelete((str)copyPath);
    xrtFileDelete((str)movePath);
    xrtFileDelete((str)binaryPath);
    xrtFileDelete((str)handlePath);
    xrtDirDelete((str)dirRoot);

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

    require_ok(
        "binary",
        wait_file_io("binary_write", xrtFilePutAllAsync((str)binaryPath, bytes, sizeof(bytes))) > 0 &&
        wait_file_bytes(xrtFileGetAllAsync((str)binaryPath), bytes, sizeof(bytes)));

    xrtAsyncFileConfigInit(&config);
    config.sPath = (str)handlePath;
    config.iFlags = XAFILE_F_READ | XAFILE_F_WRITE | XAFILE_F_CREATE | XAFILE_F_TRUNCATE;
    config.iShareFlags = XAFILE_SHARE_READ | XAFILE_SHARE_WRITE | XAFILE_SHARE_DELETE;
    asyncFile = xrtAsyncFileOpen(&config);
    require_ok("asyncFileOpen", asyncFile != NULL);
    require_ok("asyncFileWrite", wait_file_io("asyncFileWrite", xrtAsyncFileWriteAt(asyncFile, 0u, bytes, sizeof(bytes))) == sizeof(bytes));
    require_ok("asyncFileFlush", wait_future_ok(xrtAsyncFileFlush(asyncFile)));
    require_ok("asyncFileSize", wait_file_io("asyncFileSize", xrtAsyncFileGetSize(asyncFile)) == sizeof(bytes));
    require_ok("asyncFileRead", wait_file_bytes(xrtAsyncFileReadAt(asyncFile, 0u, sizeof(bytes)), bytes, sizeof(bytes)));
    require_ok("asyncFileSetSize", wait_file_io("asyncFileSetSize", xrtAsyncFileSetSize(asyncFile, sizeof(truncated))) == sizeof(truncated));
    require_ok("asyncFileTruncatedSize", wait_file_io("asyncFileTruncatedSize", xrtAsyncFileGetSize(asyncFile)) == sizeof(truncated));
    xrtAsyncFileClose(asyncFile);
    require_ok("asyncFileTruncatedBytes", wait_file_bytes(xrtFileGetAllAsync((str)handlePath), truncated, sizeof(truncated)));

    wait_file_io("dirCreate", xrtDirCreateAsync((str)dirRoot));
    wait_file_io("dirCreateAll", xrtDirCreateAllAsync((str)dirNested));
    require_ok("dirWrite", xrtFileWriteAll((str)dirValue, (str)"dir async", 0u, XRT_CP_UTF8) > 0);
    wait_file_io("dirCopy", xrtDirCopyAsync((str)dirSource, (str)dirCopy, true));
    require_ok("dirCopyText", wait_file_text(xrtFileReadAllAsync((str)"examples_out/example_dir_async/copy/nested/value.txt", XRT_CP_UTF8), "dir async"));
    wait_file_io("dirMove", xrtDirMoveAsync((str)dirCopy, (str)dirMoved, true));
    require_ok("dirMoved", !xrtDirExists((str)dirCopy));
    wait_file_io("dirDeleteMoved", xrtDirDeleteAsync((str)dirMoved));
    wait_file_io("dirDeleteSource", xrtDirDeleteAsync((str)dirSource));
    wait_file_io("dirDeleteRoot", xrtDirDeleteAsync((str)dirRoot));

    xrtFileDelete((str)binaryPath);
    xrtFileDelete((str)handlePath);

    xrtUnit();
    return 0;
}
