#ifndef PROCPS_NATIVE_H
#define PROCPS_NATIVE_H

#include <stdint.h>
#include "xrt.h"

typedef struct xvalue xvalue;
#ifdef __cplusplus
extern "C" {
#endif

int64_t xprocps_pid(void);
int64_t xprocps_ppid(void);
int xprocps_exists(int64_t pid);
/* 返回 XRT xvalue 数组，调用方使用 xrtValueRelease。 */
xvalue* xprocps_pids(void);
int64_t xprocps_count(void);
/* 返回由 xrtFree 释放的 UTF-8 独立字符串；不可访问时返回 NULL。 */
char* xprocps_name(int64_t pid);
/* 当前常驻内存 KiB；不可访问时返回 -1。 */
int64_t xprocps_mem_kb(int64_t pid);

#ifdef __cplusplus
}
#endif
#endif
