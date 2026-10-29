#include "../sysinfo_native.h"
#include <stdio.h>
#include <string.h>

static const char *uptime_text;

/* 测试进程内替换 /proc/uptime，不接触真实系统文件。 */
FILE *__wrap_fopen(const char *path, const char *mode)
{
    if (strcmp(path, "/proc/uptime") != 0) return NULL;
    (void)mode;
    return fmemopen((void *)uptime_text, strlen(uptime_text), "r");
}

static int check(const char *text, int64_t expected)
{
    uptime_text = text;
    if (xsysinfo_uptime() != expected) {
        fprintf(stderr, "unexpected uptime for: %s\n", text);
        return 0;
    }
    return 1;
}

int main(void)
{
    if (!check("123.75 0.00", 123)) return 1;
    if (!check("-1.00 0.00", 0)) return 2;
    if (!check("9223372036854775808.00 0.00", 0)) return 3;
    if (!check("1e309 0.00", 0)) return 4;
    return 0;
}
