#define XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_MEMORY_DEBUG
#include "xrt.h"
#include "money_native.h"

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "money C test failed at %d: %s\n", __LINE__, #expr); \
    return 1; \
} } while (0)

static int test_arithmetic(void) {
    int64_t result = 73;
    xvalue *boxed;
    CHECK(xm_add_checked(INT64_MAX, 0, &result) && result == INT64_MAX);
    CHECK(!xm_add_checked(INT64_MAX, 1, &result) && result == INT64_MAX);
    CHECK(!xm_add_checked(INT64_MIN, -1, &result) && result == INT64_MAX);
    CHECK(xm_sub_checked(INT64_MIN, 0, &result) && result == INT64_MIN);
    CHECK(!xm_sub_checked(INT64_MIN, 1, &result) && result == INT64_MIN);
    CHECK(!xm_sub_checked(INT64_MAX, -1, &result) && result == INT64_MIN);
    CHECK(xm_mul_checked(INT64_MIN, 1, &result) && result == INT64_MIN);
    CHECK(!xm_mul_checked(INT64_MIN, -1, &result) && result == INT64_MIN);
    CHECK(xm_mul_checked(-3, 4, &result) && result == -12);
    CHECK(xm_mul_checked(0, INT64_MAX, &result) && result == 0);
    CHECK(xm_scale_checked(1, 1, 2, &result) && result == 1);
    CHECK(xm_scale_checked(-1, 1, 2, &result) && result == -1);
    CHECK(xm_scale_checked(100, -1, 8, &result) && result == -13);
    CHECK(xm_scale_checked(INT64_MAX, INT64_MAX, INT64_MAX, &result) && result == INT64_MAX);
    CHECK(xm_scale_checked(INT64_MIN, 1, 1, &result) && result == INT64_MIN);
    CHECK(!xm_scale_checked(INT64_MIN, 1, -1, &result) && result == INT64_MIN);
    CHECK(!xm_scale_checked(1, 1, 0, &result) && result == INT64_MIN);
    CHECK(!xm_scale_checked(INT64_MAX, 2, 1, &result) && result == INT64_MIN);
    CHECK(xm_rate_checked(10099, 6.90, &result) && result == 69683);
    CHECK(!xm_rate_checked(1, NAN, &result) && result == 69683);
    CHECK(!xm_rate_checked(1, INFINITY, &result) && result == 69683);
    CHECK(!xm_rate_checked(INT64_MAX, 2.0, &result) && result == 69683);
    CHECK(xm_rate_checked(1, 0.5, &result) && result == 1);
    CHECK(xm_rate_checked(-1, 0.5, &result) && result == -1);
    CHECK(xm_rate_checked(INT64_MAX, 1.0, &result) && result == INT64_MAX);
    CHECK(xm_rate_checked(INT64_MIN, 1.0, &result) && result == INT64_MIN);
    CHECK(xm_rate_checked(INT64_MAX, 0.0, &result) && result == 0);
    boxed = xm_add(INT64_MAX, 1);
    CHECK(boxed != NULL && xrtValueType(boxed) == XVALUE_NULL);
    xrtValueRelease(boxed);
    return 0;
}

#if defined(__SIZEOF_INT128__) && !defined(__TINYC__)
static int test_scale_case(int64_t amount, int64_t numerator, int64_t denominator) {
    __int128 product = (__int128)amount * numerator;
    __int128 magnitude = product < 0 ? -product : product;
    __int128 divisor = denominator < 0 ? -(__int128)denominator : denominator;
    __int128 quotient = magnitude / divisor;
    __int128 remainder = magnitude % divisor;
    __int128 limit = ((product < 0) != (denominator < 0)) ?
        (__int128)INT64_MAX + 1 : INT64_MAX;
    int64_t result = 71;
    int ok;
    if (remainder * 2 >= divisor) ++quotient;
    ok = xm_scale_checked(amount, numerator, denominator, &result);
    if (quotient > limit) CHECK(!ok && result == 71);
    else {
        __int128 signed_result = ((product < 0) != (denominator < 0)) ? -quotient : quotient;
        CHECK(ok && result == (int64_t)signed_result);
    }
    return 0;
}

