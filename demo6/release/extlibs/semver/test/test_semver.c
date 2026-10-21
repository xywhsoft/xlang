#define XRT_MODULE_MEMORY_STATS
#define XRT_MODULE_MEMORY_DEBUG
#include "xrt.h"
#include "../semver_native.h"
#include <stdio.h>
#include <string.h>

#define CHECK(test) do { if (!(test)) { \
    fprintf(stderr, "semver test failed at line %d: %s\n", __LINE__, #test); \
    return 1; \
} } while (0)

static int test_error_contract(void)
{
    static void* (*const text_functions[])(const char*) = {
        xsemver_prerelease, xsemver_build, xsemver_clean
    };
    size_t i;
    xrtSetErrorInfo(XERR_VALUE, "semver.test", 0, "stale error");
    CHECK(xsemver_valid("1.2.3") && xrtGetError() == NULL);
    xrtSetErrorInfo(XERR_VALUE, "semver.test", 0, "stale error");
    CHECK(xsemver_major("1.2.3") == 1 && xrtGetError() == NULL);
    xrtSetErrorInfo(XERR_VALUE, "semver.test", 0, "stale error");
    CHECK(xsemver_minor("1.2.3") == 2 && xrtGetError() == NULL);
    xrtSetErrorInfo(XERR_VALUE, "semver.test", 0, "stale error");
    CHECK(xsemver_patch("1.2.3") == 3 && xrtGetError() == NULL);
    xrtSetErrorInfo(XERR_VALUE, "semver.test", 0, "stale error");
    CHECK(xsemver_compare("1.2.3", "1.2.3") == 0 && xrtGetError() == NULL);
    xrtSetErrorInfo(XERR_VALUE, "semver.test", 0, "stale error");
    CHECK(xsemver_satisfies("1.2.3", "^1.0.0") && xrtGetError() == NULL);
    for (i = 0u; i < sizeof(text_functions) / sizeof(text_functions[0]); ++i) {
        void* result;
        xrtSetErrorInfo(XERR_VALUE, "semver.test", 0, "stale error");
        result = text_functions[i]("1.2.3-alpha+build");
        CHECK(result != NULL && xrtGetError() == NULL);
        xrtFree(result);
        CHECK(xrtMemDebugFailAfter(0u));
        result = text_functions[i]("1.2.3-alpha+build");
        CHECK(xrtMemDebugFailTriggered());
        xrtMemDebugFailClear();
        CHECK(result == NULL && xrtGetError() != NULL &&
            xrtErrorKind(xrtGetError()) == XERR_MEMORY);
        result = text_functions[i]("1.2.3-alpha+build");
        CHECK(result != NULL && xrtGetError() == NULL);
        xrtFree(result);
    }
    return 0;
}

