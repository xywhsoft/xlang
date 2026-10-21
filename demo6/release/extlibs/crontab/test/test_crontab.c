#define XRT_MODULE_TIME
#define XRT_MODULE_TIME_LOCAL
#define XRT_MODULE_MEMORY_DEBUG
#include "xrt.h"
#include "crontab_native.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "crontab C test failed at %d: %s\n", __LINE__, #expr); \
    return 1; \
} } while (0)
#define TEXT_SIZE(text) (sizeof(text) - 1u)

static int64_t local_noon(int year, int month, int day) {
    xdatetime date = {0};
    xtime time;
    date.Year = year;
    date.Month = month;
    date.Day = day;
    date.Hour = 12;
    if (!xrtTimeFromLocal(&date, XTIME_FOLD_EARLIER, &time)) return -1;
    return xrtTimeUnix(time);
}

static int test_parser(void) {
    const char *invalid[] = {
        "", "* *", "* * * * * extra", "60 * * * *", "* 24 * * *",
        "* * 0 * *", "* * * 13 *", "* * * * 9", "*/0 * * * *",
        "1-0 * * * *", "1,,2 * * * *", "1/ * * * *",
        "junk * * * *", "* * * * *\n"
    };
    char longText[1100];
    size_t i;
    for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        CHECK(!xcron_is_valid(invalid[i], strlen(invalid[i])));
    }
    CHECK(!xcron_is_valid("* * * *\0*", 9));
    memset(longText, '1', sizeof(longText));
    CHECK(!xcron_is_valid(longText, sizeof(longText)));
    CHECK(xcron_is_valid("*/15 0-23/2 1,15 * 0,7", TEXT_SIZE("*/15 0-23/2 1,15 * 0,7")));
    CHECK(xcron_is_valid("0 12 * * 1-5", TEXT_SIZE("0 12 * * 1-5")));
    return 0;
}

static int test_calendar(void) {
    int64_t mon = local_noon(2024, 1, 15);
    int64_t sun = local_noon(2024, 1, 14);
    int64_t tue = local_noon(2024, 1, 16);
    int64_t wed = local_noon(2024, 1, 17);
    int64_t tueOdd = local_noon(2024, 1, 23);
    int64_t thuFifteenth = local_noon(2026, 1, 15);
    int64_t beforeLeap = local_noon(2024, 2, 28);
    int64_t leap = local_noon(2024, 2, 29);
    CHECK(sun >= 0 && mon >= 0 && tue >= 0 && wed >= 0 &&
        tueOdd >= 0 && thuFifteenth >= 0 && beforeLeap >= 0 && leap >= 0);
    CHECK(xcron_matches("0 12 * * 0", TEXT_SIZE("0 12 * * 0"), sun));
    CHECK(xcron_matches("0 12 * * 7", TEXT_SIZE("0 12 * * 7"), sun));
    CHECK(xcron_matches("0 12 15 1 2", TEXT_SIZE("0 12 15 1 2"), mon));
    CHECK(xcron_matches("0 12 15 1 2", TEXT_SIZE("0 12 15 1 2"), tue));
    CHECK(!xcron_matches("0 12 15 1 2", TEXT_SIZE("0 12 15 1 2"), wed));
    CHECK(!xcron_matches("0 12 * 1 2", TEXT_SIZE("0 12 * 1 2"), mon));
    CHECK(xcron_matches("0 12 * 1 2", TEXT_SIZE("0 12 * 1 2"), tue));
    CHECK(xcron_matches("0 12 15 1 *", TEXT_SIZE("0 12 15 1 *"), mon));
    CHECK(!xcron_matches("0 12 15 1 *", TEXT_SIZE("0 12 15 1 *"), tue));
    CHECK(!xcron_matches("0 12 15 1 */2", TEXT_SIZE("0 12 15 1 */2"), mon));
    CHECK(!xcron_matches("0 12 15 1 */2", TEXT_SIZE("0 12 15 1 */2"), tue));
    CHECK(xcron_matches("0 12 15 1 */2", TEXT_SIZE("0 12 15 1 */2"), thuFifteenth));
    CHECK(!xcron_matches("0 12 */2 1 2", TEXT_SIZE("0 12 */2 1 2"), tue));
    CHECK(xcron_matches("0 12 */2 1 2", TEXT_SIZE("0 12 */2 1 2"), tueOdd));
    CHECK(xcron_matches("0 12 1-31 1 2", TEXT_SIZE("0 12 1-31 1 2"), mon));
    CHECK(xcron_next("0 12 29 2 *", TEXT_SIZE("0 12 29 2 *"), beforeLeap) == leap);
    CHECK(xcron_is_valid("0 0 31 2 *", TEXT_SIZE("0 0 31 2 *")));
    CHECK(xcron_next("0 0 31 2 *", TEXT_SIZE("0 0 31 2 *"), beforeLeap) == -1);
    CHECK(xcron_next("0 0 31 2 */2", TEXT_SIZE("0 0 31 2 */2"), beforeLeap) == -1);
    {
        const char rule[] = "0 0 31 2 1";
        int64_t next = xcron_next(rule, TEXT_SIZE(rule), beforeLeap);
        CHECK(next > beforeLeap && xcron_matches(rule, TEXT_SIZE(rule), next));
    }
    CHECK(xcron_next("* * * * *", TEXT_SIZE("* * * * *"), -1) == 0);
    CHECK(xcron_next("* * * * *", TEXT_SIZE("* * * * *"), INT64_MAX) == -1);
    return 0;
}

