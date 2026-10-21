#define XRT_MODULE_ALL
#include "xrt.h"
#include "../watch_native.h"
#include "workdir.h"
#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "watch race failed at line %d: %s\n", __LINE__, #expr); \
    return 1; \
} } while (0)

static const char *race_directory;
static int action_before_open;

/* 在 stat 与目录打开之间删除目标，稳定复现真实文件系统竞争。 */
xdir __real_xrtDirOpen(const char *path, uint32_t flags);
xdir __wrap_xrtDirOpen(const char *path, uint32_t flags) {
    if (action_before_open && strcmp(path, race_directory) == 0) {
        int action = action_before_open;
        action_before_open = 0;
        if (action == 2) {
            xrtSetErrorInfo(XERR_IO, "xlang.extlib.watch.test", 0,
                            "injected directory read failure");
            return NULL;
        }
        if (!xrtDirRemove(path)) return NULL;
        if (action == 3 && !xrtFileWriteAll(path,
            (xbytesview){(const uint8_t *)"file", 4})) return NULL;
    }
    return __real_xrtDirOpen(path, flags);
}

static int run_tests(void) {
    char *absolute = xrtPathAbs("build/test/watch_guard/race_directory");
    void *watcher;
    char *event;
    CHECK(absolute != NULL);
    race_directory = absolute;
    CHECK(xrtDirCreateAll(absolute));
    watcher = xwatch_create();
    CHECK(watcher != NULL);

    action_before_open = 1;
    CHECK(xwatch_add(watcher, absolute) == 0);
    CHECK(action_before_open == 0 && xrtGetError() == NULL);
    event = xwatch_poll(watcher);
    CHECK(event && *event == '\0');
    xrtFree(event);

    CHECK(xrtDirCreateAll(absolute));
    event = xwatch_poll(watcher);
    CHECK(event && strncmp(event, "CREATED:", 8) == 0);
    xrtFree(event);

    action_before_open = 2;
    CHECK(xwatch_poll(watcher) == NULL && xrtGetError() != NULL &&
          xrtErrorKind(xrtGetError()) == XERR_IO);
    event = xwatch_poll(watcher);
    CHECK(event && *event == '\0' && xrtGetError() == NULL);
    xrtFree(event);

    action_before_open = 1;
    event = xwatch_poll(watcher);
    CHECK(event && strncmp(event, "DELETED:", 8) == 0);
    CHECK(action_before_open == 0 && xrtGetError() == NULL);
    xrtFree(event);

    CHECK(xrtDirCreateAll(absolute));
    event = xwatch_poll(watcher);
    CHECK(event && strncmp(event, "CREATED:", 8) == 0);
    xrtFree(event);
    action_before_open = 3;
    event = xwatch_poll(watcher);
    CHECK(event && strncmp(event, "MODIFIED:", 9) == 0);
    CHECK(action_before_open == 0 && xrtGetError() == NULL);
    xrtFree(event);
    CHECK(xrtFileDelete(absolute));

    xwatch_destroy(watcher);
    xrtFree(absolute);
    return 0;
}

int main(void) {
    return watch_test_workdir(run_tests);
}
