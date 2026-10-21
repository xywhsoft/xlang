#define XRT_MODULE_HASH64
#define XRT_MODULE_MEMORY_DEBUG
#include "xrt.h"
#include "../include/hyperloglog_native.h"
#include <stdio.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "hyperloglog C test failed at line %d: %s\n", __LINE__, #condition); \
    return 1; \
} } while (0)

int main(void)
{
    void* large = xhll_create(14);
    void* left = xhll_create(12);
    void* right = xhll_create(12);
    void* direct = xhll_create(12);
    unsigned char binary[] = {'a', 0, 'b'};
    char item[32];
    long long count, added;
    int i;
    CHECK(large != NULL && left != NULL && right != NULL && direct != NULL);
    CHECK(xhll_count(large) == 0 && xhll_added(large) == 0);

    /* 超过线性计数区间，才能直接检验寄存器 rank 与估计公式。 */
    for (i = 0; i < 100000; ++i) {
        snprintf(item, sizeof(item), "user:%d", i);
        CHECK(xhll_add(large, item, -1) == 1);
        if (i < 20000) {
            CHECK(xhll_add(i & 1 ? left : right, item, -1) == 1);
            CHECK(xhll_add(direct, item, -1) == 1);
        }
    }
    count = xhll_count(large);
    CHECK(count >= 96000 && count <= 104000);
    CHECK(xhll_added(large) == 100000);
    CHECK(xhll_merge(left, right) == 1);
    count = xhll_count(left);
    CHECK(count >= 18400 && count <= 21600);
    CHECK(count == xhll_count(direct));
    CHECK(xhll_added(left) == 20000);
    CHECK(xhll_merge(left, right) == 1);
    CHECK(xhll_count(left) == count && xhll_added(left) == 30000);
    added = xhll_added(left);
    CHECK(xhll_merge(left, left) == 1 && xhll_added(left) == added);
    CHECK(xhll_merge(large, left) == 0);
    CHECK(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_VALUE);
    CHECK(xhll_merge(left, left) == 1 && xrtGetError() == NULL);

    /* 重叠来源的并集应与逐项写入具有相同的寄存器，而非重复计数。 */
    for (i = 4; i <= 14; i += 10) {
        void* overlap_left = xhll_create(i);
        void* overlap_right = xhll_create(i);
        void* overlap_direct = xhll_create(i);
        int j;
        CHECK(overlap_left != NULL && overlap_right != NULL && overlap_direct != NULL);
        for (j = 0; j < 128; ++j) {
            snprintf(item, sizeof(item), "overlap:%d", j);
            CHECK(xhll_add(overlap_direct, item, -1) == 1);
            if (j < 96) CHECK(xhll_add(overlap_left, item, -1) == 1);
            if (j >= 32) CHECK(xhll_add(overlap_right, item, -1) == 1);
        }
        CHECK(xhll_merge(overlap_left, overlap_right) == 1);
        CHECK(xhll_count(overlap_left) == xhll_count(overlap_direct));
        CHECK(xhll_added(overlap_left) == 192);
        CHECK(xhll_merge(overlap_left, overlap_right) == 1);
        CHECK(xhll_count(overlap_left) == xhll_count(overlap_direct));
        xhll_destroy(overlap_direct);
        xhll_destroy(overlap_right);
        xhll_destroy(overlap_left);
    }

    CHECK(xhll_clear(large) == 1);
    CHECK(xhll_add(large, "a", 1) == 1);
    CHECK(xhll_add(large, (const char*)binary, 3) == 1);
    CHECK(xhll_count(large) >= 2 && xhll_count(large) <= 3);
    CHECK(xhll_added(large) == 2);
    CHECK(xhll_add(large, "a", -2) == 0);
    CHECK(xrtGetError() != NULL);
    CHECK(xhll_add(large, "a", 1) == 1 && xrtGetError() == NULL);
    CHECK(xhll_add(large, NULL, 0) == 0);
    CHECK(xhll_add(NULL, "a", 1) == 0);
    CHECK(xhll_count(NULL) == -1);
    CHECK(xrtGetError() != NULL);
    CHECK(xhll_count(large) >= 2 && xrtGetError() == NULL);
    CHECK(xhll_clear(NULL) == 0);
    CHECK(xrtGetError() != NULL);
    CHECK(xhll_clear(direct) == 1 && xrtGetError() == NULL);
    CHECK(xhll_precision(NULL) == -1);
    CHECK(xrtGetError() != NULL);
    CHECK(xhll_precision(large) == 14 && xrtGetError() == NULL);
    CHECK(xhll_added(NULL) == -1);
    CHECK(xrtGetError() != NULL);
    CHECK(xhll_added(large) == 3 && xrtGetError() == NULL);
    CHECK(xhll_create(3) == NULL && xhll_create(19) == NULL);
    CHECK(xrtGetError() != NULL);
    {
        void* fresh = xhll_create(4);
        CHECK(fresh != NULL && xrtGetError() == NULL);
        xhll_destroy(fresh);
    }

    /* 结构体和寄存器两次分配都失败时，不留下半初始化句柄。 */
    for (i = 0; i < 2; ++i) {
        void* failed;
        xrtClearError();
        CHECK(xrtMemDebugFailAfter((unsigned int)i));
        failed = xhll_create(14);
        CHECK(xrtMemDebugFailTriggered());
        xrtMemDebugFailClear();
        CHECK(failed == NULL && xrtGetError() != NULL &&
            xrtErrorKind(xrtGetError()) == XERR_MEMORY);
    }
    xrtClearError();
    CHECK(xhll_count(large) >= 2 && xhll_count(large) <= 3);

    xhll_destroy(direct);
    xhll_destroy(right);
    xhll_destroy(left);
    xhll_destroy(large);
    puts("hyperloglog C boundaries: PASS");
    return 0;
}
