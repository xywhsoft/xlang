#define XRT_MODULE_ALL
#include "xrt.h"
#include "../watch_native.h"
#include "workdir.h"
#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "watch test failed at line %d: %s\n", __LINE__, #expr); \
    return 1; \
} } while (0)

static int run_tests(void) {
    const char *path = "build/test/watch_guard/监视.txt";
    void *watcher;
    int64_t index;
    char *event, *saved;
    const char *borrowed;
    int i;
    CHECK(xrtMemDebugFailAfter(0));
    CHECK(xwatch_create() == NULL);
    CHECK(xrtMemDebugFailTriggered());
    xrtMemDebugFailClear();
    xrtClearError();
    watcher = xwatch_create();
    CHECK(watcher != NULL);
    if (xrtPathExists(path)) CHECK(xrtFileDelete(path));
    CHECK(!xwatch_exists(path));
    index = xwatch_add(watcher, path);
    CHECK(index == 0 && xwatch_count(watcher) == 1);
    CHECK(strcmp(xwatch_path(watcher, index), path) == 0);
    event = xwatch_poll(watcher);
    CHECK(event && *event == '\0');
    xrtFree(event);

    CHECK(xrtFileWriteAll(path, (xbytesview){(const uint8_t*)"a", 1}));
    CHECK(xwatch_exists(path));
    saved = xwatch_poll(watcher);
    CHECK(saved && strstr(saved, "CREATED:") == saved && strstr(saved, path));
    CHECK(xrtFileWriteAll(path, (xbytesview){(const uint8_t*)"second", 6}));
    event = xwatch_poll(watcher);
    CHECK(event && strstr(event, "MODIFIED:") == event);
    xrtFree(event);
    for (i = 0; i < 20; i++) {
        event = xwatch_poll(watcher);
        CHECK(event && *event == '\0');
        xrtFree(event);
    }
    CHECK(strstr(saved, path) != NULL);
    xrtFree(saved);

    CHECK(xrtFileDelete(path));
    event = xwatch_poll(watcher);
    CHECK(event && strstr(event, "DELETED:") == event);
    xrtFree(event);
    CHECK(xwatch_remove(watcher, index) && !xwatch_remove(watcher, index));
    CHECK(xwatch_count(watcher) == 0);
    CHECK(strcmp(xwatch_path(watcher, index), "") == 0);
    CHECK(xwatch_add(watcher, path) == index);
    borrowed = xwatch_path(watcher, index);
    for (i = 0; i < 40; i++) CHECK(xwatch_add(watcher, path) >= 0);
    CHECK(xwatch_count(watcher) == 41);
    /* 条目表扩容不能使仍在使用的借用路径失效。 */
    CHECK(xwatch_path(watcher, index) == borrowed);
    CHECK(strcmp(borrowed, path) == 0);
    CHECK(xwatch_add(watcher, "") == -1);
    xrtClearError();
    xwatch_destroy(watcher);

    {
        const char *directory = "build/test/watch_guard/目录";
        const char *child = "build/test/watch_guard/目录/child.txt";
        void *folder = xwatch_create();
        CHECK(folder != NULL);
        if (xrtPathExists(child)) CHECK(xrtFileDelete(child));
        CHECK(xrtDirCreateAll(directory));
        CHECK(xwatch_add(folder, directory) == 0);
        CHECK(xrtFileWriteAll(child, (xbytesview){(const uint8_t*)"ok", 2}));
        event = xwatch_poll(folder);
        CHECK(event && strstr(event, "MODIFIED:") == event);
        xrtFree(event);
        CHECK(xrtFileDelete(child));
        event = xwatch_poll(folder);
        CHECK(event && strstr(event, "MODIFIED:") == event);
        xrtFree(event);
        xwatch_destroy(folder);
        CHECK(xrtDirRemove(directory));
    }
    {
        const char *left = "build/test/watch_guard/left.txt";
        const char *right = "build/test/watch_guard/right.txt";
        void *pair = xwatch_create();
        CHECK(pair != NULL);
        CHECK(xrtFileWriteAll(left, (xbytesview){(const uint8_t*)"a", 1}));
        CHECK(xrtFileWriteAll(right, (xbytesview){(const uint8_t*)"b", 1}));
        CHECK(xwatch_add(pair, left) == 0 && xwatch_add(pair, right) == 1);
        CHECK(xrtFileWriteAll(left, (xbytesview){(const uint8_t*)"aa", 2}));
        CHECK(xrtFileWriteAll(right, (xbytesview){(const uint8_t*)"bb", 2}));
        event = xwatch_poll(pair);
        CHECK(event && strstr(event, left));
        xrtFree(event);
        event = xwatch_poll(pair);
        CHECK(event && strstr(event, right));
        xrtFree(event);
        xwatch_destroy(pair);
        CHECK(xrtFileDelete(left) && xrtFileDelete(right));
    }
    {
        const char *target = "build/test/watch_guard/replace_target.txt";
        const char *replacement = "build/test/watch_guard/replace_pending.txt";
        xfileinfo before, after;
        xtime modified;
        void *single = xwatch_create();
        CHECK(single != NULL);
        CHECK(xrtFileWriteAll(target, (xbytesview){(const uint8_t*)"old!", 4}));
        CHECK(xrtPathStat(target, true, &before));
        modified = before.Modified - before.Modified % 1000000;
        CHECK(xrtPathSetTimes(target, true, NULL, &modified));
        CHECK(xrtPathStat(target, true, &before));
        CHECK(xwatch_add(single, target) == 0);
        CHECK(xrtFileWriteAll(replacement,
            (xbytesview){(const uint8_t*)"new!", 4}));
        CHECK(xrtPathSetTimes(replacement, true, NULL, &before.Modified));
        CHECK(xrtPathRename(replacement, target, true));
        CHECK(xrtPathStat(target, true, &after));
        CHECK(before.Size == after.Size && before.Modified == after.Modified);
        event = xwatch_poll(single);
        CHECK(event && strcmp(event, "MODIFIED:build/test/watch_guard/replace_target.txt") == 0);
        xrtFree(event);
        xwatch_destroy(single);
        CHECK(xrtFileDelete(target));
    }
    {
        const char *directory = "build/test/watch_guard/replace_dir";
        const char *target = "build/test/watch_guard/replace_dir/child.txt";
        const char *replacement = "build/test/watch_guard/replace_dir/pending.txt";
        xfileinfo before_dir, after_dir, before_file, after_file;
        xtime modified;
        void *folder = xwatch_create();
        CHECK(folder != NULL && xrtDirCreateAll(directory));
        CHECK(xrtFileWriteAll(target, (xbytesview){(const uint8_t*)"old!", 4}));
        CHECK(xrtPathStat(target, true, &before_file));
        modified = before_file.Modified - before_file.Modified % 1000000;
        CHECK(xrtPathSetTimes(target, true, NULL, &modified));
        CHECK(xrtPathStat(target, true, &before_file));
        CHECK(xrtPathStat(directory, true, &before_dir));
        modified = before_dir.Modified - before_dir.Modified % 1000000;
        CHECK(xrtPathSetTimes(directory, true, NULL, &modified));
        CHECK(xwatch_add(folder, directory) == 0);
        CHECK(xrtPathStat(directory, true, &before_dir));
        CHECK(xrtFileWriteAll(replacement,
            (xbytesview){(const uint8_t*)"new!", 4}));
        CHECK(xrtPathSetTimes(replacement, true, NULL, &before_file.Modified));
        CHECK(xrtPathRename(replacement, target, true));
        CHECK(xrtPathSetTimes(directory, true, NULL, &before_dir.Modified));
        CHECK(xrtPathStat(directory, true, &after_dir));
        CHECK(xrtPathStat(target, true, &after_file));
        CHECK(before_dir.Modified == after_dir.Modified &&
            before_file.Size == after_file.Size &&
            before_file.Modified == after_file.Modified);
        event = xwatch_poll(folder);
        CHECK(event && strcmp(event, "MODIFIED:build/test/watch_guard/replace_dir") == 0);
        xrtFree(event);
        xwatch_destroy(folder);
        CHECK(xrtFileDelete(target) && xrtDirRemove(directory));
    }
    {
        const char *relative = "build/test/watch_guard/cwd_target.txt";
        const char *other = "build/test/watch_guard/cwd_elsewhere";
        char *cwd = xrtPathCwd();
        char *target = xrtPathAbs(relative);
        char *other_abs = xrtPathAbs(other);
        void *single = xwatch_create();
        CHECK(cwd != NULL && target != NULL && other_abs != NULL && single != NULL);
        if (xrtPathExists(target)) CHECK(xrtFileDelete(target));
        CHECK(xrtDirCreateAll(other_abs));
        CHECK(xwatch_add(single, relative) == 0);
        CHECK(xrtPathSetCwd(other_abs));
        CHECK(xrtFileWriteAll(target, (xbytesview){(const uint8_t *)"x", 1}));
        event = xwatch_poll(single);
        CHECK(event != NULL && strcmp(event,
            "CREATED:build/test/watch_guard/cwd_target.txt") == 0);
        CHECK(strcmp(xwatch_path(single, 0), relative) == 0);
        CHECK(xrtPathSetCwd(cwd));
        xrtFree(event);
        xwatch_destroy(single);
        CHECK(xrtFileDelete(target) && xrtDirRemove(other_abs));
        xrtFree(cwd);
        xrtFree(target);
        xrtFree(other_abs);
    }
    {
        void *single = xwatch_create();
        unsigned failure;
        CHECK(single != NULL);
        for (failure = 0; failure < 24; ++failure) {
            int triggered;
            CHECK(xrtMemDebugFailAfter(failure));
            index = xwatch_add(single, "build/test/watch_guard/fault_target.txt");
            triggered = xrtMemDebugFailTriggered();
            xrtMemDebugFailClear();
            if (triggered) CHECK(index == -1 && xwatch_count(single) == 0);
            else CHECK(index == 0 && xwatch_remove(single, index));
            xrtClearError();
        }
        CHECK(xwatch_add(single, "build/test/watch_guard/fault_target.txt") == 0);
        xwatch_destroy(single);
    }
    {
        const char *target = "build/test/watch_guard/poll_fault.txt";
        unsigned failure;
        int saw_failure = 0, saw_success = 0;
        for (failure = 0; failure < 32; ++failure) {
            void *single = xwatch_create();
            int triggered;
            CHECK(single != NULL);
            if (xrtPathExists(target)) CHECK(xrtFileDelete(target));
            CHECK(xwatch_add(single, target) == 0);
            CHECK(xrtFileWriteAll(target, (xbytesview){(const uint8_t *)"x", 1}));
            CHECK(xrtMemDebugFailAfter(failure));
            event = xwatch_poll(single);
            triggered = xrtMemDebugFailTriggered();
            xrtMemDebugFailClear();
            if (triggered) {
                saw_failure = 1;
                CHECK(event == NULL && xrtGetError() != NULL &&
                    xrtErrorKind(xrtGetError()) == XERR_MEMORY);
                event = xwatch_poll(single);
                CHECK(event != NULL && strcmp(event,
                    "CREATED:build/test/watch_guard/poll_fault.txt") == 0 &&
                    xrtGetError() == NULL);
            } else {
                saw_success = 1;
                CHECK(event != NULL && strcmp(event,
                    "CREATED:build/test/watch_guard/poll_fault.txt") == 0 &&
                    xrtGetError() == NULL);
            }
            xrtFree(event);
            xwatch_destroy(single);
            CHECK(xrtFileDelete(target));
            if (saw_failure && saw_success) break;
        }
        CHECK(saw_failure && saw_success);
    }
    {
        const char *target = "build/test/watch_guard/error_recovery.txt";
        void *single;
        CHECK(xrtFileWriteAll(target, (xbytesview){(const uint8_t *)"x", 1}));
        single = xwatch_create();
        CHECK(single != NULL);
        CHECK(xwatch_add(single, "") == -1 && xrtGetError() != NULL);
        CHECK(xwatch_add(single, target) == 0 && xrtGetError() == NULL);
        CHECK(xwatch_poll(NULL) == NULL && xrtGetError() != NULL);
        event = xwatch_poll(single);
        CHECK(event != NULL && *event == '\0' && xrtGetError() == NULL);
        xrtFree(event);
        CHECK(!xwatch_remove(single, 99) && xrtGetError() == NULL);
        CHECK(!xwatch_exists("build/test/watch_guard/absent.txt") &&
            xrtGetError() == NULL);
        xwatch_destroy(single);
        CHECK(xrtFileDelete(target));
    }
    return 0;
}

int main(void) {
    return watch_test_workdir(run_tests);
}
