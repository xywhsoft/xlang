#define XRT_MODULE_MEMORY_DEBUG
#include "lib/xrt/xrt.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
void activation_prepare(void);
void activation_finish(void);
void activation_cycle(int);
unsigned activation_fault_count(void);
unsigned activation_suspension_count(void);
int main(int argc, char** argv)
{
    xmemdebugsnapshot initial, before, after;
    unsigned initialFaults, initialSuspensions;
    int i, smoke = argc == 2 && strcmp(argv[1], "--smoke") == 0;
    assert(smoke || (argc == 1 && xrtMemDebugEnable(true)));
    if (!smoke) xrtMemDebugSnapshot(&initial);
    activation_prepare(); activation_cycle(smoke);
    if (!smoke) xrtMemDebugSnapshot(&before);
    initialFaults = activation_fault_count(); initialSuspensions = activation_suspension_count();
    for (i = 0; i < 100; ++i) activation_cycle(smoke);
    assert(activation_suspension_count() - initialSuspensions == 900);
    if (!smoke) {
        xrtMemDebugSnapshot(&after);
        assert(activation_fault_count() - initialFaults == 900);
        if (before.LiveCount != after.LiveCount || before.LiveBytes != after.LiveBytes)
            fprintf(stderr, "activation live: %llu/%llu -> %llu/%llu\n",
                (unsigned long long)before.LiveCount, (unsigned long long)before.LiveBytes,
                (unsigned long long)after.LiveCount, (unsigned long long)after.LiveBytes);
        assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
        assert(after.AllocCount - before.AllocCount == after.FreeCount - before.FreeCount);
        printf("virtual activation: %llu balanced allocations, 900 real launch allocation failures (direct/class/protocol)\n",
            (unsigned long long)(after.AllocCount - before.AllocCount));
    }
    activation_finish();
    if (!smoke) {
        xrtMemDebugSnapshot(&after);
        assert(initial.LiveCount == after.LiveCount && initial.LiveBytes == after.LiveBytes);
        assert(initial.InvalidFreeCount == after.InvalidFreeCount && initial.DoubleFreeCount == after.DoubleFreeCount);
        assert(initial.UseAfterFreeCount == after.UseAfterFreeCount);
    }
    puts("virtual activation: 900 real suspensions, 900 generator completion/failure/early-close cases, lazy launch and receiver lifetime");
    return 0;
}
