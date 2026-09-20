#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_ERROR
#include "lib/xrt/xrt.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "xlir_owner_fault_runtime.h"
extern void owner_views_cycle(int);
extern unsigned owner_views_fault(int);
int main(int argc, char** argv)
{
    int smoke = argc == 2 && !strcmp(argv[1], "--smoke");
    unsigned round, index, executions = 0; xmemdebugsnapshot initial, before, after;
    const unsigned stops[] = {1, 2, 0};
    assert(smoke || (argc == 1 && xrtMemDebugEnable(true)));
    if (!smoke) xrtMemDebugSnapshot(&initial);
    for (round = 0; round < (smoke ? 1u : 100u); ++round)
    for (index = 0; index < 3; ++index) {
        unsigned stop = stops[index];
        if (!smoke) xrtMemDebugSnapshot(&before);
        owner_views_cycle((int)stop); ++executions;
        if (!smoke) {
            xrtMemDebugSnapshot(&after);
            if (before.DoubleFreeCount != after.DoubleFreeCount || before.InvalidFreeCount != after.InvalidFreeCount)
                fprintf(stderr, "owner views stop=%u double-free=%llu invalid-free=%llu\n", stop,
                    (unsigned long long)(after.DoubleFreeCount-before.DoubleFreeCount),
                    (unsigned long long)(after.InvalidFreeCount-before.InvalidFreeCount));
            assert(before.DoubleFreeCount == after.DoubleFreeCount && before.InvalidFreeCount == after.InvalidFreeCount);
            assert(before.UseAfterFreeCount == after.UseAfterFreeCount);
            assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
            assert(after.AllocCount-before.AllocCount == after.FreeCount-before.FreeCount);
        }
        assert(!xrtGetError());
    }
    if (!smoke) {
        unsigned fault, faults = 0;
        for (round = 0; round < 50; ++round) {
            unsigned allocations;
            xrtMemDebugSnapshot(&before); allocations = owner_views_fault(-1);
            continuation_owner_fault_balanced(&before); assert(allocations >= 2);
            for (fault = 0; fault < allocations; ++fault) {
                xrtMemDebugSnapshot(&before); owner_views_fault((int)fault);
                continuation_owner_fault_balanced(&before); ++faults;
            }
        }
        printf("continuation owner views: %u real allocation failures before first suspension; SSA rollback verified\n", faults);
        xrtMemDebugSnapshot(&after);
        assert(initial.LiveCount == after.LiveCount && initial.LiveBytes == after.LiveBytes);
        printf("continuation owner views: %u executions, %llu balanced allocations; natural completion and each early close\n",
            executions, (unsigned long long)(after.AllocCount-initial.AllocCount));
    } else printf("continuation owner views C-host smoke: %u executions\n", executions);
    return 0;
}
