#include "xrt.h"
#include "../sysinfo_native.h"

#include <stdio.h>
#include <string.h>
#include <sys/utsname.h>
#include <unistd.h>

/* 模拟内核返回非 UTF-8 字节，不能让它们直接进入 XLang string。 */
static int host_calls;
static int cpu_calls;

static int fake_hostname(char *name, size_t length)
{
    host_calls++;
    if (length < 2u) return -1;
    name[0] = (char)0xff;
    name[1] = '\0';
    return 0;
}

int __wrap_gethostname(char *name, size_t length)
{
    return fake_hostname(name, length);
}

/* 启用 _FORTIFY_SOURCE 时，glibc 会改为调用这个带容量检查的符号。 */
int __wrap___gethostname_chk(char *name, size_t length, size_t object_size)
{
    (void)object_size;
    return fake_hostname(name, length);
}

int __wrap_uname(struct utsname *info)
{
    memset(info, 0, sizeof(*info));
    info->sysname[0] = (char)0xff;
    info->release[0] = (char)0xfe;
    info->machine[0] = (char)0xfd;
    return 0;
}

FILE *__real_fopen(const char *path, const char *mode);

FILE *__wrap_fopen(const char *path, const char *mode)
{
    static char cpu_info[] = "model name : \xff\n";
    if (strcmp(path, "/proc/cpuinfo") != 0) return __real_fopen(path, mode);
    cpu_calls++;
    return fmemopen(cpu_info, sizeof(cpu_info) - 1u, mode);
}

int main(void)
{
    char *host = xsysinfo_hostname();
    char *os = xsysinfo_os_name();
    char *version = xsysinfo_os_version();
    char *arch = xsysinfo_arch();
    char *cpu = xsysinfo_cpu_name();
    int ok = host_calls > 0 && cpu_calls > 0 && host && os && version && arch && cpu &&
        strcmp(host, "unknown") == 0 && strcmp(os, "unknown") == 0 &&
        strcmp(version, "unknown") == 0 && strcmp(arch, "unknown") == 0 &&
        strcmp(cpu, "unknown") == 0;
    if (!ok) {
        fprintf(stderr, "sysinfo non-UTF-8 fallback: host_calls=%d cpu_calls=%d host=%d os=%d version=%d arch=%d cpu=%d\n",
            host_calls, cpu_calls,
            host && strcmp(host, "unknown") == 0,
            os && strcmp(os, "unknown") == 0,
            version && strcmp(version, "unknown") == 0,
            arch && strcmp(arch, "unknown") == 0,
            cpu && strcmp(cpu, "unknown") == 0);
    }
    xrtFree(host);
    xrtFree(os);
    xrtFree(version);
    xrtFree(arch);
    xrtFree(cpu);
    return ok ? 0 : 1;
}
