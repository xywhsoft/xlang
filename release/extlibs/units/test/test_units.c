#define XRT_MODULE_MEMORY_DEBUG
#include "xrt.h"
#include "../include/units_native.h"
#include <ctype.h>
#include <math.h>
#include <fenv.h>
#include <locale.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "units C test failed at line %d: %s\n", __LINE__, #condition); \
    return 1; \
} } while (0)

static int same(void* value, const char* expected)
{
    int valid = value != NULL && strcmp((const char*)value, expected) == 0;
    xrtFree(value);
    return valid;
}

static double parse_bytes(const char* text)
{
    return xu_parse_bytes(text, (long long)strlen(text));
}

static double parse_duration(const char* text)
{
    return xu_parse_duration(text, (long long)strlen(text));
}

/* 格式化长时长只含整秒；任意合法整数秒应可被本包解析器无损读回。 */
static int test_duration_roundtrip(void)
{
    uint64_t state = 0x9e3779b97f4a7c15ULL;
    unsigned i;
    for (i = 0; i < 4096u; ++i) {
        int64_t seconds;
        char* text;
        state = state * 6364136223846793005ULL + 1u;
        seconds = (int64_t)(state % 1000000000001ULL);
        if ((i & 1u) != 0u) seconds = -seconds;
        text = (char*)xu_duration((double)seconds);
        if (text == NULL) return 0;
        if (parse_duration(text) != (double)seconds || xrtGetError() != NULL) {
            xrtFree(text);
            return 0;
        }
        xrtFree(text);
    }
    return 1;
}

/* 格式化结果可由本包解析，小数点不应随 LC_NUMERIC 改变。 */
static int test_numeric_locale(void)
{
    static const char* locales[] = {
        "French_France.1252", "fr-FR", "German_Germany.1252", "de-DE",
        "fr_FR.UTF-8", "de_DE.UTF-8"
    };
    const char* current = setlocale(LC_NUMERIC, NULL);
    char saved[128];
    size_t i;
    int ok;
    if (current == NULL || strlen(current) >= sizeof(saved)) return 0;
    strcpy(saved, current);
    for (i = 0; i < sizeof(locales) / sizeof(locales[0]); ++i)
        if (setlocale(LC_NUMERIC, locales[i]) != NULL &&
            strcmp(localeconv()->decimal_point, ",") == 0) break;
    if (i == sizeof(locales) / sizeof(locales[0])) {
        setlocale(LC_NUMERIC, saved);
        return 1;
    }
    ok = same(xu_bytes(1536.0), "1.50 KiB") &&
        same(xu_bytes_decimal(1500.0), "1.50 kB") &&
        same(xu_count(1200.0), "1.2K") &&
        parse_bytes("1.50 KiB") == 1536.0;
    setlocale(LC_NUMERIC, saved);
    return ok;
}

/* ASCII 单位不受土耳其语等区域设置中的 i/I 大小写规则影响。 */
static int test_suffix_locale(void)
{
    static const char* locales[] = {
        "Turkish_Turkey.1254", "tr-TR", "tr_TR.UTF-8", "tr_TR.utf8"
    };
    const char* current = setlocale(LC_CTYPE, NULL);
    char saved[128];
    size_t i;
    int ok;
    if (current == NULL || strlen(current) >= sizeof(saved)) return 0;
    strcpy(saved, current);
    for (i = 0; i < sizeof(locales) / sizeof(locales[0]); ++i)
        if (setlocale(LC_CTYPE, locales[i]) != NULL &&
            toupper((unsigned char)'i') != 'I') break;
    if (i == sizeof(locales) / sizeof(locales[0])) {
        setlocale(LC_CTYPE, saved);
        return 1;
    }
    ok = parse_bytes("1 kib") == 1024.0 &&
        parse_bytes("1 mib") == 1048576.0;
    setlocale(LC_CTYPE, saved);
    return ok;
}

