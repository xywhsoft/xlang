#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <limits.h>
#include <math.h>
#include "../include/bloom.h"
#include "../../../include/xrt_decl.h"

static void expect_error(xerrkind kind)
{
    assert(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == kind);
    xrtClearError();
}

/* 只改变 40 字节头部，声明巨大尺寸不能诱发回绕分配或越界读取。 */
static void corrupt_header(const char *source, void *(*decode)(const char *))
{
    static const unsigned int offsets[] = { 4, 8, 12, 16, 24, 32 };
    char *text = xrtStrDup(source);
    unsigned int i;
    assert(text != NULL);
    for (i = 0; i < sizeof(offsets) / sizeof(offsets[0]); ++i) {
        memcpy(text, source, strlen(source) + 1);
        memset(text + offsets[i] * 2, 'f', offsets[i] < 16 ? 8 : 16);
        assert(decode(text) == NULL);
        expect_error(XERR_VALUE);
    }
    xrtFree(text);
}

/* 固定样本测实际误判率；所有已加入元素必须始终可查到。 */
static int check_false_positive_rate(int items, double fp_rate,
    int max_positives, int counting)
{
    enum { QUERIES = 100000 };
    void *filter = counting ? xcounting_bloom_create(items, fp_rate) :
        xbloom_create(items, fp_rate);
    char key[32];
    int i, false_positives = 0;
    assert(filter != NULL);
    for (i = 0; i < items; ++i) {
        snprintf(key, sizeof(key), "in:%08d", i);
        if (counting) xcounting_bloom_add(filter, key, -1);
        else xbloom_add(filter, key, -1);
    }
    for (i = 0; i < items; ++i) {
        snprintf(key, sizeof(key), "in:%08d", i);
        assert(counting ? xcounting_bloom_contains(filter, key, -1) :
            xbloom_contains(filter, key, -1));
    }
    for (i = 0; i < QUERIES; ++i) {
        snprintf(key, sizeof(key), "out:%08d", i);
        false_positives += (counting ? xcounting_bloom_contains(filter, key, -1) :
            xbloom_contains(filter, key, -1)) != 0;
    }
    if (false_positives > max_positives) {
        fprintf(stderr, "%s false positives: n=%d p=%.4f observed=%d/%d, max=%d\n",
            counting ? "counting bloom" : "bloom",
            items, fp_rate, false_positives, QUERIES, max_positives);
    }
    if (counting) xcounting_bloom_destroy(filter);
    else xbloom_destroy(filter);
    return false_positives <= max_positives;
}

int main(void)
{
    void* filter = xbloom_create(256, 0.01);
    void* restored;
    const char* serialized;
    int rate_ok;


    rate_ok = check_false_positive_rate(1000, 0.01, 1200, 0);
    rate_ok &= check_false_positive_rate(100, 0.01, 1200, 0);
    rate_ok &= check_false_positive_rate(10000, 0.01, 1200, 0);
    rate_ok &= check_false_positive_rate(10000, 0.001, 130, 0);
    rate_ok &= check_false_positive_rate(10000, 0.0001, 25, 0);
    rate_ok &= check_false_positive_rate(10000, 0.001, 130, 1);
    assert(rate_ok);

    if ( filter == NULL ) {
        fputs("test_bloom: create failed\n", stderr);
        return 1;
    }
    xbloom_add(filter, "alpha", -1);
    xbloom_add(filter, "beta", -1);
    serialized = xbloom_serialize(filter);
    {
        char *old_version = xrtStrDup(serialized);
        assert(old_version != NULL);
        memcpy(old_version + 8, "01000000", 8);
        assert(xbloom_deserialize(old_version) == NULL);
        expect_error(XERR_VALUE);
        xrtFree(old_version);
    }
    restored = xbloom_deserialize(serialized);
    if ( restored == NULL || !xbloom_contains(restored, "alpha", -1) ||
         !xbloom_contains(restored, "beta", -1) ||
         xbloom_deserialize("zzzz") != NULL ) {
        fputs("test_bloom: roundtrip or invalid-data guard failed\n", stderr);
        xbloom_destroy(restored);
        xbloom_destroy(filter);
        return 1;
    }
    xbloom_destroy(restored);
    xbloom_destroy(filter);
    expect_error(XERR_VALUE);

    assert(xbloom_create(1, NAN) == NULL); expect_error(XERR_VALUE);
    assert(xbloom_create(1, INFINITY) == NULL); expect_error(XERR_VALUE);
    assert(xbloom_create(1, 0.0) == NULL); expect_error(XERR_VALUE);
    assert(xbloom_create(1, 1.0) == NULL); expect_error(XERR_VALUE);
    assert(xbloom_create(LLONG_MAX, 0.01) == NULL); expect_error(XERR_RANGE);
    assert(xcounting_bloom_create(LLONG_MAX, 0.01) == NULL); expect_error(XERR_RANGE);
    assert(xcounting_bloom_create(0, 0.01) == NULL); expect_error(XERR_VALUE);
    assert(xcounting_bloom_create(1, NAN) == NULL); expect_error(XERR_VALUE);
    assert(xbloom_deserialize(NULL) == NULL); expect_error(XERR_VALUE);
    assert(xcounting_bloom_deserialize("0") == NULL); expect_error(XERR_VALUE);
    assert(xbloom_clone(NULL) == NULL); expect_error(XERR_STATE);
    assert(xcounting_bloom_serialize(NULL)[0] == '\0'); expect_error(XERR_STATE);

    filter = xbloom_create(32, 0.01);
    assert(filter != NULL);
    /* C 的长度参数必须保留嵌入 NUL，不被语言字符串约定截断。 */
    xbloom_add(filter, "a\0b", 3);
    assert(xbloom_contains(filter, "a\0b", 3));
    corrupt_header(xbloom_serialize(filter), xbloom_deserialize);
    restored = xbloom_clone(filter);
    assert(restored != NULL && xbloom_count(restored) == 1);
    xbloom_clear(restored);
    assert(!xbloom_contains(restored, "a\0b", 3));
    assert(xbloom_contains(filter, "a\0b", 3));
    xbloom_destroy(restored);
    xbloom_destroy(filter);

    filter = xcounting_bloom_create(32, 0.01);
    assert(filter != NULL);
    xcounting_bloom_add(filter, "same", -1);
    xcounting_bloom_add(filter, "same", -1);
    xcounting_bloom_remove(filter, "same", -1);
    assert(xcounting_bloom_contains(filter, "same", -1));
    corrupt_header(xcounting_bloom_serialize(filter), xcounting_bloom_deserialize);
    restored = xcounting_bloom_deserialize(xcounting_bloom_serialize(filter));
    assert(restored && xcounting_bloom_count(restored) == 1);
    xcounting_bloom_destroy(restored);
    xcounting_bloom_destroy(filter);
    xrtTempUnit(xrtTempCurrent());
    puts("test_bloom: ok");
    return 0;
}
