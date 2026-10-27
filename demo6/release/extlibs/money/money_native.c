/* 金额以 int64 最小货币单位存储；所有算术先检查，再写入结果。 */
#include "include/money_native.h"

#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

#ifndef XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_VALUE_CONTAINER
#endif
#ifndef XRT_MODULE_STRING
#define XRT_MODULE_STRING
#endif
#include "xrt.h"

typedef struct {
    const char *code;
    const char *symbol;
    int exponent;
} XmCurrency;

static const XmCurrency xm_currencies[] = {
    {"USD", "$", 2}, {"EUR", "\xE2\x82\xAC", 2}, {"GBP", "\xC2\xA3", 2},
    {"CNY", "\xEF\xBF\xA5", 2}, {"JPY", "\xC2\xA5", 0}, {"KRW", "\xE2\x82\xA9", 0},
    {"HKD", "HK$", 2}, {"MOP", "MOP$", 2}, {"TWD", "NT$", 2},
    {"SGD", "S$", 2}, {"AUD", "A$", 2}, {"NZD", "NZ$", 2},
    {"CAD", "C$", 2}, {"CHF", "CHF", 2}, {"SEK", "kr", 2},
    {"NOK", "kr", 2}, {"DKK", "kr", 2}, {"RUB", "\xE2\x82\xBD", 2},
    {"INR", "\xE2\x82\xB9", 2}, {"BRL", "R$", 2}, {"MXN", "MX$", 2},
    {"VND", "\xE2\x82\xAB", 0}, {"THB", "\xE0\xB8\xBF", 2}
};

static int xm_fail(int code, const char *message) {
    xrtSetErrorInfo(code, "xlang.extlib.money", 0, message);
    return 0;
}

static uint64_t xm_magnitude(int64_t value) {
    return value < 0 ? (uint64_t)(-(value + 1)) + 1 : (uint64_t)value;
}

static int xm_code(const char *code, size_t length, char upper[4]) {
    size_t i;
    xrtClearError();
    if (code == NULL || length != 3) return xm_fail(XERR_VALUE, "currency must contain three ASCII letters");
    for (i = 0; i < 3; ++i) {
        unsigned char c = (unsigned char)code[i];
        if (c >= 'a' && c <= 'z') c = (unsigned char)(c - 'a' + 'A');
        if (c < 'A' || c > 'Z') return xm_fail(XERR_VALUE, "currency must contain three ASCII letters");
        upper[i] = (char)c;
    }
    upper[3] = '\0';
    return 1;
}

static const XmCurrency *xm_find(const char upper[4]) {
    size_t i;
    for (i = 0; i < sizeof(xm_currencies) / sizeof(xm_currencies[0]); ++i) {
        if (memcmp(upper, xm_currencies[i].code, 3) == 0) return &xm_currencies[i];
    }
    return NULL;
}

/* ------------ 货币元数据与格式化 ------------ */

const char *xm_symbol(const char *code, size_t length) {
    char upper[4];
    const XmCurrency *currency;
    if (!xm_code(code, length, upper)) return "";
    currency = xm_find(upper);
    return currency != NULL ? currency->symbol : "";
}

int64_t xm_exponent(const char *code, size_t length) {
    char upper[4];
    const XmCurrency *currency;
    if (!xm_code(code, length, upper)) return -1;
    currency = xm_find(upper);
    return currency != NULL ? currency->exponent : 2;
}

