#define XRT_MODULE_VALUE_CONTAINER
#include "xrt.h"
#include "../include/procps_native.h"

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

static int read_calls;

ssize_t __wrap_readlink(const char *path, char *buffer, size_t capacity)
{
    static char target[5000];
    size_t length;
    if (strncmp(path, "/proc/", 6) != 0) return -1;
    memset(target, 'a', 4900);
    memcpy(target + 4900, "/exact_name", sizeof("/exact_name") - 1u);
    length = 4900 + sizeof("/exact_name") - 1u;
    if (capacity > length) capacity = length;
    memcpy(buffer, target, capacity);
    read_calls++;
    return (ssize_t)capacity;
}

ssize_t __wrap___readlink_chk(const char *path, char *buffer,
    size_t capacity, size_t object_size)
{
    (void)object_size;
    return __wrap_readlink(path, buffer, capacity);
}

int main(void)
{
    char *name = xprocps_name((int64_t)getpid());
    int ok = name && strcmp(name, "exact_name") == 0 && read_calls >= 2;
    if (!ok) fprintf(stderr, "procps long executable path: %s, reads=%d\n",
        name ? name : "null", read_calls);
    xrtFree(name);
    return ok ? 0 : 1;
}
