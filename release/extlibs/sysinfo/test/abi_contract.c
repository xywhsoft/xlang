#include <stdint.h>
#include "../sysinfo_native.h"

/* 五个拥有型文本入口与 XLang @owned 的 char* ABI 一致。 */
extern char *xsysinfo_os_name(void);
extern char *xsysinfo_os_version(void);
extern char *xsysinfo_arch(void);
extern char *xsysinfo_hostname(void);
extern char *xsysinfo_cpu_name(void);

/* 六个整数入口必须与 XLang int 的 int64_t ABI 一致。 */
extern int64_t xsysinfo_cpu_count(void);
extern int64_t xsysinfo_mem_total(void);
extern int64_t xsysinfo_mem_available(void);
extern int64_t xsysinfo_mem_usage_pct(void);
extern int64_t xsysinfo_page_size(void);
extern int64_t xsysinfo_uptime(void);
