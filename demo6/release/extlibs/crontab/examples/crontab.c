#include "../include/crontab_native.h"
#include "xrt.h"

#include <stdio.h>

int main(void) {
    const int64_t base = 1700000000;
    const char everyMinute[] = "* * * * *";
    const char onHour[] = "0 * * * *";
    const char quarterHour[] = "*/15 * * * *";
    const char fiveMinute[] = "*/5 * * * *";
    const char steppedDay[] = "0 9 */2 * 1";
    const char invalid[] = "bad";
    int64_t quarter = xcron_next(quarterHour, sizeof(quarterHour) - 1, base);
    void *description = xcron_describe(fiveMinute, sizeof(fiveMinute) - 1);
    if (description == NULL) return 1;
    if (quarter < 0 || !xcron_matches(quarterHour, sizeof(quarterHour) - 1, quarter)) {
        xrtFree(description);
        return 1;
    }
    printf("valid '* * * * *': %s\n",
        xcron_is_valid(everyMinute, sizeof(everyMinute) - 1) ? "true" : "false");
    printf("valid 'bad': %s\n",
        xcron_is_valid(invalid, sizeof(invalid) - 1) ? "true" : "false");
    printf("next '* * * * *' from %lld : %lld\n", (long long)base,
        (long long)xcron_next(everyMinute, sizeof(everyMinute) - 1, base));
    printf("next '0 * * * *' from %lld : %lld\n", (long long)base,
        (long long)xcron_next(onHour, sizeof(onHour) - 1, base));
    printf("next '*/15 * * * *' from %lld : %lld\n", (long long)base,
        (long long)quarter);
    puts("matches '*/15 * * * *' at next: true");
    printf("next '0 9 */2 * 1' from %lld : %lld\n", (long long)base,
        (long long)xcron_next(steppedDay, sizeof(steppedDay) - 1, base));
    printf("describe '*/5 * * * *': %s\n", (const char *)description);
    xrtFree(description);
    puts("crontab example: PASS");
    return 0;
}

/* TCC 单文件范例编入实现；正式包仍按清单单独编译。 */
#ifdef __TINYC__
#include "../crontab_native.c"
#endif
