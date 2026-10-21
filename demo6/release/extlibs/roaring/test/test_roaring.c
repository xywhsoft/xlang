#define XRT_MODULE_MEMORY_DEBUG
#include "roaring_native.h"
#include "xrt.h"

#include <stdio.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "roaring C test failed at %d: %s\n", __LINE__, #expr); \
    return 1; \
} } while (0)

static int test_domain_and_order(void) {
    void *set = xroaring_create();
    CHECK(set != NULL && xroaring_count(set) == 0);
    CHECK(xroaring_min(set) == -1 && xroaring_max(set) == -1);
    CHECK(xroaring_add(set, 0) && xroaring_add(set, UINT32_MAX));
    CHECK(xroaring_add(set, 65535) && xroaring_add(set, 65536));
    CHECK(xroaring_add(set, 65536) && xroaring_count(set) == 4);
    CHECK(xroaring_min(set) == 0 && xroaring_max(set) == UINT32_MAX);
    CHECK(xroaring_contains(set, 65535) && xroaring_contains(set, UINT32_MAX));
    CHECK(!xroaring_remove(set, 123) && xroaring_remove(set, 0));
    CHECK(xroaring_min(set) == 65535 && xroaring_count(set) == 3);
    CHECK(xroaring_remove(set, UINT32_MAX) && xroaring_max(set) == 65536);
    CHECK(!xroaring_add(set, -1) && !xroaring_add(set, (int64_t)UINT32_MAX + 1));
    CHECK(xrtGetError());
    CHECK(!xroaring_contains(set, 123) && xrtGetError() == NULL);
    CHECK(xroaring_count(set) == 2);
    xrtClearError();
    xroaring_clear(set);
    CHECK(xroaring_count(set) == 0 && xroaring_min(set) == -1);
    CHECK(xroaring_add(set, UINT32_MAX) && xroaring_max(set) == UINT32_MAX);
    xroaring_destroy(set);
    return 0;
}

static int test_bitmap_and_failures(void) {
    void *set = xroaring_create();
    int i;
    CHECK(set != NULL);
    for (i = 4095; i >= 0; --i) CHECK(xroaring_add(set, i));
    CHECK(xroaring_count(set) == 4096);
    CHECK(xrtMemDebugFailAfter(0u));
    CHECK(!xroaring_add(set, 4096));
    CHECK(xrtErrorKind(xrtGetError()) == XERR_MEMORY);
    xrtMemDebugFailClear();
    xrtClearError();
    CHECK(xroaring_count(set) == 4096 && !xroaring_contains(set, 4096));
    for (i = 4096; i < 5000; ++i) CHECK(xroaring_add(set, i));
    CHECK(xroaring_count(set) == 5000);
    for (i = 0; i < 5000; ++i) CHECK(xroaring_contains(set, i));
    for (i = 1; i < 5000; i += 2) CHECK(xroaring_remove(set, i));
    CHECK(xroaring_count(set) == 2500 && xroaring_max(set) == 4998);
    for (i = 0; i < 5000; ++i) CHECK(xroaring_contains(set, i) == (i % 2 == 0));
    CHECK(xrtMemDebugFailAfter(0u));
    CHECK(!xroaring_add(set, 65536));
    CHECK(xrtErrorKind(xrtGetError()) == XERR_MEMORY);
    xrtMemDebugFailClear();
    xrtClearError();
    CHECK(xroaring_count(set) == 2500 && !xroaring_contains(set, 65536));
    CHECK(xroaring_add(set, 65536) && xroaring_count(set) == 2501);
    xroaring_destroy(set);
    return 0;
}

static int test_growth_failure(void) {
    void *set = xroaring_create();
    int i;
    CHECK(set != NULL);
    for (i = 0; i < 2; ++i) {
        CHECK(xrtMemDebugFailAfter((unsigned)i));
        CHECK(!xroaring_add(set, 123));
        CHECK(xrtErrorKind(xrtGetError()) == XERR_MEMORY);
        xrtMemDebugFailClear();
        xrtClearError();
        CHECK(xroaring_count(set) == 0 && !xroaring_contains(set, 123));
    }
    for (i = 0; i < 16; ++i) CHECK(xroaring_add(set, i));
    CHECK(xrtMemDebugFailAfter(0u));
    CHECK(!xroaring_add(set, 16));
    CHECK(xrtErrorKind(xrtGetError()) == XERR_MEMORY);
    xrtMemDebugFailClear();
    xrtClearError();
    CHECK(xroaring_count(set) == 16 && !xroaring_contains(set, 16));
    CHECK(xroaring_add(set, 16) && xroaring_count(set) == 17);
    xroaring_destroy(set);
    return 0;
}

