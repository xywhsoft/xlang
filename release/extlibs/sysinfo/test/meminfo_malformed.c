#include "../sysinfo_native.h"
#include "xrt.h"
#include <stdio.h>
#include <string.h>

static const char *input;
static unsigned cases, failures;

/* 只替换本进程的 /proc/meminfo，不修改实际系统文件。 */
FILE *__wrap_fopen(const char *path, const char *mode)
{
    (void)mode;
    if (strcmp(path, "/proc/meminfo") != 0) return NULL;
    return fmemopen((void *)input, strlen(input), "r");
}

static void check(const char *text, int64_t expected, bool valid)
{
    input = text;
    xrtSetErrorInfo(XERR_VALUE, "sysinfo.test", 0, "stale error");
    int64_t available = xsysinfo_mem_available();
    if (available != expected || xrtGetError()) {
        fprintf(stderr, "MemAvailable case %u: expected=%lld actual=%lld\n",
            cases, (long long)expected, (long long)available);
        ++failures;
    }
    xrtSetErrorInfo(XERR_VALUE, "sysinfo.test", 0, "stale error");
    int64_t usage = xsysinfo_mem_usage_pct();
    if (usage < 0 || usage > 100 || (!valid && usage != 0) || xrtGetError()) {
        fprintf(stderr, "MemAvailable case %u: invalid usage=%lld\n", cases, (long long)usage);
        ++failures;
    }
    ++cases;
}

int main(void)
{
    check("MemAvailable: 123 kB\n", 123 * 1024, true);
    check("MemAvailable:\t123\tkB\r\n", 123 * 1024, true);
    check("MemAvailable: 123 kB", 123 * 1024, true);
    check("MemAvailable: 0 kB\n", 0, true);
    check("MemAvailable: -1 kB\n", 0, false);
    check("MemAvailable: 123junk kB\n", 0, false);
    check("MemAvailable: 123_456 kB\n", 0, false);
    check("MemAvailable: 18446744073709551616 kB\n", 0, false);
    check("MemAvailable: 9223372036854775808 kB\n", 0, false);
    check("MemAvailable: 9007199254740991 kB\n", INT64_C(9223372036854774784), true);
    check("MemAvailable: 9223372036854775807 kB\n", INT64_MAX, true);
    check("MemAvailable: 123 kB garbage\n", 0, false);
    check("MemAvailableExtra: 123 kB\n", 0, false);
    /* fgets 的每个续块都不能冒充新的系统字段。 */
    for (size_t chunks = 1; chunks <= 3; ++chunks) {
        char data[1024];
        size_t length = 127u * chunks;
        memset(data, 'x', length);
        strcpy(data + length, "MemAvailable: 123 kB\nMemAvailable: 456 kB\n");
        check(data, 456 * 1024, true);
        strcpy(data + length, "MemAvailable: 123 kB\n");
        check(data, 0, false);
    }
    /* 前127字节恰好看似完整，后面的垃圾不能因缓冲截断而消失。 */
    {
        char data[256];
        memcpy(data, "MemAvailable:", 13);
        memset(data + 13, ' ', 108);
        strcpy(data + 121, "123 kB");
        check(data, 123 * 1024, true);
        strcpy(data + 127, "\n");
        check(data, 123 * 1024, true);
        strcpy(data + 127, "garbage\n");
        check(data, 0, false);
        strcat(data, "MemAvailable: 456 kB\n");
        check(data, 0, false);
    }
    printf("sysinfo meminfo records: %u cases, %u failures\n", cases, failures);
    return failures ? 1 : 0;
}
