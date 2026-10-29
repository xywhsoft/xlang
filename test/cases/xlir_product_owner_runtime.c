#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_ERROR
#include "lib/xrt/xrt.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
extern int product_owner_init(void);
extern void product_owner_fini(void);
extern void product_owner_cycle(int, int, int, int);
static unsigned budget, armed;
static int smoke, savedFault;
void product_owner_arm(void)
{
    assert(!armed++);
    savedFault = 0;
    if (!smoke) assert(xrtMemDebugFailAfter(budget));
}
int product_owner_fault_status(void) { return !smoke && (savedFault || xrtMemDebugFailTriggered()); }
void product_owner_disarm(void)
{
    /* Successful assignment ends the injected operation. Subsequent asserts
     * inspect values and may allocate, but are not part of that operation. */
    if (!smoke) { savedFault = xrtMemDebugFailTriggered(); xrtMemDebugFailClear(); }
}
int main(int argc, char** argv)
{
    static const int cases[][3] = {
        {0,0,0}, {0,1,0}, {0,2,0}, {1,0,0}, {1,1,0}, {1,2,0},
        {2,0,0}, {2,0,1}, {3,0,0}, {3,0,10}, {4,0,0}, {4,0,1}, {5,0,0}, {5,0,10}
    };
    unsigned round, mode, i, executions = 0, faults = 0;
    xmemdebugsnapshot initial, before, after;
    smoke = argc == 2 && !strcmp(argv[1], "--smoke");
    assert(smoke || (argc == 1 && xrtMemDebugEnable(true)));
    if (!smoke) xrtMemDebugSnapshot(&initial);
    assert(product_owner_init());
    for (round = 0; round < (smoke ? 1u : 50u); ++round)
    for (mode = 0; mode < 2; ++mode)
    for (i = 0; i < sizeof(cases)/sizeof(cases[0]); ++i) {
        if (!smoke) xrtMemDebugSnapshot(&before);
        product_owner_cycle(cases[i][0], cases[i][1], cases[i][2], (int)mode); ++executions;
        if (!smoke) {
            xrtMemDebugSnapshot(&after);
            if (before.LiveCount != after.LiveCount || before.LiveBytes != after.LiveBytes)
                fprintf(stderr, "product owner case=%u mode=%u live=%lld bytes=%lld\n", i, mode,
                    (long long)(after.LiveCount-before.LiveCount), (long long)(after.LiveBytes-before.LiveBytes));
            assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
            assert(after.AllocCount-before.AllocCount == after.FreeCount-before.FreeCount);
            assert(before.InvalidFreeCount == after.InvalidFreeCount && before.DoubleFreeCount == after.DoubleFreeCount);
            assert(before.UseAfterFreeCount == after.UseAfterFreeCount);
        }
    }
    /* Arm only after both RHS fields exist, then exhaust every allocation
     * boundary in string boxing and the two ordered index commits. */
    for (round = 0; round < (smoke ? 1u : 30u); ++round) {
        for (budget = 0; budget < 64; ++budget) {
            int triggered;
            if (!smoke) xrtMemDebugSnapshot(&before);
            armed = 0; product_owner_cycle(1, 3, 0, 0); ++executions;
            triggered = product_owner_fault_status();
            if (!smoke) xrtMemDebugFailClear();
            assert(armed == 1);
            if (triggered) ++faults;
            if (!smoke) {
                xrtMemDebugSnapshot(&after);
                assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
                assert(after.AllocCount-before.AllocCount == after.FreeCount-before.FreeCount);
                assert(before.InvalidFreeCount == after.InvalidFreeCount && before.DoubleFreeCount == after.DoubleFreeCount);
                assert(before.UseAfterFreeCount == after.UseAfterFreeCount);
            }
            if (!triggered) break;
        }
        assert(budget < 64);
    }
    product_owner_fini(); assert(!xrtGetError());
    if (!smoke) {
        xrtMemDebugSnapshot(&after);
        assert(initial.LiveCount == after.LiveCount && initial.LiveBytes == after.LiveBytes);
        assert(after.AllocCount-initial.AllocCount == after.FreeCount-initial.FreeCount);
        assert(faults >= 30);
        printf("product owner: %u executions, %u allocation faults, %llu balanced allocations; ordered targets, partial assignment, discard/replacement failure, original failure bit, catch/finally\n",
            executions, faults, (unsigned long long)(after.AllocCount-initial.AllocCount));
    } else puts("product owner generated C-host smoke passed");
    return 0;
}
