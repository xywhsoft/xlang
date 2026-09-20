#define XRT_MODULE_MEMORY_DEBUG
#include "lib/xrt/xrt.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
void storage_prepare(void);
void storage_finish(void);
void storage_sync_run(int mode, int present, int stage);
void storage_async_run(int mode, int present, int reject);
void storage_generator_run(int mode, int present, int stop, int stage);
static void cycle(int* syncCases, int* asyncCases, int* generatorCases)
{
    int mode, present, stage, reject;
    for (mode = 0; mode < 12; ++mode)
        for (present = 0; present < 2; ++present)
            for (stage = 0; stage < 3; ++stage) {
                storage_sync_run(mode, present, stage); ++*syncCases;
            }
    for (mode = 0; mode < 9; ++mode)
        for (present = 0; present < 2; ++present)
            for (reject = 0; reject < 2; ++reject) {
                storage_async_run(mode, present, reject); ++*asyncCases;
            }
    for (mode = 0; mode < 3; ++mode)
        for (present = 0; present < 2; ++present)
            for (stage = 0; stage < 2; ++stage)
                for (reject = 0; reject < 2; ++reject) {
                    storage_generator_run(mode, present, reject, stage); ++*generatorCases;
                }
}
int main(int argc, char** argv)
{
    xmemdebugsnapshot initial, before, after;
    int iteration, syncCases = 0, asyncCases = 0, generatorCases = 0;
    int smoke = argc == 2 && strcmp(argv[1], "--smoke") == 0;
    assert(smoke || (argc == 1 && xrtMemDebugEnable(true)));
    if (!smoke) xrtMemDebugSnapshot(&initial);
    storage_prepare(); cycle(&syncCases, &asyncCases, &generatorCases);
    syncCases = 0; asyncCases = 0; generatorCases = 0;
    if (!smoke) xrtMemDebugSnapshot(&before);
    for (iteration = 0; iteration < 100; ++iteration) cycle(&syncCases, &asyncCases, &generatorCases);
    if (!smoke) {
        xrtMemDebugSnapshot(&after);
        assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
        assert(after.AllocCount - before.AllocCount == after.FreeCount - before.FreeCount);
        assert(before.DoubleFreeCount == after.DoubleFreeCount && before.InvalidFreeCount == after.InvalidFreeCount);
        assert(before.UseAfterFreeCount == after.UseAfterFreeCount);
        printf("conditional storage: %d sync/failure cases, %d async short-circuit/suspension cases, %llu balanced allocations\n",
            syncCases, asyncCases, (unsigned long long)(after.AllocCount - before.AllocCount));
    } else printf("conditional storage smoke: %d sync cases, %d async cases\n", syncCases, asyncCases);
    printf("conditional storage: %d generator resume/early-close/failure cases\n", generatorCases);
    storage_finish();
    if (!smoke) {
        xrtMemDebugSnapshot(&after);
        assert(initial.LiveCount == after.LiveCount && initial.LiveBytes == after.LiveBytes);
        assert(initial.DoubleFreeCount == after.DoubleFreeCount && initial.InvalidFreeCount == after.InvalidFreeCount);
        assert(initial.UseAfterFreeCount == after.UseAfterFreeCount);
        puts("conditional storage: module finalization restored initial allocation baseline");
    }
    return 0;
}