static int test_scale_reference(void) {
    uint64_t seed = UINT64_C(0x9e3779b97f4a7c15);
    static const int64_t edges[] = {
        INT64_MIN, INT64_MIN + 1, -3, -1, 0, 1, 2, 3, INT64_MAX - 1, INT64_MAX
    };
    size_t a, b, d;
    int i;
    for (a = 0; a < sizeof(edges) / sizeof(edges[0]); ++a)
        for (b = 0; b < sizeof(edges) / sizeof(edges[0]); ++b)
            for (d = 0; d < sizeof(edges) / sizeof(edges[0]); ++d)
                if (edges[d] != 0 && test_scale_case(edges[a], edges[b], edges[d])) return 1;
    for (i = 0; i < 10000; ++i) {
        int64_t amount, numerator, denominator;
        seed = seed * UINT64_C(6364136223846793005) + 1;
        amount = (int64_t)(seed >> 1);
        if (seed & 4) amount = -amount;
        seed = seed * UINT64_C(6364136223846793005) + 1;
        numerator = (int64_t)(seed >> 1);
        if (seed & 8) numerator = -numerator;
        seed = seed * UINT64_C(6364136223846793005) + 1;
        denominator = (int64_t)(seed >> 1) + 1;
        if (seed & 2) denominator = -denominator;
        if (test_scale_case(amount, numerator, denominator)) return 1;
    }
    return 0;
}
#else
static int test_scale_reference(void) { return 0; }
#endif

static int test_format_and_split(void) {
    char *first = xm_format(123456, "usd", 3);
    char *second = xm_format(INT64_MIN, "USD", 3);
    char *unknown = xm_format(123, "xyz", 3);
    char *rubles = xm_format(123, "RUB", 3);
    xvalue *parts;
    int64_t amount, sum = 0;
    int i;
    CHECK(first && second && unknown && rubles);
    CHECK(strcmp((const char *)first, "$1,234.56") == 0);
    CHECK(strcmp((const char *)second, "$-92,233,720,368,547,758.08") == 0);
    CHECK(strcmp((const char *)unknown, "XYZ 1.23") == 0);
    CHECK(strcmp(xm_symbol("rub", 3), "\xE2\x82\xBD") == 0);
    CHECK(strcmp((const char *)rubles, "\xE2\x82\xBD" "1.23") == 0);
    CHECK(first != second && first != unknown);
    xrtFree(first);
    xrtFree(second);
    xrtFree(unknown);
    xrtFree(rubles);
    CHECK(strcmp(xm_symbol("jpy", 3), "\xC2\xA5") == 0);
    CHECK(xm_exponent("jpy", 3) == 0);
    CHECK(xm_format(1, "LONG", 4) == NULL);
    CHECK(xm_format(1, "U\0D", 3) == NULL);
    parts = xm_split(-101, 3);
    CHECK(parts != NULL && xrtValueCount(parts) == 3);
    for (i = 0; i < 3; ++i) {
        CHECK(xrtValueGetInt(xrtValueArrayAt(parts, i), &amount));
        CHECK(amount == (i < 2 ? -34 : -33));
        sum += amount;
    }
    CHECK(sum == -101);
    xrtValueRelease(parts);
    parts = xm_split(INT64_MIN, 3);
    CHECK(parts != NULL && xrtValueCount(parts) == 3);
    sum = 0;
    for (i = 0; i < 3; ++i) {
        CHECK(xrtValueGetInt(xrtValueArrayAt(parts, i), &amount));
        sum += amount;
    }
    CHECK(sum == INT64_MIN);
    xrtValueRelease(parts);
    parts = xm_split(100, 0);
    CHECK(parts != NULL && xrtValueType(parts) == XVALUE_NULL);
    xrtValueRelease(parts);
    return 0;
}

