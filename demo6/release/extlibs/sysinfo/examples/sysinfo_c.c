/* 原生实现先设置平台特性宏，再包含系统头；xl run 可直接编译本文件。 */
#include "../sysinfo_native.c"
#include <inttypes.h>
#include <stdio.h>

int main(void)
{
    char *os = xsysinfo_os_name();
    char *version = xsysinfo_os_version();
    char *arch = xsysinfo_arch();
    char *host = xsysinfo_hostname();
    char *cpu = xsysinfo_cpu_name();
    if (!os || !version || !arch || !host || !cpu) {
        xrtFree(os); xrtFree(version); xrtFree(arch); xrtFree(host); xrtFree(cpu);
        return 1;
    }
    printf("os: %s\nversion: %s\narch: %s\nhost: %s\n"
           "cpu: %s\ncpus: %" PRId64 "\nmemory: %" PRId64 "\npage: %" PRId64 "\n",
           os, version, arch, host, cpu, xsysinfo_cpu_count(),
           xsysinfo_mem_total(), xsysinfo_page_size());
    xrtFree(os); xrtFree(version); xrtFree(arch); xrtFree(host); xrtFree(cpu);
    return 0;
}