static int test_full_bucket_lifecycle(void) {
    void *set = xroaring_create();
    int i;
    CHECK(set != NULL);
    CHECK(xroaring_add(set, 65536) && xroaring_add(set, UINT32_MAX));
    for (i = 65535; i >= 0; --i) CHECK(xroaring_add(set, i));
    CHECK(xroaring_count(set) == 65538);
    CHECK(xroaring_min(set) == 0 && xroaring_max(set) == UINT32_MAX);
    CHECK(xroaring_add(set, 0) && xroaring_count(set) == 65538);

    /* 位图桶删空后仍须保留相邻桶，并允许在原位置重新建桶。 */
    for (i = 0; i < 65536; ++i) CHECK(xroaring_remove(set, i));
    CHECK(xroaring_count(set) == 2);
    CHECK(xroaring_min(set) == 65536 && xroaring_max(set) == UINT32_MAX);
    CHECK(xroaring_add(set, 42) && xroaring_min(set) == 42);
    CHECK(xroaring_remove(set, 42) && xroaring_remove(set, 65536));
    CHECK(xroaring_remove(set, UINT32_MAX));
    CHECK(xroaring_count(set) == 0 && xroaring_min(set) == -1);
    xroaring_destroy(set);
    return 0;
}

static int test_error_recovery(void) {
    void *set;
    xrtClearError();
    CHECK(!xroaring_add(NULL, 1) && xrtGetError());
    set = xroaring_create();
    CHECK(set && xrtGetError() == NULL);
    CHECK(!xroaring_contains(NULL, 1) && xrtGetError());
    CHECK(!xroaring_contains(set, 1) && xrtGetError() == NULL);
    CHECK(!xroaring_remove(NULL, 1) && xrtGetError());
    CHECK(!xroaring_remove(set, 1) && xrtGetError() == NULL);
    CHECK(xroaring_count(NULL) == -1 && xrtGetError());
    CHECK(xroaring_count(set) == 0 && xrtGetError() == NULL);
    CHECK(xroaring_min(NULL) == -1 && xrtGetError());
    CHECK(xroaring_min(set) == -1 && xrtGetError() == NULL);
    CHECK(xroaring_max(NULL) == -1 && xrtGetError());
    CHECK(xroaring_max(set) == -1 && xrtGetError() == NULL);
    xroaring_clear(NULL);
    CHECK(xrtGetError());
    xroaring_clear(set);
    CHECK(xroaring_count(set) == 0 && xrtGetError() == NULL);
    xroaring_destroy(set);
    return 0;
}

static uint32_t next_random(uint32_t *state) {
    *state ^= *state << 13;
    *state ^= *state >> 17;
    *state ^= *state << 5;
    return *state;
}

static int test_differential(void) {
    static unsigned char expected[3][65536];
    static const uint16_t keys[3] = { 0, 256, UINT16_MAX };
    void *set = xroaring_create();
    uint32_t random = UINT32_C(0x4b1d2a37);
    int64_t count = 0;
    int step, bucket, low;
    CHECK(set != NULL);

    /* 先跨过数组/位图阈值，再交错修改三种高位桶。 */
    for (low = 0; low < 5000; ++low) {
        CHECK(xroaring_add(set, low));
        expected[0][low] = 1;
        ++count;
    }
    for (step = 0; step < 30000; ++step) {
        uint32_t bits = next_random(&random);
        int slot = (bits & 3u) == 0 ? (int)((bits >> 2) % 3u) : 0;
        uint16_t value = (uint16_t)(bits >> 8);
        int64_t number = ((int64_t)keys[slot] << 16) | value;
        int present = expected[slot][value] != 0;

        if (step % 3 == 0) {
            CHECK(xroaring_add(set, number));
            if (!present) { expected[slot][value] = 1; ++count; }
        } else if (step % 3 == 1) {
            CHECK(xroaring_remove(set, number) == present);
            if (present) { expected[slot][value] = 0; --count; }
        } else {
            CHECK(xroaring_contains(set, number) == present);
        }

        if (step % 503 == 0) {
            int64_t minimum = -1, maximum = -1;
            CHECK(xroaring_count(set) == count);
            for (bucket = 0; bucket < 3; ++bucket) {
                for (low = 0; low < 65536; ++low) {
                    if (expected[bucket][low]) {
                        int64_t item = ((int64_t)keys[bucket] << 16) | low;
                        if (minimum < 0) minimum = item;
                        maximum = item;
                    }
                }
            }
            CHECK(xroaring_min(set) == minimum);
            CHECK(xroaring_max(set) == maximum);
        }
    }
    CHECK(xroaring_count(set) == count);
    xroaring_destroy(set);
    return 0;
}

int main(void) {
    CHECK(test_domain_and_order() == 0);
    CHECK(test_bitmap_and_failures() == 0);
    CHECK(test_growth_failure() == 0);
    CHECK(test_full_bucket_lifecycle() == 0);
    CHECK(test_error_recovery() == 0);
    CHECK(test_differential() == 0);
    return 0;
}