char *xm_format(int64_t amount, const char *code, size_t length) {
    char upper[4], text[64], digits[32];
    const XmCurrency *currency;
    const char *prefix;
    uint64_t absolute, major, fraction, unit;
    size_t pos = 0, used = 0, i;
    int exponent;
    if (!xm_code(code, length, upper)) return NULL;
    currency = xm_find(upper);
    prefix = currency != NULL ? currency->symbol : upper;
    exponent = currency != NULL ? currency->exponent : 2;
    unit = exponent == 0 ? 1 : 100;
    absolute = xm_magnitude(amount);
    major = absolute / unit;
    fraction = absolute % unit;

    /* int64 最多 19 位；含分隔符、符号、币种和小数时仍小于 64 字节。 */
    do {
        if (used != 0 && used % 4 == 3) digits[used++] = ',';
        digits[used++] = (char)('0' + major % 10);
        major /= 10;
    } while (major != 0);
    for (i = 0; prefix[i] != '\0'; ++i) text[pos++] = prefix[i];
    if (currency == NULL) text[pos++] = ' ';
    if (amount < 0) text[pos++] = '-';
    while (used != 0) text[pos++] = digits[--used];
    if (exponent != 0) {
        text[pos++] = '.';
        text[pos++] = (char)('0' + fraction / 10);
        text[pos++] = (char)('0' + fraction % 10);
    }
    text[pos] = '\0';
    return xrtStrDup(text);
}

/* ------------ 带溢出检查的整数运算 ------------ */

int xm_add_checked(int64_t a, int64_t b, int64_t *out) {
    xrtClearError();
    if (out == NULL) return xm_fail(XERR_ARGUMENT, "result pointer is null");
    if ((b > 0 && a > INT64_MAX - b) || (b < 0 && a < INT64_MIN - b))
        return xm_fail(XERR_RANGE, "money addition overflow");
    *out = a + b;
    return 1;
}

int xm_sub_checked(int64_t a, int64_t b, int64_t *out) {
    xrtClearError();
    if (out == NULL) return xm_fail(XERR_ARGUMENT, "result pointer is null");
    if ((b < 0 && a > INT64_MAX + b) || (b > 0 && a < INT64_MIN + b))
        return xm_fail(XERR_RANGE, "money subtraction overflow");
    *out = a - b;
    return 1;
}

int xm_mul_checked(int64_t a, int64_t b, int64_t *out) {
    uint64_t left = xm_magnitude(a), right = xm_magnitude(b);
    uint64_t limit = (a < 0) != (b < 0) ? (uint64_t)INT64_MAX + 1 : (uint64_t)INT64_MAX;
    uint64_t product;
    xrtClearError();
    if (out == NULL) return xm_fail(XERR_ARGUMENT, "result pointer is null");
    if (right != 0 && left > limit / right) return xm_fail(XERR_RANGE, "money multiplication overflow");
    product = left * right;
    *out = product == (uint64_t)INT64_MAX + 1 ? INT64_MIN :
        ((a < 0) != (b < 0) ? -(int64_t)product : (int64_t)product);
    return 1;
}

/* 32 位分块相乘与逐位除法：无 __int128 依赖，MSVC/GCC 均可构建。 */
static void xm_product128(uint64_t a, uint64_t b, uint64_t *hi, uint64_t *lo) {
    uint64_t p0 = (a & UINT32_MAX) * (b & UINT32_MAX);
    uint64_t p1 = (a & UINT32_MAX) * (b >> 32);
    uint64_t p2 = (a >> 32) * (b & UINT32_MAX);
    uint64_t p3 = (a >> 32) * (b >> 32);
    uint64_t middle = (p0 >> 32) + (p1 & UINT32_MAX) + (p2 & UINT32_MAX);
    *lo = (p0 & UINT32_MAX) | (middle << 32);
    *hi = p3 + (p1 >> 32) + (p2 >> 32) + (middle >> 32);
}

