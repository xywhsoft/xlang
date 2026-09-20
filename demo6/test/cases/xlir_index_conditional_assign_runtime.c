#define XRT_MODULE_MEMORY_DEBUG
#include "lib/xrt/xrt.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
void conditional_prepare(void);
void conditional_finish(void);
void conditional_sync_run(int mode, int present, int stage);
void conditional_async_run(int mode, int present, int reject);
static void cycle(int* syncCases, int* asyncCases)
{
    int mode, present, stage, reject;
    for (mode = 0; mode < 5; ++mode)
        for (present = 0; present < 2; ++present)
            for (stage = 0; stage < 4; ++stage) {
                conditional_sync_run(mode, present, stage); ++*syncCases;
            }
    conditional_sync_run(0, 0, 4); ++*syncCases;
    for (mode = 0; mode < 8; ++mode)
        for (present = 0; present < 2; ++present)
            for (reject = 0; reject < 2; ++reject) {
                conditional_async_run(mode, present, reject); ++*asyncCases;
            }
}
int main(int argc, char** argv)
{
    xmemdebugsnapshot initial, before, after;
    int iteration, syncCases = 0, asyncCases = 0;
    int smoke = argc == 2 && strcmp(argv[1], "--smoke") == 0;
    assert(smoke || (argc == 1 && xrtMemDebugEnable(true)));
    if (!smoke) xrtMemDebugSnapshot(&initial);
    conditional_prepare(); cycle(&syncCases, &asyncCases);
    syncCases = 0; asyncCases = 0;
    if (!smoke) xrtMemDebugSnapshot(&before);
    for (iteration = 0; iteration < 100; ++iteration) cycle(&syncCases, &asyncCases);
    if (!smoke) {
        xrtMemDebugSnapshot(&after);
        assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
        assert(after.AllocCount - before.AllocCount == after.FreeCount - before.FreeCount);
        assert(before.DoubleFreeCount == after.DoubleFreeCount && before.InvalidFreeCount == after.InvalidFreeCount);
        assert(before.UseAfterFreeCount == after.UseAfterFreeCount);
        printf("conditional index: %d lazy sync/failure cases, %d async short-circuit/suspension cases, %llu balanced allocations\n",
            syncCases, asyncCases, (unsigned long long)(after.AllocCount - before.AllocCount));
    } else printf("conditional index smoke: %d sync cases, %d async cases\n", syncCases, asyncCases);
    conditional_finish();
    if (!smoke) {
        xrtMemDebugSnapshot(&after);
        assert(initial.LiveCount == after.LiveCount && initial.LiveBytes == after.LiveBytes);
        assert(initial.DoubleFreeCount == after.DoubleFreeCount && initial.InvalidFreeCount == after.InvalidFreeCount);
        assert(initial.UseAfterFreeCount == after.UseAfterFreeCount);
        puts("conditional index: module finalization restored initial allocation baseline");
    }
    return 0;
}