static int test_split_reference(void) {
    static const int64_t edges[] = {
        INT64_MIN, INT64_MIN + 1, -101, -1, 0, 1, 101, INT64_MAX - 1, INT64_MAX
    };
    uint64_t seed = UINT64_C(0x6a09e667f3bcc909);
    size_t sample;
    int count;

    /* 小份数覆盖全部余数位置；极值与固定随机金额共用独立的整数参照。 */
    for (sample = 0; sample < sizeof(edges) / sizeof(edges[0]) + 128; ++sample) {
        int64_t original;
        if (sample < sizeof(edges) / sizeof(edges[0])) original = edges[sample];
        else {
            seed = seed * UINT64_C(6364136223846793005) + 1;
            original = (int64_t)(seed >> 1);
            if (seed & 4) original = -original;
        }
        for (count = 1; count <= 16; ++count) {
            xvalue *parts = xm_split(original, count);
            int64_t quotient = original / count;
            int64_t remainder = original % count;
            int64_t sum = 0;
            int index;
            CHECK(parts != NULL && xrtValueType(parts) == XVALUE_ARRAY);
            CHECK(xrtValueCount(parts) == (size_t)count);
            for (index = 0; index < count; ++index) {
                int64_t actual;
                int64_t expected = quotient + (index < (remainder < 0 ? -remainder : remainder) ?
                    (remainder < 0 ? -1 : 1) : 0);
                CHECK(xrtValueGetInt(xrtValueArrayAt(parts, index), &actual));
                CHECK(actual == expected);
                sum += actual;
            }
            CHECK(sum == original);
            xrtValueRelease(parts);
        }
    }
    return 0;
}

static int test_allocation_errors(void) {
    char *formatted;
    xvalue *parts;
    unsigned failure, splitFailures = 0;
    xrtClearError();
    CHECK(xrtMemDebugFailAfter(0u));
    formatted = xm_format(123, "USD", 3u);
    CHECK(xrtMemDebugFailTriggered());
    xrtMemDebugFailClear();
    CHECK(formatted == NULL && xrtGetError() != NULL &&
        xrtErrorKind(xrtGetError()) == XERR_MEMORY);
    xrtClearError();

    for (failure = 0; failure < 24u; ++failure) {
        int triggered;
        CHECK(xrtMemDebugFailAfter(failure));
        parts = xm_split(101, 3);
        triggered = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        if (triggered) {
            CHECK((parts == NULL || xrtValueType(parts) == XVALUE_NULL) &&
                xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_MEMORY);
            ++splitFailures;
        } else CHECK(parts != NULL && xrtValueType(parts) == XVALUE_ARRAY &&
            xrtValueCount(parts) == 3);
        xrtValueRelease(parts);
        xrtClearError();
    }
    CHECK(splitFailures >= 2u);
    formatted = xm_format(123, "USD", 3u);
    CHECK(formatted != NULL);
    xrtFree(formatted);
    return 0;
}

static int test_error_recovery(void) {
    int64_t result = 0;
    xvalue *parts;
    char *formatted;
    CHECK(!xm_add_checked(INT64_MAX, 1, &result) && xrtGetError() != NULL);
    CHECK(xm_add_checked(1, 2, &result) && result == 3 && xrtGetError() == NULL);
    CHECK(!xm_scale_checked(1, 1, 0, &result) && xrtGetError() != NULL);
    CHECK(xm_scale_checked(3, 1, 2, &result) && result == 2 && xrtGetError() == NULL);
    CHECK(xm_format(1, "bad!", 4) == NULL && xrtGetError() != NULL);
    formatted = xm_format(1, "USD", 3);
    CHECK(formatted != NULL && xrtGetError() == NULL);
    xrtFree(formatted);
    CHECK(xm_symbol("bad!", 4)[0] == '\0' && xrtGetError() != NULL);
    CHECK(xm_exponent("USD", 3) == 2 && xrtGetError() == NULL);
    parts = xm_split(1, 0);
    CHECK(parts != NULL && xrtValueType(parts) == XVALUE_NULL && xrtGetError() != NULL);
    xrtValueRelease(parts);
    parts = xm_split(3, 2);
    CHECK(parts != NULL && xrtValueType(parts) == XVALUE_ARRAY && xrtGetError() == NULL);
    xrtValueRelease(parts);
    return 0;
}

int main(void) {
    if (test_arithmetic() || test_scale_reference() || test_format_and_split() ||
        test_split_reference() ||
        test_allocation_errors() || test_error_recovery()) return 1;
    puts("money C: PASS");
    return 0;
}
