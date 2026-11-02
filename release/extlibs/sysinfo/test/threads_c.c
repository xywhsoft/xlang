/* 真实系统查询：结果独立拥有，线程错误不能污染其他查询。 */
#define XRT_MODULE_UNICODE
#define XRT_MODULE_NUMBER_INTEGER
#define XRT_MODULE_THREAD
#define XRT_MODULE_MEMORY_DEBUG
#include "xrt.h"
#include "../sysinfo_native.h"
#ifdef __TINYC__
#include "../sysinfo_native.c"
#endif
#include <stdio.h>
#include <string.h>

static char* (*const text_queries[])(void) = {
    xsysinfo_os_name, xsysinfo_os_version, xsysinfo_arch,
    xsysinfo_hostname, xsysinfo_cpu_name
};
static int64_t (*const number_queries[])(void) = {
    xsysinfo_cpu_count, xsysinfo_mem_total, xsysinfo_mem_available,
    xsysinfo_mem_usage_pct, xsysinfo_page_size, xsysinfo_uptime
};

static int run(void* context)
{
    (void)context;
    for (unsigned round = 0; round < 64; ++round) {
        char* results[5] = {0};
        bool valid = true;
        for (unsigned i = 0; i < 5; ++i) {
            xrtSetErrorInfo(XERR_IO, "sysinfo.thread.old", round, "old diagnostic");
            results[i] = text_queries[i]();
            if (!results[i] || !*results[i] || xrtGetError() ||
                !xrtUtf8Valid((xstrview){results[i], strlen(results[i])}, NULL)) valid = false;
        }
        /* 前面的结果仍活着时，后一次查询不能复用其返回缓冲。 */
        for (unsigned i = 0; i < 5; ++i) {
            for (unsigned j = 0; j < i; ++j)
                if (results[i] && results[i] == results[j]) valid = false;
        }
        for (unsigned i = 0; i < 5; ++i) xrtFree(results[i]);
        if (!valid) return 1;
        for (unsigned i = 0; i < 6; ++i) {
            xrtSetErrorInfo(XERR_IO, "sysinfo.thread.old", round, "old diagnostic");
            int64_t value = number_queries[i]();
            if (value < 0 || xrtGetError() || (i == 3 && value > 100)) return 2;
        }
    }
    return 0;
}

int main(int argc, char** argv)
{
    (void)argv;
    if (argc != 1 || !xrtMemDebugEnabled()) return 2;
    xthread* threads[4] = {0};
    xmemdebugsnapshot before, after;
    xrtSetErrorInfo(XERR_IO, "sysinfo.main.old", 0, "warm error slot");
    xrtClearError();
    xrtMemDebugSnapshot(&before);
    bool valid = true;
    for (unsigned i = 0; i < 4; ++i) {
        threads[i] = xrtThreadCreate(run, NULL, 0);
        if (!threads[i]) valid = false;
    }
    for (unsigned i = 0; i < 4; ++i) {
        if (!threads[i]) continue;
        if (xrtThreadWait(threads[i]) != XWAIT_OK || xrtThreadExitCode(threads[i]) != 0)
            valid = false;
        xrtThreadDestroy(threads[i]);
    }
    xrtClearError();
    xrtMemDebugSnapshot(&after);
    valid = valid && before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes &&
        before.InvalidFreeCount == after.InvalidFreeCount &&
        before.DoubleFreeCount == after.DoubleFreeCount &&
        before.OverflowCount == after.OverflowCount &&
        before.UnderflowCount == after.UnderflowCount &&
        before.UseAfterFreeCount == after.UseAfterFreeCount;
    printf("sysinfo threads: 4 workers, 64 rounds, 2816 queries, %s\n", valid ? "PASS" : "FAIL");
    return valid ? 0 : 1;
}
