/* 与 bloom_basic.xl 同结果；C 显式结束拥有型句柄的生命周期。 */
#include "../../extlibs/bloom/bloom_native.c"
#include <stdio.h>

int main(void)
{
    void *seen = xbloom_create(100, 0.01), *restored = NULL, *independent = NULL;
    void *repeated = NULL;
    int result = 1;
    if (!seen) goto done;
    xbloom_add(seen, "alpha", -1);
    xbloom_add(seen, "beta", -1);
    restored = xbloom_deserialize(xbloom_serialize(seen));
    independent = xbloom_clone(seen);
    repeated = xcounting_bloom_create(100, 0.01);
    if (!restored || !independent || !repeated) goto done;
    xbloom_clear(independent);
    printf("restored: %s\n", xbloom_contains(restored, "alpha", -1) ? "true" : "false");
    printf("independent count: %lld\n", xbloom_count(independent));
    printf("original count: %lld\n", xbloom_count(seen));
    xcounting_bloom_add(repeated, "entry", -1);
    xcounting_bloom_add(repeated, "entry", -1);
    xcounting_bloom_remove(repeated, "entry", -1);
    printf("remaining: %s\n", xcounting_bloom_contains(repeated, "entry", -1) ? "true" : "false");
    result = xrtGetError() ? 1 : 0;
done:
    xbloom_destroy(seen);
    xbloom_destroy(restored);
    xbloom_destroy(independent);
    xcounting_bloom_destroy(repeated);
    xrtTempUnit(xrtTempCurrent());
    return result;
}
