#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_VALUE_CONTAINER
#include "roaring_native.h"
#include "xrt.h"

#include <stdio.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "roaring C test failed at %d: %s\n", __LINE__, #expr); \
    return 1; \
} } while (0)

static int check_snapshot(void *set, const int64_t *expected, size_t count) {
    xvalue *values = xroaring_to_array(set);
    size_t i;
    CHECK(values != NULL && xrtValueType(values) == XVALUE_ARRAY);
    CHECK(xrtValueCount(values) == count);
    for (i = 0; i < count; ++i) {
        int64 actual;
        CHECK(xrtValueGetInt(xrtValueArrayGet(values, i), &actual));
        CHECK(actual == expected[i]);
    }
    xrtValueRelease(values);
    return 0;
}

static int test_domain_and_order(void) {
    void *set = xroaring_create();
    static const int64_t initial[] = {0, 65535, 65536, UINT32_MAX};
    static const int64_t remaining[] = {65535, 65536};
    CHECK(set != NULL && xroaring_count(set) == 0);
    CHECK(check_snapshot(set, NULL, 0) == 0);
    CHECK(xroaring_min(set) == -1 && xroaring_max(set) == -1);
    CHECK(xroaring_add(set, 0) && xroaring_add(set, UINT32_MAX));
    CHECK(xroaring_add(set, 65535) && xroaring_add(set, 65536));
    CHECK(xroaring_add(set, 65536) && xroaring_count(set) == 4);
    CHECK(check_snapshot(set, initial, 4) == 0);
    CHECK(xroaring_min(set) == 0 && xroaring_max(set) == UINT32_MAX);
    CHECK(xroaring_contains(set, 65535) && xroaring_contains(set, UINT32_MAX));
    CHECK(!xroaring_remove(set, 123) && xroaring_remove(set, 0));
    CHECK(xroaring_min(set) == 65535 && xroaring_count(set) == 3);
    CHECK(xroaring_remove(set, UINT32_MAX) && xroaring_max(set) == 65536);
    CHECK(!xroaring_add(set, -1) && !xroaring_add(set, (int64_t)UINT32_MAX + 1));
    CHECK(xrtGetError());
    CHECK(!xroaring_contains(set, 123) && xrtGetError() == NULL);
    CHECK(xroaring_count(set) == 2);
    CHECK(check_snapshot(set, remaining, 2) == 0);
    xrtClearError();
    xroaring_clear(set);
    CHECK(xroaring_count(set) == 0 && xroaring_min(set) == -1);
    CHECK(xroaring_add(set, UINT32_MAX) && xroaring_max(set) == UINT32_MAX);
    xroaring_destroy(set);
    return 0;
}

