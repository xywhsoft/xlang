#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_ERROR
#include "lib/xrt/xrt.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
extern int storage_exchange_init(void);
extern void storage_exchange_fini(void);
extern void storage_exchange_cycle(int, int);
int main(int argc, char** argv)
{
    int smoke = argc == 2 && !strcmp(argv[1], "--smoke");
    unsigned round, kind, mode, executions = 0;
    xmemdebugsnapshot initial, before, after;
    assert(smoke || (argc == 1 && xrtMemDebugEnable(true)));
    if (!smoke) xrtMemDebugSnapshot(&initial);
    assert(storage_exchange_init());
    for (round = 0; round < (smoke ? 1u : 100u); ++round)
    for (kind = 0; kind < 4; ++kind)
    for (mode = 0; mode < 3; ++mode) {
        if (!smoke) xrtMemDebugSnapshot(&before);
        storage_exchange_cycle((int)kind, (int)mode); ++executions;
        if (!smoke) {
            xrtMemDebugSnapshot(&after);
            assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
            assert(after.AllocCount-before.AllocCount == after.FreeCount-before.FreeCount);
            assert(before.InvalidFreeCount == after.InvalidFreeCount && before.DoubleFreeCount == after.DoubleFreeCount);
            assert(before.UseAfterFreeCount == after.UseAfterFreeCount);
        }
    }
    storage_exchange_fini(); assert(!xrtGetError());
    if (!smoke) {
        xrtMemDebugSnapshot(&after);
        assert(initial.LiveCount == after.LiveCount && initial.LiveBytes == after.LiveBytes);
        assert(after.AllocCount-initial.AllocCount == after.FreeCount-initial.FreeCount);
        printf("storage exchange: %u executions, %llu balanced allocations; reentry observes publication, reentrant write survives, cell/global/byref/generator, explicit failure with and without diagnostic\n",
            executions, (unsigned long long)(after.AllocCount-initial.AllocCount));
    } else puts("storage exchange generated C-host smoke passed");
    return 0;
}
