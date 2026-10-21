/* 原生实现先设置平台特性宏，再包含系统头；xl run 可直接编译本文件。 */
#include "../sysinfo_native.c"
#include <stdio.h>

int main(void)
{
    char *os = xsysinfo_os_name();
    char *version = xsysinfo_os_version();
    char *arch = xsysinfo_arch();
    char *host = xsysinfo_hostname();
    if (!os || !version || !arch || !host) {
        xrtFree(os); xrtFree(version); xrtFree(arch); xrtFree(host);
        return 1;
    }
    printf("os: %s\nversion: %s\narch: %s\nhost: %s\n", os, version, arch, host);
    xrtFree(os); xrtFree(version); xrtFree(arch); xrtFree(host);
    return 0;
}