/* 与语言回归使用相同预算：仅最终文本需要一次 XRT 分配。 */
static int test_allocation_failure(void)
{
    char* (*formats[])(double) = {
        xu_bytes, xu_bytes_decimal, xu_duration, xu_duration_short, xu_count
    };
    const char* expected[] = {
        "1.50 KiB", "1.54 kB", "25m 36s", "25:36", "1.5K"
    };
    size_t i;
    for (i = 0; i < sizeof(formats) / sizeof(formats[0]); ++i) {
        unsigned budget;
        for (budget = 0; budget < 2u; ++budget) {
            char* result;
            bool triggered;
            bool memory_error;
            CHECK(xrtMemDebugFailAfter(budget));
            result = formats[i](1536.0);
            triggered = xrtMemDebugFailTriggered();
            memory_error = xrtErrorKind(xrtGetError()) == XERR_MEMORY;
            xrtMemDebugFailClear();
            if (budget == 0u) {
                CHECK(result == NULL && triggered && memory_error);
            } else {
                CHECK(!triggered && !memory_error);
                CHECK(same(result, expected[i]));
                CHECK(xrtGetError() == NULL);
            }
        }
        CHECK(same(formats[i](1536.0), expected[i]));
        CHECK(xrtGetError() == NULL);
    }
    return 0;
}

