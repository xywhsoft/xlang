#define XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_UNICODE
#include "xrt.h"
#include "../include/procps_native.h"
#include <stdio.h>
#include <string.h>
#ifndef _WIN32
#include <unistd.h>
#endif

#define CHECK(test) do { if (!(test)) { \
    fprintf(stderr, "procps test failed at line %d: %s\n", __LINE__, #test); \
    return 1; \
} } while (0)

int main(int argc, char** argv)
{
    int64_t self = xprocps_pid();
    int64_t parent = xprocps_ppid();
    xvalue* ids;
    char* first;
    char* second;
    size_t i;
    int found = 0;

#ifndef _WIN32
    if (argc == 2 && strcmp(argv[1], "--invalid-name") == 0) {
        char target[4096];
        ssize_t length = readlink("/proc/self/exe", target, sizeof(target));
        char* name;
        CHECK(length > 0 && (size_t)length < sizeof(target));
        CHECK(!xrtUtf8Valid((xstrview){ target, (size_t)length }, NULL));
        xrtClearError();
        name = xprocps_name(xprocps_pid());
        if (name != NULL) xrtFree(name);
        CHECK(name == NULL);
        CHECK(xrtErrorKind(xrtGetError()) == XERR_VALUE);
        puts("procps invalid UTF-8 name: PASS");
        return 0;
    }
#else
    (void)argc;
    (void)argv;
#endif

    CHECK(self > 0 && parent > 0 && parent != self);
    CHECK(xprocps_exists(self));
    CHECK(!xprocps_exists(0));
    CHECK(!xprocps_exists(-1));
    CHECK(!xprocps_exists(INT64_MAX));
    CHECK(xprocps_mem_kb(self) > 0);
    CHECK(xprocps_mem_kb(-1) == -1);
    xrtSetErrorInfo(XERR_VALUE, "procps.test", 0, "stale error");
    CHECK(xprocps_pid() == self && xrtGetError() == NULL);
    xrtSetErrorInfo(XERR_VALUE, "procps.test", 0, "stale error");
    CHECK(xprocps_ppid() == parent && xrtGetError() == NULL);
    xrtSetErrorInfo(XERR_VALUE, "procps.test", 0, "stale error");
    CHECK(xprocps_exists(self) && xrtGetError() == NULL);
    xrtSetErrorInfo(XERR_VALUE, "procps.test", 0, "stale error");
    CHECK(!xprocps_exists(0) && xrtGetError() == NULL);
    xrtSetErrorInfo(XERR_VALUE, "procps.test", 0, "stale error");
    CHECK(xprocps_count() > 0 && xrtGetError() == NULL);
    xrtSetErrorInfo(XERR_VALUE, "procps.test", 0, "stale error");
    CHECK(xprocps_mem_kb(self) > 0 && xrtGetError() == NULL);
    first = xprocps_name(self);
    second = xprocps_name(self);
    CHECK(first != NULL && second != NULL && first != second);
    CHECK(*first != '\0' && strcmp(first, second) == 0);
    xrtFree(first);
    xrtFree(second);
    xrtSetErrorInfo(XERR_VALUE, "procps.test", 0, "stale error");
    first = xprocps_name(self);
    CHECK(first != NULL && xrtGetError() == NULL);
    xrtFree(first);
    CHECK(xprocps_name(-1) == NULL);

    xrtSetErrorInfo(XERR_VALUE, "procps.test", 0, "stale error");
    ids = xprocps_pids();
    CHECK(ids != NULL && xrtGetError() == NULL && xrtValueCount(ids) > 0);
    for (i = 0; i < xrtValueCount(ids); ++i) {
        int64_t pid = -1;
        CHECK(xrtValueGetInt(xrtValueArrayGet(ids, i), &pid));
        if (pid == self) found = 1;
    }
    CHECK(found && xprocps_count() > 0);
    xrtValueRelease(ids);
    puts("procps C boundaries: PASS");
    return 0;
}
