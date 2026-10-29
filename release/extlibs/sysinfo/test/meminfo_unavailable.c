#include "../sysinfo_native.h"
#include <stdio.h>

/* 只在本测试进程内使 /proc/meminfo 不可读，不改生产路径。 */
FILE *__wrap_fopen(const char *path, const char *mode)
{
    (void)path;
    (void)mode;
    return NULL;
}

int main(void)
{
    if (xsysinfo_mem_total() <= 0) return 1;
    if (xsysinfo_mem_available() != 0) return 2;
    if (xsysinfo_mem_usage_pct() != 0) {
        fputs("memUsagePct must be 0 when /proc/meminfo is unavailable\n", stderr);
        return 3;
    }
    return 0;
}