static int test_bitmap_and_failures(void) {
    void *set = xroaring_create();
    xvalue *snapshot;
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
    snapshot = xroaring_to_array(set);
    CHECK(snapshot != NULL && xrtValueCount(snapshot) == 5000);
    for (i = 0; i < 5000; ++i) {
        int64 actual;
        CHECK(xrtValueGetInt(xrtValueArrayGet(snapshot, (size_t)i), &actual));
        CHECK(actual == i);
    }
    for (i = 0; i < 5000; ++i) CHECK(xroaring_contains(set, i));
    for (i = 1; i < 5000; i += 2) CHECK(xroaring_remove(set, i));
    CHECK(xroaring_count(set) == 2500 && xroaring_max(set) == 4998);
    CHECK(xrtValueCount(snapshot) == 5000);
    xrtValueRelease(snapshot);
    snapshot = xroaring_to_array(set);
    CHECK(snapshot != NULL && xrtValueCount(snapshot) == 2500);
    for (i = 0; i < 2500; ++i) {
        int64 actual;
        CHECK(xrtValueGetInt(xrtValueArrayGet(snapshot, (size_t)i), &actual));
        CHECK(actual == i * 2);
    }
    xrtValueRelease(snapshot);
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

static int test_compaction_failure(void) {
    void *set = xroaring_create();
    int i;
    CHECK(set != NULL);
    for (i = 0; i <= 4096; ++i) CHECK(xroaring_add(set, i));

    /* 降级和收缩分配失败时，删除必须保持原集合不变。 */
    CHECK(xrtMemDebugFailAfter(0u));
    CHECK(!xroaring_remove(set, 4096) && xrtMemDebugFailTriggered() &&
        xrtErrorKind(xrtGetError()) == XERR_MEMORY);
    xrtMemDebugFailClear();
    CHECK(xroaring_count(set) == 4097 && xroaring_contains(set, 4096));
    CHECK(xroaring_remove(set, 4096) && xroaring_count(set) == 4096);

    for (i = 4095; i > 1024; --i) CHECK(xroaring_remove(set, i));
    CHECK(xroaring_count(set) == 1025);
    CHECK(xrtMemDebugFailAfter(0u));
    CHECK(!xroaring_remove(set, 1024) && xrtMemDebugFailTriggered() &&
        xrtErrorKind(xrtGetError()) == XERR_MEMORY);
    xrtMemDebugFailClear();
    CHECK(xroaring_count(set) == 1025 && xroaring_contains(set, 1024));
    CHECK(xroaring_remove(set, 1024) && xroaring_count(set) == 1024);
    for (i = 1023; i > 0; --i) CHECK(xroaring_remove(set, i));
    CHECK(xroaring_count(set) == 1 && xroaring_min(set) == 0 &&
        xroaring_max(set) == 0);
    CHECK(xroaring_add(set, 4096) && xroaring_count(set) == 2);
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
    xvalue *values;
    xrtClearError();
    values = xroaring_to_array(NULL);
    CHECK(values != NULL && xrtValueType(values) == XVALUE_NULL &&
        xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
    CHECK(!xroaring_add(NULL, 1) && xrtGetError());
    set = xroaring_create();
    CHECK(set && xrtGetError() == NULL);
    CHECK(!xroaring_contains(NULL, 1) && xrtGetError());
    CHECK(!xroaring_contains(set, 1) && xrtGetError() == NULL);
    CHECK(!xroaring_remove(NULL, 1) && xrtGetError());
    CHECK(!xroaring_remove(set, 1) && xrtGetError() == NULL);
    CHECK(xroaring_count(NULL) == -1 && xrtGetError());
    CHECK(xroaring_count(set) == 0 && xrtGetError() == NULL);
    CHECK(check_snapshot(set, NULL, 0) == 0 && xrtGetError() == NULL);
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

static int test_snapshot_allocation_failure(void) {
    void *set = xroaring_create();
    static const int64_t expected[] = {7, 65536, UINT32_MAX};
    int i, failures = 0, successes = 0;
    CHECK(set != NULL);
    for (i = 0; i < 3; ++i) CHECK(xroaring_add(set, expected[i]));
    for (i = 0; i < 16; ++i) {
        xvalue *values;
        CHECK(xrtMemDebugFailAfter((unsigned)i));
        values = xroaring_to_array(set);
        if (xrtMemDebugFailTriggered()) {
            CHECK(values != NULL && xrtValueType(values) == XVALUE_NULL);
            CHECK(xrtErrorKind(xrtGetError()) == XERR_MEMORY);
            ++failures;
        } else {
            CHECK(values != NULL && xrtValueType(values) == XVALUE_ARRAY);
            CHECK(xrtValueCount(values) == 3);
            ++successes;
        }
        xrtValueRelease(values);
        xrtMemDebugFailClear();
        CHECK(xroaring_count(set) == 3);
        CHECK(check_snapshot(set, expected, 3) == 0);
    }
    CHECK(failures > 0 && successes > 0);
    xroaring_destroy(set);
    return 0;
}

static int test_combine(void) {
    void *left = xroaring_create(), *right = xroaring_create();
    void *result = xroaring_create();
    static const int64_t united[] = {1, 2, 3, 4096, 65536, 131072, UINT32_MAX};
    static const int64_t common[] = {2, 65536};
    static const int64_t remaining[] = {1, 4096, UINT32_MAX};
    int i;
    CHECK(left && right && result);
    CHECK(xroaring_add(left, 1) && xroaring_add(left, 2) &&
        xroaring_add(left, 4096) && xroaring_add(left, 65536) &&
        xroaring_add(left, UINT32_MAX));
    CHECK(xroaring_add(right, 2) && xroaring_add(right, 3) &&
        xroaring_add(right, 65536) && xroaring_add(right, 131072));
    CHECK(xroaring_combine_into(result, left, right, 0));
    CHECK(check_snapshot(result, united, 7) == 0);
    CHECK(xroaring_combine_into(result, left, right, 1));
    CHECK(check_snapshot(result, common, 2) == 0);
    CHECK(xroaring_combine_into(result, left, right, 2));
    CHECK(check_snapshot(result, remaining, 3) == 0);
    CHECK(xroaring_count(left) == 5 && xroaring_count(right) == 4);

    /* 输出与输入别名也必须在完整构造后原子替换。 */
    CHECK(xroaring_combine_into(left, left, right, 0));
    CHECK(check_snapshot(left, united, 7) == 0);
    CHECK(xroaring_combine_into(right, left, right, 1));
    CHECK(check_snapshot(right, (const int64_t[]){2, 3, 65536, 131072}, 4) == 0);
    CHECK(xroaring_combine_into(result, left, left, 2));
    CHECK(check_snapshot(result, NULL, 0) == 0);
    CHECK(xroaring_combine_into(left, left, left, 2));
    CHECK(check_snapshot(left, NULL, 0) == 0);
    CHECK(!xroaring_combine_into(NULL, left, right, 0) &&
        xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
    CHECK(!xroaring_combine_into(result, left, right, 3) &&
        xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);

    xroaring_clear(left);
    xroaring_clear(right);
    for (i = 0; i < 5000; ++i) CHECK(xroaring_add(left, i));
    for (i = 2500; i < 7500; ++i) CHECK(xroaring_add(right, i));
    CHECK(xroaring_combine_into(result, left, right, 0));
    CHECK(xroaring_count(result) == 7500 && xroaring_min(result) == 0 &&
        xroaring_max(result) == 7499);
    for (i = 0; i < 7500; ++i) CHECK(xroaring_contains(result, i));
    CHECK(xroaring_combine_into(result, left, right, 1));
    CHECK(xroaring_count(result) == 2500 && xroaring_min(result) == 2500 &&
        xroaring_max(result) == 4999);
    for (i = 0; i < 7500; ++i)
        CHECK(xroaring_contains(result, i) == (i >= 2500 && i < 5000));
    CHECK(xroaring_combine_into(result, left, right, 2));
    CHECK(xroaring_count(result) == 2500 && xroaring_min(result) == 0 &&
        xroaring_max(result) == 2499);
    for (i = 0; i < 7500; ++i)
        CHECK(xroaring_contains(result, i) == (i < 2500));
    xroaring_destroy(result);
    xroaring_destroy(right);
    xroaring_destroy(left);
    return 0;
}

static int test_combine_differential(void) {
    static unsigned char members[2][3][10000];
    static const uint16_t keys[3] = {0, 1, UINT16_MAX};
    void *left = xroaring_create(), *right = xroaring_create();
    void *result = xroaring_create();
    int key, value, operation;
    CHECK(left && right && result);
    for (key = 0; key < 3; ++key) {
        for (value = 0; value < 10000; ++value) {
            int add_left = (key == 0 && value < 5000) ||
                (key == 1 && value % 11 == 0) ||
                (key == 2 && value < 6000);
            int add_right = (key == 0 && value % 7 == 0) ||
                (key == 1 && value >= 2500 && value < 7500) ||
                (key == 2 && value >= 3000 && value < 9000);
            int64_t number = ((int64_t)keys[key] << 16) | value;
            if (add_left) CHECK(xroaring_add(left, number));
            if (add_right) CHECK(xroaring_add(right, number));
            members[0][key][value] = (unsigned char)add_left;
            members[1][key][value] = (unsigned char)add_right;
        }
    }
    for (operation = 0; operation < 3; ++operation) {
        int64_t expected_count = 0;
        CHECK(xroaring_combine_into(result, left, right, operation));
        for (key = 0; key < 3; ++key) {
            for (value = 0; value < 10000; ++value) {
                int a = members[0][key][value] != 0;
                int b = members[1][key][value] != 0;
                int expected = operation == 0 ? (a || b) :
                    operation == 1 ? (a && b) : (a && !b);
                int64_t number = ((int64_t)keys[key] << 16) | value;
                CHECK(xroaring_contains(result, number) == expected);
                expected_count += expected;
            }
        }
        CHECK(xroaring_count(result) == expected_count);
    }
    xroaring_destroy(result);
    xroaring_destroy(right);
    xroaring_destroy(left);
    return 0;
}

static int test_combine_allocation_failure(void) {
    void *left = xroaring_create(), *right = xroaring_create();
    void *result = xroaring_create();
    static const int64_t old[] = {999};
    int i, failures = 0, successes = 0;
    CHECK(left && right && result);
    CHECK(xroaring_add(left, 1) && xroaring_add(left, 2));
    CHECK(xroaring_add(right, 2) && xroaring_add(right, 3));
    for (i = 0; i < 32; ++i) {
        int combined;
        xroaring_clear(result);
        CHECK(xroaring_add(result, 999));
        CHECK(xrtMemDebugFailAfter((unsigned)i));
        combined = xroaring_combine_into(result, left, right, 0);
        if (!combined) {
            CHECK(xrtMemDebugFailTriggered() &&
                xrtErrorKind(xrtGetError()) == XERR_MEMORY);
            ++failures;
        } else {
            CHECK(!xrtMemDebugFailTriggered() && xroaring_count(result) == 3);
            ++successes;
        }
        xrtMemDebugFailClear();
        if (!combined) CHECK(check_snapshot(result, old, 1) == 0);
        CHECK(xroaring_count(left) == 2 && xroaring_count(right) == 2);
    }
    CHECK(failures > 0 && successes > 0);
    CHECK(xrtMemDebugFailAfter(0u));
    CHECK(!xroaring_combine_into(left, left, right, 0) &&
        xrtMemDebugFailTriggered() &&
        xrtErrorKind(xrtGetError()) == XERR_MEMORY);
    xrtMemDebugFailClear();
    CHECK(check_snapshot(left, (const int64_t[]){1, 2}, 2) == 0);
    xroaring_destroy(result);
    xroaring_destroy(right);
    xroaring_destroy(left);
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
    CHECK(test_compaction_failure() == 0);
    CHECK(test_full_bucket_lifecycle() == 0);
    CHECK(test_error_recovery() == 0);
    CHECK(test_snapshot_allocation_failure() == 0);
    CHECK(test_combine() == 0);
    CHECK(test_combine_differential() == 0);
    CHECK(test_combine_allocation_failure() == 0);
    CHECK(test_differential() == 0);
    return 0;
}
