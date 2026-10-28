#define XRT_MODULE_DIR
#define XRT_MODULE_PATH
#define XRT_MODULE_HASH64
#define XRT_MODULE_MEMORY_DEBUG
#include "xrt.h"
#include "../watch_native.h"
#include <stdio.h>
#include <string.h>

/* 只在本翻译单元替换取样，避免文件系统时钟粒度掩盖摘要的确定性缺陷。 */
static xdirentry items[2];
static unsigned position, opened, closed;
static bool reverse_order, fail_next, fail_close;
static int directory;

static bool sample_stat(const char* path, bool follow, xfileinfo* info)
{
    (void)path;
    (void)follow;
    memset(info, 0, sizeof(*info));
    info->Type = XFILE_TYPE_DIRECTORY;
    return true;
}

static xdir sample_open(const char* path, uint32_t flags)
{
    (void)path;
    if (flags != XDIR_STAT) return NULL;
    position = 0;
    ++opened;
    return (xdir)&directory;
}

static xdirnext sample_next(xdir dir, xdirentry* item)
{
    (void)dir;
    if (position < 2) {
        *item = items[reverse_order ? 1u - position : position];
        ++position;
        return XDIR_NEXT_ITEM;
    }
    if (fail_next) {
        xrtSetErrorInfo(XERR_IO, "watch.test.next", 13, "enumeration failed");
        return XDIR_NEXT_ERROR;
    }
    return XDIR_NEXT_END;
}

static bool sample_close(xdir dir)
{
    (void)dir;
    ++closed;
    if (fail_close) {
        xrtSetErrorInfo(XERR_NOT_FOUND, "watch.test.close", 14, "cleanup failed");
        return false;
    }
    return true;
}

#define xrtPathStat sample_stat
#define xrtDirOpen sample_open
#define xrtDirNext sample_next
#define xrtDirClose sample_close
#include "../watch_native.c"
#undef xrtPathStat
#undef xrtDirOpen
#undef xrtDirNext
#undef xrtDirClose

static unsigned cases, failures;

static void expect(bool condition, const char* label)
{
    if (!condition) { fprintf(stderr, "watch directory case %u: %s\n", cases, label); ++failures; }
}

static void reset_items(void)
{
    memset(items, 0, sizeof(items));
    for (unsigned i = 0; i < 2; ++i) {
        items[i].Name = (xstrview){i ? "b" : "a", 1};
        items[i].Info.Type = XFILE_TYPE_FILE;
        items[i].Info.Available = XFILE_INFO_SIZE | XFILE_INFO_MODIFY_TIME |
            XFILE_INFO_CHANGE_TIME | XFILE_INFO_CREATE_TIME | XFILE_INFO_IDENTITY;
        items[i].Info.Size = 4;
        items[i].Info.Modified = items[i].Info.Changed = 100;
        items[i].Info.Created = 50;
        items[i].Info.Device = 11;
        items[i].Info.Identity = 22 + i;
    }
    reverse_order = fail_next = fail_close = false;
}

static void event_for(void* watcher, bool changed)
{
    char* event = xwatch_poll(watcher);
    expect(event && !strcmp(event, changed ? "MODIFIED:sample" : "") && !xrtGetError(), "unexpected event");
    xrtFree(event);
    event = xwatch_poll(watcher);
    expect(event && !*event && !xrtGetError(), "event repeated");
    xrtFree(event);
}

int main(void)
{
    xmemdebugsnapshot before, after;
    xrtSetErrorInfo(XERR_IO, "watch.test", 0, "warm diagnostic slot");
    xrtClearError();
    xrtMemDebugSnapshot(&before);
    for (unsigned scenario = 0; scenario < 15; ++scenario) {
        reset_items();
        if (scenario == 7) items[0].Info.Available = 0;
        if (scenario == 8) {
            items[0].Info.Available &= ~XFILE_INFO_CREATE_TIME;
            items[0].Info.Created = 0;
        }
        void* watcher = xwatch_create();
        expect(watcher && xwatch_add(watcher, "sample") == 0, "initial snapshot");
        switch (scenario) {
        case 0: items[0].Info.Modified = items[0].Info.Changed = 200; break;
        case 1: items[0].Info.Modified ^= 4; items[0].Info.Created ^= 4; break;
        case 2: items[0].Info.Size ^= 4; items[0].Info.Identity ^= 4; break;
        case 3: items[0].Info.Device = 22; items[0].Info.Identity = 11; break;
        case 4: items[0].Info.Modified = 200; break;
        case 5: items[0].Info.Accessed = 300; break;
        case 6: reverse_order = true; break;
        case 7:
            items[0].Info.Size = 1000; items[0].Info.Created = 9;
            items[0].Info.Modified = 555; items[0].Info.Changed = 999;
            items[0].Info.Device = 777; items[0].Info.Identity = 888;
            break;
        case 8: items[0].Info.Available |= XFILE_INFO_CREATE_TIME; break;
        case 9: items[0].Name = (xstrview){"renamed", 7}; break;
        case 10:
            items[0].Info.Modified = items[0].Info.Changed = 200;
            expect(xrtMemDebugFailAfter(0), "arm event allocation failure");
            {
                char* event = xwatch_poll(watcher);
                bool triggered = xrtMemDebugFailTriggered();
                xrtMemDebugFailClear();
                expect(!event && triggered && xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_MEMORY, "allocation failure");
                xrtFree(event);
            }
            break;
        case 11:
            items[0].Info.Modified = 200;
            fail_next = fail_close = true;
            {
                char* event = xwatch_poll(watcher);
                expect(!event && xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_IO &&
                    !strcmp(xrtErrorDomain(xrtGetError()), "watch.test.next"), "cleanup replaced first error");
                xrtFree(event);
            }
            fail_next = fail_close = false;
            break;
        case 12: items[0].Info.Identity ^= UINT64_C(0x8000000000000000); break;
        case 13: items[0].Info.Modified = INT64_MIN; break;
        case 14: items[0].Info.Size = UINT64_MAX; break;
        }
        event_for(watcher, scenario != 5 && scenario != 6 && scenario != 7);
        xwatch_destroy(watcher);
        ++cases;
    }
    expect(opened == closed, "directory handles did not balance");
    xrtClearError();
    xrtMemDebugSnapshot(&after);
    expect(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes &&
        before.InvalidFreeCount == after.InvalidFreeCount && before.DoubleFreeCount == after.DoubleFreeCount &&
        before.OverflowCount == after.OverflowCount && before.UnderflowCount == after.UnderflowCount, "allocation counters changed");
    printf("watch directory digest: %u cases, %u failures\n", cases, failures);
    return failures ? 1 : 0;
}
