/* 与 shard_union.xl 同一组部分重叠数据，估计值不是精确集合大小保证。 */
#include "../include/hyperloglog_native.h"
#include <inttypes.h>
#include <stdio.h>

#define CHECK(test) do { if (!(test)) goto done; } while (0)

int main(void) {
    void *left = xhll_create(12), *right = xhll_create(12), *direct = xhll_create(12);
    int i, result = 1;
    int64_t estimate, source_estimate;
    char visitor[32];
    CHECK(left != NULL && right != NULL && direct != NULL);
    for (i = 0; i < 2000; ++i) {
        snprintf(visitor, sizeof(visitor), "visitor:%d", i);
        CHECK(xhll_add(direct, visitor, -1));
        if (i < 1500) CHECK(xhll_add(left, visitor, -1));
        if (i >= 500) CHECK(xhll_add(right, visitor, -1));
    }
    source_estimate = xhll_count(right);
    CHECK(xhll_merge(left, right));
    estimate = xhll_count(left);
    CHECK(estimate >= 0 && estimate == xhll_count(direct) && xhll_added(left) == 3000);
    printf("known union: 2000\nestimated union: %" PRId64 "\ninput calls: %" PRId64 "\n",
        estimate, xhll_added(left));
    CHECK(xhll_merge(left, right) && xhll_merge(left, left));
    CHECK(xhll_count(left) == estimate && xhll_added(left) == 4500);
    CHECK(xhll_count(right) == source_estimate && xhll_added(right) == 1500);
    printf("repeat added: %" PRId64 "\nsame estimate: true\nsource intact: true\n", xhll_added(left));
    CHECK(xhll_clear(left) && xhll_count(left) == 0 && xhll_added(left) == 0);
    printf("reset: %" PRId64 " %" PRId64 "\n", xhll_count(left), xhll_added(left));
    CHECK(xhll_add(left, "again", -1) && xhll_count(left) == 1 && xhll_added(left) == 1);
    printf("reused calls: %" PRId64 "\n", xhll_added(left));
    result = 0;
done:
    xhll_destroy(direct);
    xhll_destroy(right);
    xhll_destroy(left);
    return result;
}

#ifdef __TINYC__
#include "../hyperloglog_native.c"
#endif
