/* 固定分配预算使用普通构建，与带额外诊断分配的 sanitizer 边界测试分开。 */
#define XRT_MODULE_ALL
#define XRT_MODULE_MEMORY_DEBUG
#include "xrt.h"
#include "../include/imagehash_native.h"
#include <stdio.h>
#include <string.h>

#ifdef __TINYC__
/* 单文件测试入口；GCC 仍分离编译包实现。 */
#include "../../stb_image/stb_image_native.c"
#include "../../stb_image_resize2/stb_image_resize2_native.c"
#include "../imagehash_native.c"
#endif

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "imagehash budget failed at %d: %s\n", __LINE__, #expr); \
    return 1; \
} } while (0)

int main(void) {
    const char *path = "release/examples/extlibs/assets/tiny.png";
    char *(*functions[])(const char *) = {xihash_ahash, xihash_dhash, xihash_phash};
    const char *expected[] = {"ffffffffffffffff", "0000000000000000", "0000000000000000"};
#ifdef _WIN32
    const unsigned needed = 14u;
#else
    const unsigned needed = 13u;
#endif
    unsigned method;
    for (method = 0u; method < 3u; ++method) {
        char *held = functions[method](path);
        unsigned attempt;
        CHECK(held != NULL && strcmp(held, expected[method]) == 0);
        for (attempt = 0u; attempt < 2u; ++attempt) {
            unsigned budget = attempt == 0u ? 0u : needed;
            char *result;
            bool triggered, memory_error;
            CHECK(xrtMemDebugFailAfter(budget));
            result = functions[method](path);
            triggered = xrtMemDebugFailTriggered();
            memory_error = xrtErrorKind(xrtGetError()) == XERR_MEMORY;
            xrtMemDebugFailClear();
            if (budget == 0u) {
                CHECK(triggered && memory_error && result == NULL);
            } else {
                CHECK(!triggered && !memory_error && result != NULL);
                CHECK(strcmp(result, expected[method]) == 0);
                CHECK(result != held && xrtGetError() == NULL);
                CHECK(strcmp(xihash_last_error(), "") == 0);
            }
            xrtFree(result);
            CHECK(strcmp(held, expected[method]) == 0);
        }
        xrtFree(held);
    }
    puts("imagehash C owned result budgets: PASS");
    return 0;
}
