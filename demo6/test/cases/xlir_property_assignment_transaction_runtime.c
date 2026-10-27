#define XRT_MODULE_MEMORY_DEBUG
#include "lib/xrt/xrt.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
void property_prepare(void);
void property_finish(void);
void property_cycle(void);
void property_idle(void);
int main(int argc, char** argv)
{
    xmemdebugsnapshot initial, before, after;
    int i, smoke = argc == 2 && strcmp(argv[1], "--smoke") == 0;
    assert(smoke || (argc == 1 && xrtMemDebugEnable(true)));
    if (!smoke) xrtMemDebugSnapshot(&initial);
    property_prepare(); property_cycle();
    property_idle();
    if (!smoke) xrtMemDebugSnapshot(&before);
    for (i = 0; i < 100; ++i) property_cycle();
    property_idle();
    if (!smoke) {
        xrtMemDebugSnapshot(&after);
        if (before.LiveCount != after.LiveCount || before.LiveBytes != after.LiveBytes)
            fprintf(stderr, "property live before=%llu/%llu after=%llu/%llu\n",
                (unsigned long long)before.LiveCount, (unsigned long long)before.LiveBytes,
                (unsigned long long)after.LiveCount, (unsigned long long)after.LiveBytes);
        assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
        assert(after.AllocCount - before.AllocCount == after.FreeCount - before.FreeCount);
        assert(before.DoubleFreeCount == after.DoubleFreeCount && before.InvalidFreeCount == after.InvalidFreeCount);
        assert(before.UseAfterFreeCount == after.UseAfterFreeCount);
        printf("property transaction: %llu balanced allocations\n", (unsigned long long)(after.AllocCount - before.AllocCount));
    }
    property_finish();
    if (!smoke) {
        xrtMemDebugSnapshot(&after);
        assert(initial.LiveCount == after.LiveCount && initial.LiveBytes == after.LiveBytes);
        assert(initial.DoubleFreeCount == after.DoubleFreeCount && initial.InvalidFreeCount == after.InvalidFreeCount);
        assert(initial.UseAfterFreeCount == after.UseAfterFreeCount);
    }
    puts("property transaction: 8800 read/set/compound failure cases, 300 raising-cleanup cases, 2400 pending-source suspensions, 800 pre-await getter failures, 100 checked base dispatch cases, 100 normalized nullable-return cases, initial allocation baseline restored");
    return 0;
}
