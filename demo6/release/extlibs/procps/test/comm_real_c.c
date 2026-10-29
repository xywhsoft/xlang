#define XRT_MODULE_FILE_WHOLE
#include "xrt.h"
#include "../include/procps_native.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/prctl.h>
#include <unistd.h>

/* 仅使本进程名称查询回退；comm 内容仍由真实 Linux 内核产生。 */
ssize_t __wrap_readlink(const char* path, char* buffer, size_t capacity)
{
    (void)path; (void)buffer; (void)capacity;
    errno = ENOENT;
    return -1;
}

ssize_t __wrap___readlink_chk(const char* path, char* buffer, size_t capacity, size_t object_size)
{
    (void)object_size;
    return __wrap_readlink(path, buffer, capacity);
}

int main(void)
{
    char previous[16] = {0};
    static const char expected[] = "left\nright\r";
    if (prctl(PR_GET_NAME, previous, 0, 0, 0) != 0 ||
        prctl(PR_SET_NAME, expected, 0, 0, 0) != 0) return 2;
    size_t size;
    bytes raw = xrtFileReadAll("/proc/self/comm", &size);
    bool raw_matches = raw && size == sizeof(expected) && raw[size - 1] == '\n' &&
        memcmp(raw, expected, sizeof(expected) - 1u) == 0;
    xrtFree(raw);
    char* actual = xprocps_name((int64_t)getpid());
    bool ok = raw_matches && actual && strcmp(actual, expected) == 0 && !xrtGetError();
    if (prctl(PR_SET_NAME, previous, 0, 0, 0) != 0) ok = false;
    xrtFree(actual);
    printf("procps real comm: kernel bytes match=%d\n", (int)raw_matches);
    puts(ok ? "procps real comm: PASS" : "procps real comm: FAIL");
    return ok ? 0 : 1;
}
