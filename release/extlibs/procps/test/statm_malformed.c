#define XRT_MODULE_FILE_WHOLE
#include "xrt.h"
#include "../include/procps_native.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static const char *input;
static size_t input_size;
static xerrkind read_error;

/* 只替换本测试进程的 statm 读取，不访问实际进程文件。 */
bytes __wrap_xrtFileReadAllLimit(cstr path, size_t limit, size_t *size)
{
    size_t length;
    bytes copy;
    (void)path;
    if (read_error != XERR_NONE) {
        xrtSetErrorInfo(read_error, "procps.test", 0, "injected statm read error");
        return NULL;
    }
    length = input_size != 0u ? input_size : strlen(input);
    if (length > limit) return NULL;
    copy = xrtMalloc(length + 1u);
    if (copy == NULL) return NULL;
    memcpy(copy, input, length + 1u);
    if (size != NULL) *size = length;
    return copy;
}

int main(void)
{
    long page = sysconf(_SC_PAGESIZE);
    if (page <= 0) return 1;
    input = "100 2 0 0 0 0 0\n";
    if (xprocps_mem_kb(1) != (int64_t)(2 * page / 1024)) return 2;
    input = "-1 2 0 0 0 0 0\n";
    if (xprocps_mem_kb(1) != -1 || xrtGetError() != NULL) {
        fputs("negative virtual-page count was accepted\n", stderr);
        return 3;
    }
    input = "100 2junk 0 0 0 0 0\n";
    if (xprocps_mem_kb(1) != -1 || xrtGetError() != NULL) return 4;
    input = "100 2\0junk\n";
    input_size = sizeof("100 2\0junk\n") - 1u;
    if (xprocps_mem_kb(1) != -1 || xrtGetError() != NULL) return 5;
    read_error = XERR_NOT_FOUND;
    if (xprocps_mem_kb(1) != -1 || xrtGetError() != NULL) return 6;
    read_error = XERR_MEMORY;
    if (xprocps_mem_kb(1) != -1 || xrtGetError() == NULL ||
        xrtErrorKind(xrtGetError()) != XERR_MEMORY) return 7;
    return 0;
}
