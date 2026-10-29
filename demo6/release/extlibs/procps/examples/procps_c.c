#define XRT_MODULE_VALUE_CONTAINER
#include "xrt.h"
#include "../include/procps_native.h"
#ifdef __TINYC__
/* 教学范例单文件运行；正式包仍由清单单独编译原生实现。 */
#include "../procps_native.c"
#endif
#include <stdio.h>

int main(void)
{
    int64_t self = xprocps_pid();
    int64_t parent = xprocps_ppid();
    int64_t count = xprocps_count();
    xvalue* pids = xprocps_pids();
    char* name = xprocps_name(self);
    size_t i;
    int found = 0;
    if (pids == NULL || name == NULL || parent < 0 || count <= 0) {
        xrtValueRelease(pids);
        xrtFree(name);
        return 1;
    }
    for (i = 0; i < xrtValueCount(pids); ++i) {
        int64_t pid = 0;
        if (xrtValueGetInt(xrtValueArrayGet(pids, i), &pid) && pid == self)
            found = 1;
    }
    printf("self exists: %s\n", xprocps_exists(self) ? "true" : "false");
    printf("name known: %s\n", *name ? "true" : "false");
    printf("self listed: %s\n", found ? "true" : "false");
    printf("rss positive: %s\n", xprocps_mem_kb(self) > 0 ? "true" : "false");
    /* PID 1 的父进程可以为 0；不同查询时刻的 count 不要求与数组长度相等。 */
    printf("parent query: %s\n", parent >= 0 ? "true" : "false");
    printf("count positive: %s\n", count > 0 ? "true" : "false");
    xrtValueRelease(pids);
    xrtFree(name);
    return 0;
}
