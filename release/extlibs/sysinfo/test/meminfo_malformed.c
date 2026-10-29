#include "../sysinfo_native.h"
#include <stdio.h>
#include <string.h>

static char input[128];

/* 只替换本进程的 /proc/meminfo，不修改实际系统文件。 */
FILE *__wrap_fopen(const char *path, const char *mode)
{
    (void)mode;
    if (strcmp(path, "/proc/meminfo") != 0) return NULL;
    return fmemopen(input, strlen(input), "r");
}

static int check(const char *text, int64_t expected)
{
    strcpy(input, text);
    if (xsysinfo_mem_available() != expected) {
        fprintf(stderr, "unexpected MemAvailable for: %s", text);
        return 0;
    }
    return 1;
}

int main(void)
{
    if (!check("MemAvailable: 123 kB\n", 123 * 1024)) return 1;
    if (!check("MemAvailable: -1 kB\n", 0)) return 2;
    if (!check("MemAvailable: 123junk kB\n", 0)) return 3;
    if (!check("MemAvailable: 18446744073709551616 kB\n", 0)) return 4;
    return 0;
}
