#define XRT_MODULE_TIME
#define XRT_MODULE_TIME_LOCAL
#include "xrt.h"
#include "crontab_native.h"

#include <stdio.h>

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

int main(void) {
    const char rule[] = "0 12 29 2 *";
    int64_t from = local_noon(2096, 3, 1);
    int64_t expected = local_noon(2104, 2, 29);
    if (from < 0 || expected < 0 ||
        xcron_next(rule, sizeof(rule) - 1, from) != expected) {
        fputs("crontab long-horizon test failed\n", stderr);
        return 1;
    }
    puts("crontab long horizon: PASS");
    return 0;
}
