#define XRT_MODULE_MEMORY_DEBUG
#include "lib/xrt/xrt.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
void dynamic_setter_cycle(int smoke);
unsigned dynamic_setter_allocation_faults(void);
int main(int argc, char** argv)
{
    xmemdebugsnapshot initial, before, after;
    unsigned faultsBefore;
    int i, smoke = argc == 2 && strcmp(argv[1], "--smoke") == 0;
    assert(smoke || (argc == 1 && xrtMemDebugEnable(true)));
    if (!smoke) xrtMemDebugSnapshot(&initial);
    dynamic_setter_cycle(smoke);
    faultsBefore = dynamic_setter_allocation_faults();
    if (!smoke) xrtMemDebugSnapshot(&before);
    for (i = 0; i < 100; ++i) dynamic_setter_cycle(smoke);
    if (!smoke) {
        xrtMemDebugSnapshot(&after);
        if (initial.LiveCount != after.LiveCount || initial.LiveBytes != after.LiveBytes)
            fprintf(stderr, "dynamic setter initial=%llu/%llu before=%llu/%llu after=%llu/%llu\n",
                (unsigned long long)initial.LiveCount, (unsigned long long)initial.LiveBytes,
                (unsigned long long)before.LiveCount, (unsigned long long)before.LiveBytes,
                (unsigned long long)after.LiveCount, (unsigned long long)after.LiveBytes);
        assert(initial.LiveCount == after.LiveCount && initial.LiveBytes == after.LiveBytes);
        assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
        assert(after.AllocCount - before.AllocCount == after.FreeCount - before.FreeCount);
        assert(initial.InvalidFreeCount == after.InvalidFreeCount && initial.DoubleFreeCount == after.DoubleFreeCount);
        assert(initial.UseAfterFreeCount == after.UseAfterFreeCount);
        printf("dynamic setter: %llu balanced allocations\n", (unsigned long long)(after.AllocCount - before.AllocCount));
    }
    if (!smoke) { assert(dynamic_setter_allocation_faults() - faultsBefore >= 100); printf("dynamic setter: %u real allocation faults\n", dynamic_setter_allocation_faults() - faultsBefore); }
    puts("dynamic setter: 100 complete module lifecycles, inherited/raising setters, read-only/mismatch rejection, RHS-first ordering and fields, 300 real async suspensions");
    return 0;
}
