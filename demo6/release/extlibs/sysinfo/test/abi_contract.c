#include <stdint.h>
#include "../sysinfo_native.h"

/* 六个整数入口必须与 XLang int 的 int64_t ABI 一致。 */
extern int64_t xsysinfo_cpu_count(void);
extern int64_t xsysinfo_mem_total(void);
extern int64_t xsysinfo_mem_available(void);
extern int64_t xsysinfo_mem_usage_pct(void);
extern int64_t xsysinfo_page_size(void);
extern int64_t xsysinfo_uptime(void);
