#ifndef XL_SYSINFO_NATIVE_H
#define XL_SYSINFO_NATIVE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 字符串结果用 XRT 分配，由 C 调用方用 xrtFree 释放。 */
char *xsysinfo_os_name(void);
char *xsysinfo_os_version(void);
char *xsysinfo_arch(void);
char *xsysinfo_hostname(void);
char *xsysinfo_cpu_name(void);
int64_t xsysinfo_cpu_count(void);
int64_t xsysinfo_mem_total(void);
int64_t xsysinfo_mem_available(void);
int64_t xsysinfo_mem_usage_pct(void);
int64_t xsysinfo_page_size(void);
int64_t xsysinfo_uptime(void);

#ifdef __cplusplus
}
#endif
#endif
