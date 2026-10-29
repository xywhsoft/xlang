#define XRT_MODULE_MEMORY_DEBUG
#include "lib/xrt/xrt.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
void dynamic_bound_cycle(int allocationFaults);
int main(int argc, char** argv)
{
    xmemdebugsnapshot initial, before, after;
    int i, smoke = argc == 2 && strcmp(argv[1], "--smoke") == 0;
    assert(smoke || (argc == 1 && xrtMemDebugEnable(true)));
    if (!smoke) xrtMemDebugSnapshot(&initial);
    dynamic_bound_cycle(!smoke);
    if (!smoke) xrtMemDebugSnapshot(&before);
    for (i = 0; i < 100; ++i) dynamic_bound_cycle(!smoke);
    if (!smoke) {
        xrtMemDebugSnapshot(&after);
        if (initial.LiveCount != after.LiveCount || initial.LiveBytes != after.LiveBytes)
            fprintf(stderr, "dynamic bound method initial=%llu/%llu before=%llu/%llu after=%llu/%llu\n",
                (unsigned long long)initial.LiveCount, (unsigned long long)initial.LiveBytes,
                (unsigned long long)before.LiveCount, (unsigned long long)before.LiveBytes,
                (unsigned long long)after.LiveCount, (unsigned long long)after.LiveBytes);
        assert(initial.LiveCount == after.LiveCount && initial.LiveBytes == after.LiveBytes);
        assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
        assert(after.AllocCount - before.AllocCount == after.FreeCount - before.FreeCount);
        assert(initial.InvalidFreeCount == after.InvalidFreeCount && initial.DoubleFreeCount == after.DoubleFreeCount);
        assert(initial.UseAfterFreeCount == after.UseAfterFreeCount);
        printf("dynamic bound method: %llu balanced allocations\n", (unsigned long long)(after.AllocCount - before.AllocCount));
    }
    puts("dynamic bound method: 100 module lifecycles, retained receiver identity, inherited overloads, checked calls, transport rollback, 300 real async suspensions");
    if (!smoke) puts("dynamic bound method: 400 real allocation faults in environment/callable/value creation and borrowed-frame copying");
    return 0;
}
