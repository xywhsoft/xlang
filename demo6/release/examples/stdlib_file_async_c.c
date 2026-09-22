#define XRT_MODULE_FILE_ASYNC
#define XRT_MODULE_FILE_ASYNC_WHOLE
#define XRT_MODULE_FILE_ASYNC_MANAGE
#define XRT_MODULE_DIR_ASYNC
#define XRT_MODULE_FILE_TREE_ASYNC
#define XRT_MODULE_FILE_TEXT
#include "xrt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>



/* 输出与 xlang 示例一致的失败并终止进程。 */
static void fail(const char* name)
{
    fprintf(stderr, "%s=fail\n", name);
    exit(1);
}



/* 输出与 xlang 示例一致的检查结果。 */
static void require_line(const char* name, int ok)
{
    if ( !ok ) {
        fail(name);
    }
    printf("%s=ok\n", name);
}



/* 执行不需要输出文本的边界检查。 */
static void require_ok(const char* name, int ok)
{
    if ( !ok ) {
        fail(name);
    }
}



/* 等待 Future 成功并在返回前释放消费端引用。 */
static int wait_future_ok(xfuture* future)
{
    int ok;

    if ( future == NULL ) {
        return 0;
    }
    ok = (xrtFutureWait(future) == XWAIT_OK) &&
        (xrtFutureState(future) == XFUTURE_RESOLVED);
    xrtFutureDestroy(future);
    return ok;
}



/* 等待写入类操作并返回其实际字节数。 */
static uint64 wait_file_change(const char* name, xfuture* future)
{
    const xfilechange* change;
    uint64 size;

    if ( (future == NULL) ||
         (xrtFutureWait(future) != XWAIT_OK) ||
         (xrtFutureState(future) != XFUTURE_RESOLVED) ) {
        xrtFutureDestroy(future);
        fail(name);
    }
    change = (const xfilechange*)xrtFutureValue(future);
    if ( change == NULL ) {
        xrtFutureDestroy(future);
        fail(name);
    }
    size = change->Size;
    xrtFutureDestroy(future);
    return size;
}



/* 等待大小查询并返回 64 位结果。 */
static uint64 wait_file_size(const char* name, xfuture* future)
{
    const xfilesize* size;
    uint64 value;

    if ( (future == NULL) ||
         (xrtFutureWait(future) != XWAIT_OK) ||
         (xrtFutureState(future) != XFUTURE_RESOLVED) ) {
        xrtFutureDestroy(future);
        fail(name);
    }
    size = (const xfilesize*)xrtFutureValue(future);
    if ( size == NULL ) {
        xrtFutureDestroy(future);
        fail(name);
    }
    value = size->Size;
    xrtFutureDestroy(future);
    return value;
}



/* 比较 Future 借用的整文件结果和预期字节。 */
static int wait_file_bytes(
    xfuture* future,
    const void* expected,
    size_t expectedSize)
{
    const xfiledata* data;
    int ok;

    if ( (future == NULL) ||
         (xrtFutureWait(future) != XWAIT_OK) ||
         (xrtFutureState(future) != XFUTURE_RESOLVED) ) {
        xrtFutureDestroy(future);
        return 0;
    }
    data = (const xfiledata*)xrtFutureValue(future);
    ok = (data != NULL) &&
        (data->Size == expectedSize) &&
        ((expectedSize == 0) ||
         ((data->Data != NULL) &&
          (memcmp(data->Data, expected, expectedSize) == 0)));
    xrtFutureDestroy(future);
    return ok;
}



/* 比较 UTF-8 整文件结果，不依赖额外零结尾。 */
static int wait_file_text(xfuture* future, const char* expected)
{
    return wait_file_bytes(future, expected, strlen(expected));
}



/* 清理示例创建的目录树，不把不存在视为错误。 */
static void remove_tree_if_exists(const char* path)
{
    if ( xrtPathExists(path) ) {
        xrtDirRemoveAll(path);
    }
}