/* 独立枚举近期本地分钟，验证 next 没有跳过更早的匹配点。 */
static bool reference_match(int rule, const xdatetime* date) {
    if (date->Second != 0) return false;
    switch (rule) {
        case 0: return date->Minute % 17 == 0;
        case 1: return date->Minute == 5 && date->Hour == 12 &&
            date->Weekday >= 1 && date->Weekday <= 5;
        case 2: return date->Minute == 0 && date->Hour == 12 &&
            date->Month == 1 && (date->Day == 15 || date->Weekday == 2);
        default: return date->Minute == 13 && date->Hour % 4 == 0;
    }
}

static int test_next_reference(void) {
    static const char* rules[] = {
        "*/17 * * * *", "5 12 * * 1-5", "0 12 15 1 2", "13 */4 * * *"
    };
    const int64_t base = INT64_C(1704067200); /* 2024-01-01 00:00 UTC */
    size_t rule;
    int seed;
    for (rule = 0u; rule < sizeof(rules) / sizeof(rules[0]); ++rule) {
        for (seed = 0; seed < 16; ++seed) {
            int64_t after = base + (int64_t)seed * 86400 + (seed % 4) * 61;
            int64_t expected = -1;
            int64_t candidate = after - after % 60 + 60;
            int minute;
            for (minute = 0; minute < 14 * 1440; ++minute, candidate += 60) {
                xdatetime date = {0};
                xtime time;
                CHECK(xrtTimeFromUnix(candidate, &time) && xrtTimeLocal(time, &date));
                if (reference_match((int)rule, &date)) {
                    expected = candidate;
                    break;
                }
            }
            CHECK(expected != -1);
            CHECK(xcron_next(rules[rule], strlen(rules[rule]), after) == expected);
            CHECK(xcron_matches(rules[rule], strlen(rules[rule]), expected));
        }
    }
    return 0;
}

static int test_description(void) {
    void *first = xcron_describe("* * * * *", TEXT_SIZE("* * * * *"));
    void *second = xcron_describe("0 12 * * 1-5", TEXT_SIZE("0 12 * * 1-5"));
    void *stepped = xcron_describe("*/5 * * * *", TEXT_SIZE("*/5 * * * *"));
    void *bad = xcron_describe("bad", TEXT_SIZE("bad"));
    CHECK(first != NULL && second != NULL && stepped != NULL && bad != NULL);
    CHECK(strcmp((const char *)first, "every minute") == 0);
    CHECK(strstr((const char *)second, "minute=0") != NULL);
    CHECK(strstr((const char *)second, "weekday=1-5") != NULL);
    CHECK(strstr((const char *)stepped, "minute=*/5") != NULL);
    CHECK(strcmp((const char *)bad, "invalid") == 0);
    CHECK(strcmp((const char *)first, "every minute") == 0);
    xrtFree(first);
    xrtFree(second);
    xrtFree(stepped);
    xrtFree(bad);
    return 0;
}

static int test_error_recovery(void) {
    const char rule[] = "* * * * *";
    void *description;
    xrtSetErrorInfo(XERR_VALUE, "test", 0, "previous failure");
    CHECK(xcron_is_valid(rule, TEXT_SIZE(rule)) && xrtGetError() == NULL);
    xrtSetErrorInfo(XERR_VALUE, "test", 0, "previous failure");
    CHECK(xcron_matches(rule, TEXT_SIZE(rule), 0) && xrtGetError() == NULL);
    xrtSetErrorInfo(XERR_VALUE, "test", 0, "previous failure");
    CHECK(xcron_next(rule, TEXT_SIZE(rule), 0) == 60 && xrtGetError() == NULL);
    xrtSetErrorInfo(XERR_VALUE, "test", 0, "previous failure");
    description = xcron_describe(rule, TEXT_SIZE(rule));
    CHECK(description != NULL && xrtGetError() == NULL);
    xrtFree(description);
    return 0;
}

static int test_description_allocation(void) {
    const char valid[] = "* * * * *";
    const char invalid[] = "bad";
    const char *inputs[] = {valid, invalid};
    size_t i;
    for (i = 0; i < sizeof(inputs) / sizeof(inputs[0]); ++i) {
        const char *text = inputs[i];
        void *result;
        CHECK(xrtMemDebugFailAfter(0u));
        CHECK(xcron_describe(text, strlen(text)) == NULL &&
            xrtMemDebugFailTriggered() && xrtGetError() != NULL &&
            xrtErrorKind(xrtGetError()) == XERR_MEMORY);
        xrtMemDebugFailClear();
        result = xcron_describe(text, strlen(text));
        CHECK(result != NULL && xrtGetError() == NULL);
        CHECK(strcmp((const char *)result, i == 0 ? "every minute" : "invalid") == 0);
        xrtFree(result);
    }
    return 0;
}

int main(void) {
    if (test_parser() || test_calendar() || test_next_reference() || test_description() ||
        test_error_recovery() || test_description_allocation()) return 1;
    puts("crontab C: PASS");
    return 0;
}
