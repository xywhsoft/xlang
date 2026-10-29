#include "xrt.h"
#include "../include/procps_native.h"
#include <errno.h>
#include <sys/types.h>

static xerrkind read_error;

/* 强制走 comm 回退分支，避免依赖测试机上的真实进程状态。 */
ssize_t __wrap_readlink(const char* path, char* buffer, size_t capacity)
{
    (void)path;
    (void)buffer;
    (void)capacity;
    errno = ENOENT;
    return -1;
}

ssize_t __wrap___readlink_chk(const char* path, char* buffer,
    size_t capacity, size_t buffer_size)
{
    (void)buffer_size;
    return __wrap_readlink(path, buffer, capacity);
}

bytes __wrap_xrtFileReadAllLimit(cstr path, size_t limit, size_t* size)
{
    (void)path;
    (void)limit;
    (void)size;
    xrtSetErrorInfo(read_error, "procps.test", 0, "injected comm read error");
    return NULL;
}

int main(void)
{
    read_error = XERR_NOT_FOUND;
    if (xprocps_name(1) != NULL || xrtGetError() != NULL) return 1;
    read_error = XERR_MEMORY;
    if (xprocps_name(1) != NULL || xrtGetError() == NULL ||
        xrtErrorKind(xrtGetError()) != XERR_MEMORY) return 2;
    return 0;
}
