#define XRT_MODULE_ALL
#include "xrt.h"
#include "../json5_native.h"
#include <stdio.h>
#include <string.h>

#define CHECK(test) do { if (!(test)) { \
    fprintf(stderr, "JSON5 number failed at %d: %s\n", __LINE__, #test); return 1; \
} } while (0)

static bool same_bits(xvalue *value, uint64 expected)
{
    double number = 0;
    uint64 actual = 0;
    bool ok = value && !xrtGetError() && xrtValueType(value) == XVALUE_FLOAT &&
        xrtValueGetFloat(value, &number);
    memcpy(&actual, &number, sizeof(actual));
    if (!ok || actual != expected) {
        fprintf(stderr, "expected %016llx, actual %016llx\n",
            (unsigned long long)expected, (unsigned long long)actual);
        ok = false;
    }
    xrtValueRelease(value);
    return ok;
}

static bool check_number(const char *source, uint64 expected)
{
    size_t size = strlen(source);
    double parsed;
    uint64 actual = 0;
    char *text;
    bool ok;
    if (!same_bits(xjson5_parse_len(source, size), expected) ||
        !same_bits(xjson5_get_value_len(source, size, ""), expected)) return false;
    text = xjson5_get_text_len(source, size, "");
    ok = text && !xrtGetError() && xrtNumParse((xstrview){text, strlen(text)}, 0u, &parsed);
    if (ok) memcpy(&actual, &parsed, sizeof(actual));
    xrtFree(text);
    return ok && actual == expected && xjson5_valid_len(source, size) && !xrtGetError();
}

int main(void)
{
    static const struct { const char *source; uint64 bits; } cases[] = {
        {"0x8000000000000401", UINT64_C(0x43e0000000000001)},
        {"0x80000000000003ff", UINT64_C(0x43e0000000000000)},
        {"0x8000000000000400", UINT64_C(0x43e0000000000000)},
        {"0x8000000000000800", UINT64_C(0x43e0000000000001)},
        {"0x8000000000000bff", UINT64_C(0x43e0000000000001)},
        {"0x8000000000000c00", UINT64_C(0x43e0000000000002)},
        {"0x8000000000000c01", UINT64_C(0x43e0000000000002)},
        {"0xfffffffffffffbff", UINT64_C(0x43efffffffffffff)},
        {"0xfffffffffffffc00", UINT64_C(0x43f0000000000000)},
        {"0xfffffffffffffc01", UINT64_C(0x43f0000000000000)},
        {"0x10000000000000800", UINT64_C(0x43f0000000000000)},
        {"0x10000000000000801", UINT64_C(0x43f0000000000001)}
    };
    char source[280];
    size_t i, variant;

    {
        static const struct { const char *source; int64 number; } integers[] = {
            {"0x7fffffffffffffff", INT64_MAX}, {"-0x8000000000000000", INT64_MIN},
            {"9223372036854775807", INT64_MAX}, {"-9223372036854775808", INT64_MIN},
            {"+0x000000000000002a", 42}, {"-0x000000000000002a", -42}, {"0x0000", 0}
        };
        for (i = 0; i < sizeof(integers) / sizeof(integers[0]); ++i) {
            int64 number;
            xvalue *value = xjson5_parse_len(integers[i].source, strlen(integers[i].source));
            CHECK(value && !xrtGetError() && xrtValueType(value) == XVALUE_INT &&
                xrtValueGetInt(value, &number) && number == integers[i].number);
            xrtValueRelease(value);
        }
        CHECK(same_bits(xjson5_parse_len("-0x0000", 7), UINT64_C(0x8000000000000000)));
    }

    /* 位模式独立固定；同时验证符号、前导零和 JSON5 数据格式的大小写前缀。 */
    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        for (variant = 0; variant < 6; ++variant) {
            const char *sign = variant % 3 == 0 ? "" : variant % 3 == 1 ? "+" : "-";
            uint64 expected = cases[i].bits | (variant % 3 == 2 ? UINT64_C(1) << 63 : 0);
            snprintf(source, sizeof(source), "%s0%c%s%s", sign, variant & 1 ? 'X' : 'x',
                variant >= 3 ? "00000000" : "", cases[i].source + 2);
            CHECK(check_number(source, expected));
        }
    }
    CHECK(check_number("9223372036854775808", UINT64_C(0x43e0000000000000)));
    CHECK(check_number("-9223372036854775809", UINT64_C(0xc3e0000000000000)));
    CHECK(check_number("18446744073709551615", UINT64_C(0x43f0000000000000)));

    /* 只差末尾一位的 1024-bit 边界：最大有限值、溢出中点之下与中点本身。 */
    memcpy(source, "0xfffffffffffff8", 16);
    memset(source + 16, '0', 242);
    source[258] = 0;
    CHECK(check_number(source, UINT64_C(0x7fefffffffffffff)));
    memcpy(source, "0xfffffffffffffb", 16);
    memset(source + 16, 'f', 242);
    CHECK(check_number(source, UINT64_C(0x7fefffffffffffff)));
    memcpy(source, "0xfffffffffffffc", 16);
    memset(source + 16, '0', 242);
    xvalue *overflow = xjson5_parse_len(source, 258);
    CHECK(overflow && xrtValueType(overflow) == XVALUE_NULL && xrtGetError() &&
        xrtErrorKind(xrtGetError()) == XERR_VALUE);
    xrtValueRelease(overflow);
    CHECK(!xjson5_valid_len(source, 258) && !xrtGetError());

    /* 大整数探测产生的临时错误不能污染成功，也不能掩盖真正的内存失败。 */
    {
        const char* documents[] = {
            "{n:0x8000000000000401,tail:[1,2]}",
            "{n:9223372036854775808,tail:[1,2]}",
            "{n:1.234567890123456789012345678901,tail:[1,2]}"
        };
        for (size_t document = 0; document < sizeof(documents) / sizeof(documents[0]); ++document) {
            size_t failures = 0;
            for (i = 0; i < 128; ++i) {
                xmemdebugsnapshot before, after;
                xvalue *value;
                bool triggered;
                xrtClearError();
                xrtMemDebugSnapshot(&before);
                CHECK(xrtMemDebugFailAfter(i));
                value = xjson5_parse_len(documents[document], strlen(documents[document]));
                triggered = xrtMemDebugFailTriggered();
                xrtMemDebugFailClear();
                CHECK(value != NULL);
                if (triggered) {
                    if (!xrtGetError() || xrtErrorKind(xrtGetError()) != XERR_MEMORY)
                        fprintf(stderr, "allocation %llu: type=%d error=%d message=%s\n",
                            (unsigned long long)i, (int)xrtValueType(value),
                            xrtGetError() ? (int)xrtErrorKind(xrtGetError()) : 0,
                            xrtGetError() ? xrtErrorMessage(xrtGetError()) : "none");
                    CHECK(xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_MEMORY);
                    ++failures;
                } else CHECK(!xrtGetError());
                xrtValueRelease(value);
                xrtClearError();
                xrtMemDebugSnapshot(&after);
                CHECK(after.LiveCount == before.LiveCount && after.LiveBytes == before.LiveBytes);
                if (!triggered) break;
            }
            CHECK(failures != 0 && i < 128);
        }
    }
    puts("json5 hexadecimal rounding: PASS");
    return 0;
}
