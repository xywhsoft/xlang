#include "crontab_native.h"

#include <stdio.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "crontab half-hour DST test failed at %d: %s\n", __LINE__, #expr); \
    return 1; \
} } while (0)
#define RULE_SIZE(rule) (sizeof(rule) - 1u)

int main(void) {
    const char repeated[] = "45 1 * * *";
    const char skipped[] = "15 2 * * *";
    const char first_after_jump[] = "30 2 * * *";

    /* Australia/Lord_Howe 在 2024-04-07 回拨半小时，01:45 依次出现两次。 */
    CHECK(xcron_next(repeated, RULE_SIZE(repeated), 1712414640) == 1712414700);
    CHECK(xcron_next(repeated, RULE_SIZE(repeated), 1712414700) == 1712416500);
    CHECK(xcron_matches(repeated, RULE_SIZE(repeated), 1712414700));
    CHECK(xcron_matches(repeated, RULE_SIZE(repeated), 1712416500));

    /* 2024-10-06 从 01:59 跳到 02:30，02:15 要等到次日。 */
    CHECK(xcron_next(first_after_jump, RULE_SIZE(first_after_jump),
        1728142140) == 1728142200);
    CHECK(!xcron_matches(skipped, RULE_SIZE(skipped), 1728142200));
    CHECK(xcron_next(skipped, RULE_SIZE(skipped), 1728142140) == 1728227700);
    puts("crontab half-hour DST: PASS");
    return 0;
}