int main(void)
{
    char* first;
    char* second;
    CHECK(same(xu_bytes(0.0), "0 B"));
    CHECK(same(xu_bytes(-0.0), "0 B"));
    CHECK(same(xu_bytes(-0.4), "0 B"));
    CHECK(same(xu_bytes(-0.6), "-1 B"));
    CHECK(same(xu_bytes(1024.0), "1.00 KiB"));
    CHECK(same(xu_bytes(1536.0), "1.50 KiB"));
    CHECK(same(xu_bytes(-1536.0), "-1.50 KiB"));
    CHECK(same(xu_bytes(1023.9), "1.00 KiB"));
    CHECK(same(xu_bytes(1048575.0), "1.00 MiB"));
    CHECK(same(xu_bytes(-1048575.0), "-1.00 MiB"));
    CHECK(same(xu_bytes_decimal(1000.0), "1.00 kB"));
    CHECK(same(xu_bytes_decimal(-0.4), "0 B"));
    CHECK(same(xu_bytes_decimal(1500000.0), "1.50 MB"));
    CHECK(same(xu_bytes_decimal(999.9), "1.00 kB"));
    CHECK(same(xu_bytes_decimal(999999.0), "1.00 MB"));
    CHECK(parse_bytes("10 KB") == 10000.0);
    CHECK(parse_bytes("1.5MiB") == 1572864.0);
    CHECK(parse_bytes("1.5 mib") == 1572864.0);
    CHECK(parse_bytes("1 kib") == 1024.0);
    CHECK(parse_bytes("-1 B") == -1.0);
    CHECK(parse_bytes("1.50 KiB") == 1536.0);
    CHECK(parse_bytes("1.50 kB") == 1500.0);
    CHECK(isnan(parse_bytes("1K")));
    CHECK(isnan(parse_bytes("1e3B")));
    CHECK(isnan(parse_bytes("")));
    CHECK(isnan(parse_bytes("1 MB extra")));
    CHECK(isnan(parse_bytes("10000000000000000000000 B")));
    CHECK(isnan(xu_parse_bytes("1\0MB", 4)));
    CHECK(isnan(xu_parse_bytes(NULL, 1)));
    CHECK(xu_bytes(INFINITY) == NULL);
    CHECK(xu_bytes(NAN) == NULL);
    CHECK(xu_bytes(1e22) == NULL);

    CHECK(same(xu_duration(0.0), "0s"));
    CHECK(same(xu_duration(60.0), "1m"));
    CHECK(same(xu_duration(3725.0), "1h 2m 5s"));
    CHECK(same(xu_duration(93784.0), "1d 2h 3m 4s"));
    CHECK(same(xu_duration(-3725.0), "-1h 2m 5s"));
    CHECK(same(xu_duration(59.6), "1m"));
    CHECK(same(xu_duration_short(45.0), "0:45"));
    CHECK(same(xu_duration_short(3725.0), "1:02:05"));
    CHECK(same(xu_duration_short(93784.0), "1d02:03:04"));
    CHECK(same(xu_duration_short(-3725.0), "-1:02:05"));
    CHECK(parse_duration("1d2h30m") == 95400.0);
    CHECK(parse_duration("-1h30m") == -5400.0);
    CHECK(parse_duration("-1") == -1.0);
    CHECK(parse_duration("1d2h30m4s") == 95404.0);
    CHECK(test_duration_roundtrip());
    CHECK(isnan(parse_duration("1h2h")));
    CHECK(isnan(parse_duration("1m2h")));
    CHECK(isnan(parse_duration("1d2h3m4s5s")));
    CHECK(isnan(parse_duration("1d2")));
    CHECK(isnan(parse_duration("1d2h30m#")));
    CHECK(isnan(parse_duration("1e9s")));
    CHECK(isnan(parse_duration("")));
    CHECK(isnan(xu_parse_duration("1\0h", 3)));
    CHECK(xu_duration(1e13) == NULL);
    CHECK(same(xu_count(1200.0), "1.2K"));
    CHECK(same(xu_count(999.96), "1.0K"));
    CHECK(same(xu_count(999960.0), "1.0M"));
    CHECK(same(xu_count(-999960.0), "-1.0M"));
    {
        int rounding = fegetround();
        /* TCC 运行环境可能不提供可切换的 fenv。 */
        if (rounding != -1 && fesetround(FE_UPWARD) == 0) {
            CHECK(same(xu_bytes(-1023.1), "-1023 B"));
            CHECK(same(xu_count(-999.1), "-999"));
            CHECK(fesetround(rounding) == 0);
        }
        if (rounding != -1 && fesetround(FE_DOWNWARD) == 0) {
            CHECK(same(xu_bytes(1048575.0), "1.00 MiB"));
            CHECK(same(xu_bytes_decimal(999999.0), "1.00 MB"));
            CHECK(same(xu_count(999960.0), "1.0M"));
            CHECK(fesetround(rounding) == 0);
        }
    }
    CHECK(same(xu_count(-0.4), "0"));
    CHECK(same(xu_count(-0.6), "-1"));
    CHECK(same(xu_count(3400000.0), "3.4M"));
    CHECK(xu_count(INFINITY) == NULL);
    CHECK(xu_is_pow2(1) == 1 && xu_is_pow2(64) == 1);
    CHECK(xu_is_pow2(0) == 0 && xu_is_pow2(-2) == 0 && xu_is_pow2(63) == 0);

    /* 成功入口不得继承上一次失败留下的线程错误。 */
    CHECK(xu_bytes(INFINITY) == NULL && xrtGetError() != NULL);
    CHECK(same(xu_bytes(1.0), "1 B") && xrtGetError() == NULL);
    CHECK(isnan(parse_bytes("bad")) && xrtGetError() != NULL);
    CHECK(parse_bytes("2 B") == 2.0 && xrtGetError() == NULL);
    CHECK(isnan(parse_duration("bad")) && xrtGetError() != NULL);
    CHECK(parse_duration("2s") == 2.0 && xrtGetError() == NULL);
    CHECK(xu_count(INFINITY) == NULL && xrtGetError() != NULL);
    CHECK(same(xu_count(2.0), "2") && xrtGetError() == NULL);
    CHECK(xu_duration(INFINITY) == NULL && xrtGetError() != NULL);
    CHECK(same(xu_duration_short(2.0), "0:02") && xrtGetError() == NULL);

    first = (char*)xu_bytes(1536.0);
    second = (char*)xu_duration(3725.0);
    CHECK(first != NULL && second != NULL && first != second);
    CHECK(strcmp(first, "1.50 KiB") == 0 && strcmp(second, "1h 2m 5s") == 0);
    xrtFree(first);
    xrtFree(second);
    CHECK(test_numeric_locale());
    CHECK(test_suffix_locale());
    CHECK(test_allocation_failure() == 0);
    puts("units C boundaries: PASS");
    return 0;
}