/* 演示整文件、随机访问文件和目录树的当前异步契约。 */
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
    const char* dirValue =
        "examples_out/example_dir_async/source/nested/value.txt";
    const char* dirCopy = "examples_out/example_dir_async/copy";
    const char* dirMoved = "examples_out/example_dir_async/moved";
    const unsigned char binary[] = {0, 127, 255};
    const unsigned char truncated[] = {0, 127};
    xfileoptions options;
    xtaskpool* pool;
    xasyncfile* asyncFile;

    /* 一个有界任务池服务全部示例操作，并在退出前统一排空。 */
    pool = xrtTaskPoolCreate(NULL);
    require_ok("taskPoolCreate", pool != NULL);

    /* 只清理本示例拥有的路径。 */
    xrtDirCreateAll("examples_out");
    xrtFileDelete(filePath);
    xrtFileDelete(copyPath);
    xrtFileDelete(movePath);
    xrtFileDelete(binaryPath);
    xrtFileDelete(handlePath);
    remove_tree_if_exists(dirRoot);

    /* 整文件 Future 的成功值由 Future 拥有，消费端只借用检查。 */
    require_line(
        "write_read",
        (wait_file_change(
            "write_read",
            xrtFileWriteAllAsync(
                pool,
                filePath,
                XRT_BYTES_LITERAL("hello")
            )
        ) == 5) &&
        wait_file_text(xrtFileReadAllAsync(pool, filePath), "hello")
    );

    require_line(
        "append_copy",
        (wait_file_change(
            "append_copy",
            xrtFileAppendAsync(
                pool,
                filePath,
                XRT_BYTES_LITERAL(" async")
            )
        ) == 6) &&
        wait_future_ok(xrtFileCopyAsync(pool, filePath, copyPath, true)) &&
        wait_file_text(
            xrtFileReadAllAsync(pool, copyPath),
            "hello async"
        )
    );

    require_line(
        "move_delete",
        wait_future_ok(xrtFileMoveAsync(pool, copyPath, movePath, true)) &&
        !xrtFileExists(copyPath) &&
        wait_future_ok(xrtFileDeleteAsync(pool, filePath)) &&
        wait_future_ok(xrtFileDeleteAsync(pool, movePath))
    );

    /* 二进制整文件接口保留零字节和高位字节。 */
    require_ok(
        "binary",
        (wait_file_change(
            "binary_write",
            xrtFileWriteAllAsync(
                pool,
                binaryPath,
                (xbytesview){binary, sizeof(binary)}
            )
        ) == sizeof(binary)) &&
        wait_file_bytes(
            xrtFileReadAllAsync(pool, binaryPath),
            binary,
            sizeof(binary)
        )
    );

    /* 持久对象覆盖偏移 IO、刷新、大小查询、截断和异步关闭。 */
    xrtFileOptionsInit(&options);
    options.Flags = XFILE_READ | XFILE_WRITE | XFILE_CREATE | XFILE_TRUNCATE;
    options.Share = XFILE_SHARE_ALL;
    asyncFile = xrtAsyncFileOpen(pool, handlePath, &options);
    require_ok("asyncFileOpen", asyncFile != NULL);
    require_ok(
        "asyncFileWrite",
        wait_file_change(
            "asyncFileWrite",
            xrtAsyncFileWriteAt(
                asyncFile,
                0,
                (xbytesview){binary, sizeof(binary)}
            )
        ) == sizeof(binary)
    );
    require_ok("asyncFileFlush", wait_future_ok(xrtAsyncFileFlush(asyncFile)));
    require_ok(
        "asyncFileSize",
        wait_file_size("asyncFileSize", xrtAsyncFileSize(asyncFile)) ==
            sizeof(binary)
    );
    require_ok(
        "asyncFileRead",
        wait_file_bytes(
            xrtAsyncFileReadAt(asyncFile, 0, sizeof(binary)),
            binary,
            sizeof(binary)
        )
    );
    require_ok(
        "asyncFileResize",
        wait_file_change(
            "asyncFileResize",
            xrtAsyncFileResize(asyncFile, sizeof(truncated))
        ) == sizeof(truncated)
    );
    require_ok(
        "asyncFileTruncatedSize",
        wait_file_size("asyncFileTruncatedSize", xrtAsyncFileSize(asyncFile)) ==
            sizeof(truncated)
    );
    require_ok("asyncFileClose", wait_future_ok(xrtAsyncFileClose(asyncFile)));
    require_ok(
        "asyncFileTruncatedBytes",
        wait_file_bytes(
            xrtFileReadAllAsync(pool, handlePath),
            truncated,
            sizeof(truncated)
        )
    );

    /* 目录异步接口与整文件接口复用同一任务池和 Future 语义。 */
    require_ok("dirCreate", wait_future_ok(xrtDirCreateAsync(pool, dirRoot)));
    require_ok(
        "dirCreateAll",
        wait_future_ok(xrtDirCreateAllAsync(pool, dirNested))
    );
    require_ok(
        "dirWrite",
        xrtFileWriteText(
            dirValue,
            XRT_STR_LITERAL("dir async"),
            XENCODING_UTF8,
            XUTF_STRICT,
            false
        )
    );
    require_ok(
        "dirCopy",
        wait_future_ok(xrtDirCopyAsync(pool, dirSource, dirCopy, true))
    );
    require_ok(
        "dirCopyText",
        wait_file_text(
            xrtFileReadAllAsync(
                pool,
                "examples_out/example_dir_async/copy/nested/value.txt"
            ),
            "dir async"
        )
    );
    require_ok(
        "dirMove",
        wait_future_ok(xrtDirMoveAsync(pool, dirCopy, dirMoved, true))
    );
    require_ok("dirMoved", !xrtDirExists(dirCopy));
    require_ok(
        "dirDeleteMoved",
        wait_future_ok(xrtDirRemoveAllAsync(pool, dirMoved))
    );
    require_ok(
        "dirDeleteSource",
        wait_future_ok(xrtDirRemoveAllAsync(pool, dirSource))
    );
    require_ok(
        "dirDeleteRoot",
        wait_future_ok(xrtDirRemoveAllAsync(pool, dirRoot))
    );

    xrtFileDelete(binaryPath);
    xrtFileDelete(handlePath);
    require_ok("taskPoolDestroy", xrtTaskPoolDestroy(pool));
    return 0;
}
