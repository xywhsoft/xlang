#define XRT_MODULE_MEMORY_DEBUG
#include "lib/xrt/xrt.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
void member_prepare(void);
void member_finish(void);
void member_sync_run(int mode, int present, int stage);
void member_regular_run(void);
void member_async_run(int mode, int present, int reject);
void member_generator_run(int mode, int present, int stop, int stage);
static void cycle(void)
{
    int mode, present, stage, stop;
    for (mode = 0; mode < 3; ++mode)
        for (present = 0; present < 2; ++present) {
            for (stage = 0; stage < 4; ++stage) member_sync_run(mode, present, stage);
            for (stage = 0; stage < 2; ++stage) {
                member_async_run(mode, present, stage);
                for (stop = 0; stop < 2; ++stop) member_generator_run(mode, present, stop, stage);
            }
        }
    member_regular_run();
}
int main(int argc, char** argv)
{
    xmemdebugsnapshot initial, before, after;
    int iteration, smoke = argc == 2 && strcmp(argv[1], "--smoke") == 0;
    assert(smoke || (argc == 1 && xrtMemDebugEnable(true)));
    if (!smoke) xrtMemDebugSnapshot(&initial);
    member_prepare(); cycle();
    if (!smoke) xrtMemDebugSnapshot(&before);
    for (iteration = 0; iteration < 100; ++iteration) cycle();
    if (!smoke) {
        xrtMemDebugSnapshot(&after);
        if (before.LiveCount != after.LiveCount || before.LiveBytes != after.LiveBytes)
            fprintf(stderr, "member live before=%llu/%llu after=%llu/%llu\n",
                (unsigned long long)before.LiveCount, (unsigned long long)before.LiveBytes,
                (unsigned long long)after.LiveCount, (unsigned long long)after.LiveBytes);
        assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
        assert(after.AllocCount - before.AllocCount == after.FreeCount - before.FreeCount);
        assert(before.DoubleFreeCount == after.DoubleFreeCount && before.InvalidFreeCount == after.InvalidFreeCount);
        assert(before.UseAfterFreeCount == after.UseAfterFreeCount);
        printf("member assignment: %llu balanced allocations\n", (unsigned long long)(after.AllocCount - before.AllocCount));
    }
    puts("member assignment: 2400 conditional, 1200 genuine async, 2400 generator, 700 regular/static cases");
    member_finish();
    if (!smoke) {
        xrtMemDebugSnapshot(&after);
        assert(initial.LiveCount == after.LiveCount && initial.LiveBytes == after.LiveBytes);
        assert(initial.DoubleFreeCount == after.DoubleFreeCount && initial.InvalidFreeCount == after.InvalidFreeCount);
        assert(initial.UseAfterFreeCount == after.UseAfterFreeCount);
        puts("member assignment: module finalization restored the initial allocation baseline");
    }
    return 0;
}
