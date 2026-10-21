#include "crontab_native.h"

#include <stdio.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "crontab DST test failed at %d: %s\n", __LINE__, #expr); \
    return 1; \
} } while (0)
#define RULE_SIZE(rule) (sizeof(rule) - 1u)

int main(void) {
    const char spring[] = "30 2 * * *";
    const char autumn[] = "30 1 * * *";
    const char midnight[] = "0 0 * * *";
    const char noon[] = "0 12 * * *";

    /* America/New_York: 2024-03-10 的 02:30 不存在。 */
    CHECK(xcron_next(spring, RULE_SIZE(spring), 1710053940) == 1710138600);
    /* 2024-11-03 的 01:30 在回拨前后各出现一次。 */
    CHECK(xcron_next(autumn, RULE_SIZE(autumn), 1730609940) == 1730611800);
    CHECK(xcron_next(autumn, RULE_SIZE(autumn), 1730611800) == 1730615400);
    CHECK(xcron_matches(autumn, RULE_SIZE(autumn), 1730611800));
    CHECK(xcron_matches(autumn, RULE_SIZE(autumn), 1730615400));

    /* 1880 年纽约时差含秒数，本地整分钟不一定是 Unix 分钟边界。 */
    CHECK(xcron_next(midnight, RULE_SIZE(midnight), INT64_C(-2840123039)) ==
        INT64_C(-2840123038));
    /* 1883-11-18 纽约由 LMT 切至 EST，12:00:00 落在原步长之前。 */
    CHECK(xcron_next(noon, RULE_SIZE(noon), INT64_C(-2717650858)) ==
        INT64_C(-2717650800));
    puts("crontab DST: PASS");
    return 0;
}
