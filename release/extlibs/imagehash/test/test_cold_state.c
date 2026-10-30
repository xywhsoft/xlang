/* 每个 kind/budget 由门禁启动新进程，不能用预热后的错误槽代替首次调用。 */
#define XRT_MODULE_ALL
#define XRT_MODULE_MEMORY_DEBUG
#include "xrt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../imagehash_native.c"

#ifdef __TINYC__
#include "../../stb_image/stb_image_native.c"
#include "../../stb_image_resize2/stb_image_resize2_native.c"
#endif

static const char* expected[] = {
    "ffffffffffffffff", "0000000000000000", "0000000000000000"
};
static unsigned failures;

static void check(bool ok, const char* message)
{
    if (!ok) { ++failures; fprintf(stderr, "imagehash cold: %s\n", message); }
}

static char* invoke(unsigned kind, bool* success)
{
    const char* path = "release/examples/extlibs/assets/tiny.png";
    char* result = NULL;
    if (kind == 0) result = xihash_ahash(path);
    else if (kind == 1) result = xihash_dhash(path);
    else if (kind == 2) result = xihash_phash(path);
    else if (kind == 3) result = xihash_phash("");
    else if (kind == 4) {
        *success = xihash_hamming("bad", 3, "0000000000000000", 16) >= 0;
        return NULL;
    } else {
        *success = xihash_valid("0123456789abcdef", 16);
        return NULL;
    }
    *success = result != NULL;
    return result;
}

int main(int argc, char** argv)
{
    unsigned kind = argc > 1 ? (unsigned)atoi(argv[1]) : 0;
    unsigned budget = argc > 2 ? (unsigned)atoi(argv[2]) : 0;
    if (kind > 5) return 2;
    if (!xrtMemDebugEnabled() && !xrtMemDebugEnable(true)) return 2;
    xrtClearError();
    xmemdebugsnapshot before, after;
    xrtMemDebugSnapshot(&before);
    if (!xrtMemDebugFailAfter(budget)) return 2;
    bool success = false;
    char* result = invoke(kind, &success);
    bool injected = xrtMemDebugFailTriggered();
    bool memory = xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_MEMORY;
    xrtMemDebugFailClear();

    // 解码器允许恢复局部分配失败；成功必须值正确、错误为空，而非强制失败。
    if (kind <= 2) {
        if (success) check(result && !strcmp(result, expected[kind]) && !xrtGetError(),
            "successful hash differs or retained an error");
        else check(injected && memory, "failed hash lost the memory cause");
    } else if (kind == 5) {
        check(success && !xrtGetError(), "ordinary valid query failed");
    } else {
        check(!success, "invalid input was accepted");
        check(memory ? injected : !strcmp(xihash_last_error(),
            kind == 3 ? "image path is empty" : "invalid hash string"),
            "invalid input lost its diagnosis");
    }
    xrtFree(result);
    xrtClearError();

    // 取消注入后重复原输入，验证 once 重试、诊断和结果所有权均可恢复。
    result = invoke(kind, &success);
    if (kind <= 2) check(success && result && !strcmp(result, expected[kind]),
        "same input did not recover");
    else if (kind == 5) check(success, "same query did not recover");
    else check(!success && !strcmp(xihash_last_error(),
        kind == 3 ? "image path is empty" : "invalid hash string"),
        "same invalid input changed its diagnosis");
    check(!xrtGetError(), "recovery retained an XRT error");
    xrtFree(result);
    result = xihash_ahash("release/examples/extlibs/assets/tiny.png");
    check(result && !strcmp(result, expected[0]) && !xrtGetError() &&
        !strcmp(xihash_last_error(), ""), "subsequent success retained a failure");
    xrtFree(result);

    // 独占测试进程可释放私有键，以把冷初始化本身也纳入活动内存平衡检查。
    if (xihash_error_key) check(xrtThreadKeyDestroy(xihash_error_key), "thread key cleanup failed");
    xrtClearError();
    xrtMemDebugSnapshot(&after);
    check(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes &&
        before.InvalidFreeCount == after.InvalidFreeCount && before.DoubleFreeCount == after.DoubleFreeCount &&
        before.UseAfterFreeCount == after.UseAfterFreeCount, "cold call or recovery leaked memory");
    if (!failures) printf("imagehash cold: kind=%u budget=%u injected=%u PASS\n",
        kind, budget, (unsigned)injected);
    return failures ? 1 : 0;
}