int xm_scale_checked(int64_t amount, int64_t numerator, int64_t denominator, int64_t *out) {
    uint64_t hi, lo, divisor, remainder = 0, quotient = 0, limit;
    int negative, bit;
    xrtClearError();
    if (out == NULL) return xm_fail(XERR_ARGUMENT, "result pointer is null");
    if (denominator == 0) return xm_fail(XERR_VALUE, "money scale denominator is zero");
    negative = (amount < 0) != (numerator < 0);
    negative ^= denominator < 0;
    divisor = xm_magnitude(denominator);
    limit = negative ? (uint64_t)INT64_MAX + 1 : (uint64_t)INT64_MAX;
    xm_product128(xm_magnitude(amount), xm_magnitude(numerator), &hi, &lo);
    if (hi == 0) {
        quotient = lo / divisor;
        remainder = lo % divisor;
    } else {
        for (bit = 127; bit >= 0; --bit) {
            remainder = (remainder << 1) | (bit >= 64 ? ((hi >> (bit - 64)) & 1) : ((lo >> bit) & 1));
            if (remainder >= divisor) {
                remainder -= divisor;
                if (bit >= 64) return xm_fail(XERR_RANGE, "money scale overflow");
                quotient |= UINT64_C(1) << bit;
            }
        }
    }
    if (remainder >= (divisor >> 1) + (divisor & 1)) {
        if (quotient == UINT64_MAX) return xm_fail(XERR_RANGE, "money scale overflow");
        ++quotient;
    }
    if (quotient > limit) return xm_fail(XERR_RANGE, "money scale overflow");
    *out = quotient == (uint64_t)INT64_MAX + 1 ? INT64_MIN :
        (negative ? -(int64_t)quotient : (int64_t)quotient);
    return 1;
}

int xm_rate_checked(int64_t amount, double rate, int64_t *out) {
    long double rounded;
    xrtClearError();
    if (out == NULL) return xm_fail(XERR_ARGUMENT, "result pointer is null");
    if (!isfinite(rate)) return xm_fail(XERR_VALUE, "money rate must be finite");
    if (rate == 1.0) {
        *out = amount;
        return 1;
    }
    rounded = roundl((long double)amount * (long double)rate);
    if (rounded < -9223372036854775808.0L || rounded >= 9223372036854775808.0L)
        return xm_fail(XERR_RANGE, "money rate overflow");
    *out = (int64_t)rounded;
    return 1;
}

/* XLang extern 使用 xvalue 传递失败状态；C 调用方可直接用 checked API。 */
static xvalue *xm_result(int ok, int64_t amount) {
    return ok ? xrtValueInt(amount) : xrtValueNull();
}

xvalue *xm_add(int64_t a, int64_t b) {
    int64_t result = 0;
    return xm_result(xm_add_checked(a, b, &result), result);
}

xvalue *xm_sub(int64_t a, int64_t b) {
    int64_t result = 0;
    return xm_result(xm_sub_checked(a, b, &result), result);
}

xvalue *xm_mul(int64_t a, int64_t b) {
    int64_t result = 0;
    return xm_result(xm_mul_checked(a, b, &result), result);
}

xvalue *xm_scale(int64_t amount, int64_t numerator, int64_t denominator) {
    int64_t result = 0;
    return xm_result(xm_scale_checked(amount, numerator, denominator, &result), result);
}

xvalue *xm_rate(int64_t amount, double rate) {
    int64_t result = 0;
    return xm_result(xm_rate_checked(amount, rate, &result), result);
}

xvalue *xm_split(int64_t amount, int64_t n) {
    xvalue *array;
    int64_t whole, remainder, i;
    xrtClearError();
    if (n <= 0 || n > 1000000) {
        xm_fail(XERR_VALUE, "split requires 1..1000000 parts");
        return xrtValueNull();
    }
    array = xrtValueArray();
    if (array == NULL) return xrtValueNull();
    whole = amount / n;
    remainder = amount % n;
    for (i = 0; i < n; ++i) {
        int64_t part = whole;
        if (i < (remainder < 0 ? -remainder : remainder)) part += remainder < 0 ? -1 : 1;
        if (!xrtValueArrayAppendNew(array, xrtValueInt(part))) {
            xrtValueRelease(array);
            return xrtValueNull();
        }
    }
    return array;
}
