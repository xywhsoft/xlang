#define XRT_MODULE_TIME
#define XRT_MODULE_TIME_LOCAL
#define XRT_MODULE_MEMORY_DEBUG
#include "xrt.h"
#include "../include/crontab_native.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static unsigned checks, failures;
#define CHECK(expr) do { ++checks; if (!(expr)) { ++failures; \
    fprintf(stderr, "crontab input line=%d: %s\n", __LINE__, #expr); } } while (0)

static void invalid(const char* text, size_t size)
{
    // 无效表达式是正常查询结果；每条公开路径都不能残留旧错误。
    xrtSetErrorInfo(XERR_IO, "cron.test", 0, "previous");
    CHECK(!xcron_is_valid(text, size) && !xrtGetError());
    xrtSetErrorInfo(XERR_IO, "cron.test", 0, "previous");
    CHECK(!xcron_matches(text, size, 0) && !xrtGetError());
    xrtSetErrorInfo(XERR_IO, "cron.test", 0, "previous");
    CHECK(xcron_next(text, size, 0) == -1 && !xrtGetError());
    xrtSetErrorInfo(XERR_IO, "cron.test", 0, "previous");
    char* description = xcron_describe(text, size);
    CHECK(description && !strcmp(description, "invalid") && !xrtGetError());
    xrtFree(description);
}

int main(void)
{
    static const char* bad[] = {
        "0 0 1 1 -1", "0 0 1 1 8", "*/2147483648 * * * *", "*/0 * * * *",
        "0/ * * * *", "0-59/ * * * *", "0-59//2 * * * *", "0-59/2/2 * * * *",
        "0-60 * * * *", "0 * * * * extra", "0 * * * *\r", "0 * * * *\n",
        "0 * * * *\v", "0 * * * *\f", "0 * * * *\x7f", "+0 * * * *",
        "0x1 * * * *", "0.5 * * * *", "0, * * * *", ",0 * * * *", "0,,1 * * * *"
    };
    static const char* valid[] = {
        "\t *\t* * * *  ", "0/2147483647 * * * *", "0-59/2147483647 * * * *",
        "* * * * 0,7", "00000 00 01 01 00", "0 0 31 2 *"
    };
    static const char summary[] = "minute=*/5; hour=*; day=*; month=*; weekday=*";
    const char* descriptions[] = {"* * * * *", "bad", "*/5 * * * *"};
    const char* expected[] = {"every minute", "invalid", summary};
    const char exact[9] = {'*',' ','*',' ','*',' ','*',' ','*'};
    const char trailing[] = "* * * * *\0unselected bytes";
    char text[1025];
    xmemdebugsnapshot before, after;
    // 预热错误槽，快照只衡量受测操作是否回收，不把首次诊断初始化当泄漏。
    xrtSetErrorInfo(XERR_IO, "cron.test", 0, "warm");
    xrtClearError();
    xrtMemDebugSnapshot(&before);
    invalid(NULL, 0);
    invalid(NULL, SIZE_MAX);
    invalid(exact, 0);
    invalid(exact, SIZE_MAX);
    for (size_t i = 0; i < sizeof(bad)/sizeof(*bad); ++i) invalid(bad[i], strlen(bad[i]));
    for (size_t i = 0; i < sizeof(valid)/sizeof(*valid); ++i)
        CHECK(xcron_is_valid(valid[i], strlen(valid[i])) && !xrtGetError());
    CHECK(xcron_is_valid(exact, sizeof(exact)));
    CHECK(xcron_is_valid(trailing, sizeof(exact)));
    invalid(trailing, sizeof(trailing)-1);
    // 刚好1024字节合法，1025字节拒绝；不要求源缓冲有额外NUL哨兵。
    memset(text, '0', sizeof(text));
    memcpy(text+1016, " * * * *", 8);
    CHECK(xcron_is_valid(text, 1024));
    char* full = xcron_describe(text, 1024);
    CHECK(full && !strncmp(full, "minute=", 7) &&
        strlen(full) == 1016+strlen("minute=; hour=*; day=*; month=*; weekday=*"));
    xrtFree(full);
    invalid(text, sizeof(text));
    // 逐个位置注入NUL，验证有长度入口不会按前缀误接受。
    for (size_t i = 0; i < sizeof(exact); ++i) {
        char embedded[sizeof(exact)];
        memcpy(embedded, exact, sizeof(exact));
        embedded[i] = 0;
        invalid(embedded, sizeof(embedded));
    }
    // 三个分配分支：通配简写、无效描述、一般字段摘要。
    for (size_t i = 0; i < 3; ++i) {
        for (uint64_t budget = 0; budget <= 1; ++budget) {
            CHECK(xrtMemDebugFailAfter(budget));
            char* result = xcron_describe(descriptions[i], strlen(descriptions[i]));
            bool injected = xrtMemDebugFailTriggered();
            xrtMemDebugFailClear();
            CHECK(budget ? result && !injected && !xrtGetError() && !strcmp(result, expected[i]) :
                !result && injected && xrtErrorKind(xrtGetError()) == XERR_MEMORY);
            xrtFree(result);
            xrtClearError();
        }
    }
    xrtMemDebugSnapshot(&after);
    CHECK(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
    CHECK(before.InvalidFreeCount == after.InvalidFreeCount && before.DoubleFreeCount == after.DoubleFreeCount);
    CHECK(before.OverflowCount == after.OverflowCount && before.UnderflowCount == after.UnderflowCount);
    printf("crontab input boundaries: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}

#ifdef __TINYC__
#include "../crontab_native.c"
#endif
