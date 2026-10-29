#if defined(_WIN32) && !defined(_WIN32_WINNT)
#define _WIN32_WINNT 0x0601
#endif
#define XRT_MODULE_MEMORY_DEBUG
#include "xrt.h"
#include "../sysinfo_native.h"
#ifdef _WIN32
#include <windows.h>
#endif
#include <stdio.h>
#include <string.h>

#ifndef _WIN32
static long long linux_mem_available(void)
{
    FILE *file = fopen("/proc/meminfo", "r");
    char line[128];
    unsigned long long kib;
    if (!file) return -1;
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "MemAvailable:", 13) == 0 &&
            sscanf(line + 13, "%llu", &kib) == 1) {
            fclose(file);
            return kib <= 0x7fffffffffffffffULL / 1024u ?
                (long long)(kib * 1024u) : -1;
        }
    }
    fclose(file);
    return -1;
}
#endif

#define CHECK(test) do { if (!(test)) { \
    fprintf(stderr, "sysinfo test failed at line %d: %s\n", __LINE__, #test); \
    return 1; \
} } while (0)

static int check_text_error_reset(char* (*query)(void))
{
    void* result;
    int clean;
    xrtSetErrorInfo(XERR_VALUE, "sysinfo.test", 0, "stale error");
    result = query();
    clean = result != NULL && xrtGetError() == NULL;
    xrtFree(result);
    return clean;
}

static int check_number_error_reset(int64_t (*query)(void))
{
    xrtSetErrorInfo(XERR_VALUE, "sysinfo.test", 0, "stale error");
    (void)query();
    return xrtGetError() == NULL;
}

int main(void)
{
    char *os = xsysinfo_os_name();
    char *version = xsysinfo_os_version();
    char *arch = xsysinfo_arch();
    char *host = xsysinfo_hostname();
    char *cpu = xsysinfo_cpu_name();
    long long total = xsysinfo_mem_total();
#ifndef _WIN32
    long long meminfo_before = linux_mem_available();
#endif
    long long available = xsysinfo_mem_available();
#ifndef _WIN32
    long long meminfo_after = linux_mem_available();
    long long tolerance = total / 100;
#endif

    CHECK(os && version && arch && host && cpu);
    CHECK(*os && *version && *arch && *host && *cpu);
    CHECK(xsysinfo_cpu_count() >= 1);
#ifdef _WIN32
    CHECK(xsysinfo_cpu_count() ==
        (long long)GetActiveProcessorCount(ALL_PROCESSOR_GROUPS));
#endif
    CHECK(total > 0 && available >= 0 && available <= total);
#ifndef _WIN32
    /* 两次系统快照夹住本次调用，只为短时内存变化留 1% 余量。 */
    CHECK(meminfo_before > 0 && meminfo_after > 0);
    CHECK(available >= (meminfo_before < meminfo_after ? meminfo_before : meminfo_after) - tolerance);
    CHECK(available <= (meminfo_before > meminfo_after ? meminfo_before : meminfo_after) + tolerance);
#endif
    CHECK(xsysinfo_mem_usage_pct() >= 0 && xsysinfo_mem_usage_pct() <= 100);
    CHECK(xsysinfo_page_size() >= 4096);
    CHECK(xsysinfo_uptime() >= 0);
    /* 独立结果不可共用临时缓冲区，也不能指向栈上 uname 结构。 */
    CHECK(os != version && os != arch && arch != host && host != cpu);
    CHECK(strlen(os) > 0 && strlen(arch) > 0);

    CHECK(check_text_error_reset(xsysinfo_os_name));
    CHECK(check_text_error_reset(xsysinfo_os_version));
    CHECK(check_text_error_reset(xsysinfo_arch));
    CHECK(check_text_error_reset(xsysinfo_hostname));
    CHECK(check_text_error_reset(xsysinfo_cpu_name));
    CHECK(check_number_error_reset(xsysinfo_cpu_count));
    CHECK(check_number_error_reset(xsysinfo_mem_total));
    CHECK(check_number_error_reset(xsysinfo_mem_available));
    CHECK(check_number_error_reset(xsysinfo_mem_usage_pct));
    CHECK(check_number_error_reset(xsysinfo_page_size));
    CHECK(check_number_error_reset(xsysinfo_uptime));

    /* 每个文本查询都要区分内存不足与探测不可用，且清除故障后可重试。 */
    {
        char* (*queries[])(void) = {
            xsysinfo_os_name, xsysinfo_os_version, xsysinfo_arch,
            xsysinfo_hostname, xsysinfo_cpu_name
        };
        for (size_t query = 0; query < sizeof(queries) / sizeof(queries[0]); ++query) {
            unsigned failures = 0u, successes = 0u;
            for (uint64 point = 0u; point < 64u; ++point) {
                void* text;
                bool triggered;
                CHECK(xrtMemDebugFailAfter(point));
                text = queries[query]();
                triggered = xrtMemDebugFailTriggered();
                xrtMemDebugFailClear();
                if (text != NULL) {
                    CHECK(((char*)text)[0] != '\0' && xrtGetError() == NULL);
                    ++successes;
                } else {
                    CHECK(triggered && xrtGetError() != NULL &&
                        xrtErrorKind(xrtGetError()) == XERR_MEMORY);
                    ++failures;
                }
                xrtFree(text);
                text = queries[query]();
                CHECK(text != NULL && ((char*)text)[0] != '\0' && xrtGetError() == NULL);
                xrtFree(text);
            }
            CHECK(failures > 0u && successes > 0u);
        }
    }

    xrtFree(os);
    xrtFree(version);
    xrtFree(arch);
    xrtFree(host);
    xrtFree(cpu);
    return 0;
}
