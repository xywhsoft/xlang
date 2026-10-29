#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_ERROR
#include "lib/xrt/xrt.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "xlir_owner_fault_runtime.h"
extern void owner_cfg_cycle(int, int, int);
extern unsigned owner_cfg_fault(int, int, int, int);
int main(int argc, char** argv)
{
    int smoke = argc == 2 && !strcmp(argv[1], "--smoke"), kind, choice, stop, round, executions = 0;
    xmemdebugsnapshot initial, before, after;
    assert(smoke || (argc == 1 && xrtMemDebugEnable(true)));
    if (!smoke) xrtMemDebugSnapshot(&initial);
    for (round = 0; round < (smoke ? 1 : 100); ++round)
    for (kind = 0; kind < 4; ++kind)
    for (choice = 0; choice <= (kind == 2 ? 3 : 1); ++choice) {
        int yields = kind == 0 || kind == 3 ? 2 : kind == 1 ? 2 + choice : 2 * choice;
        for (stop = 0; stop <= yields; ++stop) {
            if (!smoke) xrtMemDebugSnapshot(&before);
            owner_cfg_cycle(kind, choice, stop); ++executions;
            if (!smoke) {
                xrtMemDebugSnapshot(&after);
                assert(after.DoubleFreeCount == before.DoubleFreeCount && after.InvalidFreeCount == before.InvalidFreeCount);
                assert(after.UseAfterFreeCount == before.UseAfterFreeCount);
                assert(after.LiveCount == before.LiveCount && after.LiveBytes == before.LiveBytes);
                assert(after.AllocCount - before.AllocCount == after.FreeCount - before.FreeCount);
            }
            assert(!xrtGetError());
        }
    }
    if (!smoke) {
        unsigned faults = 0, residentFaults = 0, fault;
        for (round = 0; round < 20; ++round)
        for (kind = 0; kind < 4; ++kind)
        for (choice = 0; choice <= (kind == 2 ? 3 : 1); ++choice) {
            int yields = kind == 0 || kind == 3 ? 2 : kind == 1 ? 2 + choice : 2 * choice;
            for (stop = 0; stop < yields; ++stop) {
                unsigned allocations;
                xrtMemDebugSnapshot(&before); allocations = owner_cfg_fault(kind, choice, stop, -1);
                continuation_owner_fault_balanced(&before);
                for (fault = 0; fault < allocations; ++fault) {
                    xrtMemDebugSnapshot(&before); owner_cfg_fault(kind, choice, stop, (int)fault);
                    continuation_owner_fault_balanced(&before); ++faults;
                    if (kind == 1 && choice == 1 && stop == 1) ++residentFaults;
                }
            }
        }
        assert(faults && residentFaults);
        printf("continuation owner CFG: %u real allocation failures, %u after a resident-owner suspension; branch/loop/storage rollback\n", faults, residentFaults);
        xrtMemDebugSnapshot(&after);
        assert(initial.LiveCount == after.LiveCount && initial.LiveBytes == after.LiveBytes);
        printf("continuation owner CFG: %d executions, %llu balanced allocations; branch sinks, skipped first state, loop reacquisition, storage transfer, every early close\n", executions, (unsigned long long)(after.AllocCount - initial.AllocCount));
    } else printf("continuation owner CFG C-host smoke: %d executions\n", executions);
    return 0;
}
