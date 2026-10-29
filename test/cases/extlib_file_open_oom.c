/* 同一用例验证独立 C、发布 DLL 和 xl run 实际使用的文件实现。 */
#define XRT_MODULE_FILE_WHOLE
#define XRT_MODULE_FILE_TEMP
#define XRT_MODULE_DIR_TEMP
#define XRT_MODULE_MEMORY_DEBUG
#include "xrt.h"
#include <stdio.h>
#include <string.h>

static bool check(bool ok, const char* message)
{
    if (!ok) fprintf(stderr, "file runtime: %s\n", message);
    return ok;
}

static bool preserved(const char* path)
{
    size_t size = 0;
    bytes data = xrtFileReadAll(path, &size);
    bool ok = data != NULL && size == 4 && memcmp(data, "keep", 4) == 0;
    xrtFree(data);
    return ok;
}

/* 失败必须发生在系统截断前；不依赖平台上的具体分配次数。 */
static bool truncate_sweep(const char* path)
{
    unsigned int i, failures = 0, successes = 0;
    bool result = true;
    for (i = 0; i < 64 && successes < 2; ++i) {
        xmemdebugsnapshot before, after;
        xfile file;
        bool hit, ok;
        if (!check(xrtFileWriteAll(path, XRT_BYTES_LITERAL("keep")), "fixture write failed")) return false;
        xrtMemDebugSnapshot(&before);
        if (!check(xrtMemDebugFailAfter(i), "fault injection unavailable")) return false;
        file = xrtOpen(path, XFILE_WRITE | XFILE_CREATE | XFILE_TRUNCATE);
        hit = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        if (hit) {
            ok = file == NULL && xrtErrorKind(xrtGetError()) == XERR_MEMORY;
            result = check(ok, "injected allocation failure lost its error") && result;
            ++failures;
        } else {
            result = check(file != NULL, "open failed without injected failure") && result;
            ++successes;
        }
        if (file != NULL) result = check(xrtClose(file), "file close failed") && result;
        xrtClearError();
        xrtMemDebugSnapshot(&after);
        ok = before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes &&
            before.InvalidFreeCount == after.InvalidFreeCount && before.DoubleFreeCount == after.DoubleFreeCount &&
            before.UnderflowCount == after.UnderflowCount && before.OverflowCount == after.OverflowCount;
        result = check(ok, "file object ownership mismatch") && result;
        if (hit) result = check(preserved(path), "allocation failure truncated the destination") && result;
    }
    result = check(failures != 0 && successes == 2, "incomplete allocation sweep") && result;
    printf("file runtime: failures=%u successes=%u result=%s\n", failures, successes, result ? "pass" : "fail");
    return result;
}

int main(void)
{
    str directory = NULL, path = NULL;
    bool ok = false;
    // JIT 宿主已开启跟踪时，不在仍有编译器对象存活期间重复切换开关。
    if (!check(xrtMemDebugEnabled() || xrtMemDebugEnable(true), "memory debug unavailable")) return 1;
    directory = xrtDirTemp(NULL, "xl-file-oom-", NULL);
    if (!check(directory != NULL, "fixture directory failed")) goto cleanup;
    path = xrtPathJoin(directory, "destination.bin");
    if (!check(path != NULL, "fixture path failed")) goto cleanup;
    ok = truncate_sweep(path);
cleanup:
    // 用例只创建这一份目标；故障路径同样清理，不留下随机临时目录。
    xrtMemDebugFailClear();
    xrtClearError();
    if (path != NULL && xrtPathExists(path)) ok = check(xrtFileDelete(path), "fixture cleanup failed") && ok;
    if (directory != NULL) ok = check(xrtDirRemove(directory), "fixture directory cleanup failed") && ok;
    xrtFree(path);
    xrtFree(directory);
    return ok ? 0 : 1;
}
