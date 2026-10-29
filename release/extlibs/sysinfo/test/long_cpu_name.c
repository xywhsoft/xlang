#include "xrt.h"
#include "../sysinfo_native.h"

#include <stdio.h>
#include <string.h>

static int cpu_calls;

FILE *__real_fopen(const char *path, const char *mode);

FILE *__wrap_fopen(const char *path, const char *mode)
{
    static char long_model[1200];
    static char misleading_line[640];
    static char misleading_key[] = "model name extra : bogus\nmodel name : actual\n";
    static char blank_model[] = "model name : \t  \n";
    size_t length;
    if (strcmp(path, "/proc/cpuinfo") != 0) return __real_fopen(path, mode);

    if (cpu_calls++ == 0) {
        const char *prefix = "model name : ";
        length = strlen(prefix);
        memcpy(long_model, prefix, length);
        memset(long_model + length, 'A', 1100);
        length += 1100;
        long_model[length++] = '\n';
        return fmemopen(long_model, length, mode);
    }

    /* 第一行跨越缓冲边界；中途的字段样式不能冒充下一行。 */
    if (cpu_calls == 3) {
        return fmemopen(misleading_key, sizeof(misleading_key) - 1u, mode);
    }
    if (cpu_calls == 4) {
        return fmemopen(blank_model, sizeof(blank_model) - 1u, mode);
    }
    memset(misleading_line, 'x', 511);
    length = 511;
    memcpy(misleading_line + length, "model name : wrong\nmodel name : actual\n",
        sizeof("model name : wrong\nmodel name : actual\n") - 1u);
    length += sizeof("model name : wrong\nmodel name : actual\n") - 1u;
    return fmemopen(misleading_line, length, mode);
}

int main(void)
{
    char *long_name = xsysinfo_cpu_name();
    char *actual_name = xsysinfo_cpu_name();
    char *after_bad_key = xsysinfo_cpu_name();
    char *blank_name = xsysinfo_cpu_name();
    int ok = long_name && strlen(long_name) == 1100 &&
        strspn(long_name, "A") == 1100 && actual_name &&
        strcmp(actual_name, "actual") == 0 && after_bad_key &&
        strcmp(after_bad_key, "actual") == 0 && blank_name &&
        strcmp(blank_name, "unknown") == 0 && cpu_calls == 4;
    if (!ok) fprintf(stderr, "sysinfo long CPU model/line boundary failed\n");
    xrtFree(long_name);
    xrtFree(actual_name);
    xrtFree(after_bad_key);
    xrtFree(blank_name);
    return ok ? 0 : 1;
}