int main(void)
{
    static const struct {
        const char *version;
        const char *range;
        bool expected;
    } range_cases[] = {
        { "1.2.2", "<=1.2.3", true },
        { "1.2.3", "<=1.2.3", true },
        { "1.2.4", "<=1.2.3", false },
        { "1.2.3", ">= 1.2.3 < 2.0.0", true },
        { "2.0.0", ">= 1.2.3 < 2.0.0", false },
        { "1.2.3", "~\t1.2", true },
        { "2.5.0", "^ 1.0.0 || ^ 2.0.0", true },
        { "1.2.9", "<=1.2", true },
        { "1.3.0", "<=1.2", false },
        { "1.3.0", ">1.2", true },
        { "1.2.9", ">1.2", false },
        { "0.0.3", "^0.0.3", true },
        { "0.0.4", "^0.0.3", false },
        { "0.2.9", "^0.2.3", true },
        { "0.3.0", "^0.2.3", false },
        { "1.2.9", "~1.2.3", true },
        { "1.3.0", "~1.2.3", false },
        { "2.3.9", "1.2.3 - 2.3", true },
        { "2.4.0", "1.2.3 - 2.3", false },
        { "1.2.3-alpha.2", ">=1.2.3-alpha.1 <2.0.0", true },
        { "1.2.4-alpha.1", ">=1.2.3-alpha.1 <2.0.0", false },
        { "2.5.0", "^1.0.0 || ^2.0.0", true }
    };
    char long_range[600];
    char limit_range[513];
    char comparator_range[160] = "";
    char spaced_comparator_range[400] = "";
    char long_version[520];
    char oversized_version[8192];
    char unterminated_version[1024];
    char unterminated_range[512];
    static const char ascii_space[] = { ' ', '\t', '\n', '\r', '\f', '\v' };
    char clean_input[32];
    xmemstats stats;
    void *first;
    void *second;
    void *cleaned;
    size_t i;

    CHECK(test_error_contract() == 0);

    CHECK(xsemver_valid("1.2.3+build-5"));
    CHECK(xsemver_valid("1.2.3-alpha+build-5"));
    CHECK(!xsemver_valid("9223372036854775808.0.0"));
    /* 超过格式上限后应直接拒绝，不继续扫描调用方缓冲区。 */
    memset(unterminated_version, 'a', sizeof(unterminated_version));
    memset(unterminated_range, 'a', sizeof(unterminated_range));
    CHECK(!xsemver_valid(unterminated_version));
    CHECK(!xsemver_satisfies("1.2.3", unterminated_range));
    CHECK(!xsemver_valid("1.2.3-01"));
    CHECK(!xsemver_valid("1.2.3-\xC3\xA9"));
    CHECK(xsemver_compare("1.0.0+build-5", "1.0.0+build-6") == 0);
    CHECK(xsemver_compare("1.0.0-alpha.2", "1.0.0-alpha.11") < 0);

    /* clean 是显式清理入口，六种 ASCII 空白均可出现在首尾。 */
    for (i = 0; i < sizeof(ascii_space); ++i) {
        snprintf(clean_input, sizeof(clean_input), "%cv1.2.3+build-5%c",
            ascii_space[i], ascii_space[i]);
        cleaned = xsemver_clean(clean_input);
        CHECK(cleaned && strcmp(cleaned, "1.2.3+build-5") == 0);
        xrtFree(cleaned);
    }

    /* 分配结果必须独立，且全部由 C 调用方显式释放。 */
    first = xsemver_prerelease("1.2.3-alpha");
    second = xsemver_build("1.2.3+build-5");
    CHECK(first && second);
    CHECK(strcmp(first, "alpha") == 0);
    CHECK(strcmp(second, "build-5") == 0);
    xrtFree(first);
    xrtFree(second);

    CHECK(xsemver_satisfies("1.2.3", ">=1.0.0 <2.0.0"));
    CHECK(xsemver_satisfies("1.2.3", " \t "));
    CHECK(!xsemver_satisfies("1.2.3-alpha", " \t "));
    CHECK(xsemver_satisfies("1.2.2", "<=1.2.3"));
    CHECK(xsemver_satisfies("1.2.3", "<=1.2.3"));
    CHECK(!xsemver_satisfies("1.2.4", "<=1.2.3"));
    CHECK(xsemver_satisfies("2.5.0", "^1.0.0 || ^2.0.0"));
    CHECK(!xsemver_satisfies("1.2.3", ">=1.0.0 || invalid"));
    CHECK(!xsemver_satisfies("1.2.3", "<1.0.0 ||"));
    CHECK(!xsemver_satisfies("1.2.3", "1..3"));
    CHECK(!xsemver_satisfies("1.2.3", "1.x.3"));
    CHECK(!xsemver_satisfies("1.2.3", ">="));
    CHECK(!xsemver_satisfies("1.2.3", "<="));
    CHECK(!xsemver_satisfies("1.2.3", ">"));
    CHECK(!xsemver_satisfies("1.2.3", "<"));
    CHECK(!xsemver_satisfies("1.2.3", "="));
    CHECK(!xsemver_satisfies("1.2.3", "^"));
    CHECK(!xsemver_satisfies("1.2.3", "~"));
    CHECK(!xsemver_satisfies("1.2.3", ">= "));
    CHECK(!xsemver_satisfies("1.2.3", "1.2.3 || >="));
    CHECK(xsemver_satisfies("1.2.3", "*"));
    CHECK(!xsemver_satisfies("1.2.3", ">=9223372036854775808.0.0"));
    CHECK(xsemver_satisfies("9223372036854775807.0.0",
        "9223372036854775807.0.0"));
    CHECK(xsemver_satisfies("9223372036854775807.0.0",
        ">=9223372036854775807"));
    CHECK(xsemver_satisfies("1.0.0", "<9223372036854775807"));
    CHECK(xsemver_satisfies("1.9223372036854775807.0",
        ">=1.9223372036854775807"));
    CHECK(!xsemver_satisfies("1.0.0", "<=1.9223372036854775807"));
    CHECK(!xsemver_satisfies("1.0.0", "^0.0.9223372036854775807"));
    /* 每个 ^ token 展开为两个比较器，32 个可用，第 33 个应拒绝。 */
    for (i = 0; i < 16; ++i) strcat(comparator_range, "^1.0.0 ");
    CHECK(xsemver_satisfies("1.2.3", comparator_range));
    strcat(comparator_range, "^1.0.0 ");
    CHECK(!xsemver_satisfies("1.2.3", comparator_range));
    for (i = 0; i < 32; ++i) strcat(spaced_comparator_range, ">= 1.0.0 ");
    CHECK(xsemver_satisfies("1.2.3", spaced_comparator_range));
    strcat(spaced_comparator_range, ">= 1.0.0 ");
    CHECK(!xsemver_satisfies("1.2.3", spaced_comparator_range));
    for (i = 0; i < sizeof(range_cases) / sizeof(range_cases[0]); ++i) {
        if (xsemver_satisfies(range_cases[i].version, range_cases[i].range) !=
            range_cases[i].expected) {
            fprintf(stderr, "range mismatch: %s %s\n",
                range_cases[i].version, range_cases[i].range);
            return 1;
        }
    }

    /* 范围超限必须拒绝，不能只解析前 511 字节。 */
    memset(long_range, ' ', sizeof(long_range));
    memcpy(long_range, ">=1.0.0 ||", 10);
    memcpy(long_range + 560, "invalid", 7);
    long_range[567] = '\0';
    CHECK(!xsemver_satisfies("1.2.3", long_range));

    memset(limit_range, ' ', sizeof(limit_range));
    memcpy(limit_range, "1.2.3", 5);
    limit_range[511] = '\0';
    CHECK(xsemver_satisfies("1.2.3", limit_range));
    limit_range[511] = ' ';
    limit_range[512] = '\0';
    CHECK(!xsemver_satisfies("1.2.3", limit_range));

    /* prerelease 与 build 各为允许的 255 字节时，clean 不能截断。 */
    memcpy(long_version, "1.2.3-", 6);
    memset(long_version + 6, 'a', 255);
    long_version[261] = '+';
    memset(long_version + 262, 'b', 255);
    long_version[517] = '\0';
    CHECK(xsemver_valid(long_version));
    cleaned = xsemver_clean(long_version);
    CHECK(cleaned && strcmp(cleaned, long_version) == 0);
    xrtFree(cleaned);

    /* 超长非法版本不得先复制整段输入；首尾空白仍可正常清理。 */
    memset(oversized_version, 'a', sizeof(oversized_version) - 1);
    oversized_version[sizeof(oversized_version) - 1] = '\0';
    xrtMemStatsEnable(true);
    xrtMemStatsReset();
    cleaned = xsemver_clean(oversized_version);
    xrtMemStatsGet(&stats);
    xrtMemStatsEnable(false);
    CHECK(cleaned && strcmp(cleaned, "") == 0);
    CHECK(stats.MallocBytes <= 1024);
    xrtFree(cleaned);
    memset(oversized_version, ' ', sizeof(oversized_version));
    memcpy(oversized_version + 1, "v1.2.3", 6);
    oversized_version[sizeof(oversized_version) - 1] = '\0';
    cleaned = xsemver_clean(oversized_version);
    CHECK(cleaned && strcmp(cleaned, "1.2.3") == 0);
    xrtFree(cleaned);
    return 0;
}
