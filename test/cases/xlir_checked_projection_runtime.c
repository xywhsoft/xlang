#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_ERROR
#include "lib/xrt/xrt.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
extern int checked_projection_host_init(void);
extern void checked_projection_host_fini(void);
extern int64_t checked_projection_host_case(int);
static unsigned budget, armed, dropped, caught, finalized;
static int smoke;
void checked_projection_arm(void)
{
    assert(!armed++);
    if (!smoke) assert(xrtMemDebugFailAfter(budget));
}
void checked_projection_drop(void) { ++dropped; }
void checked_projection_caught(void) { ++caught; }
void checked_projection_finally(void) { ++finalized; }
int main(int argc, char** argv)
{
    unsigned round, kind, faults = 0, executions = 0;
    xmemdebugsnapshot initial, before, after;
    static const int expected[] = {11,11,17,11,11,5,11,5,17,11,5,11,5,5,17,11};
    smoke = argc == 2 && !strcmp(argv[1], "--smoke");
    assert(smoke || (argc == 1 && xrtMemDebugEnable(true)));
    if (!smoke) xrtMemDebugSnapshot(&initial);
    assert(checked_projection_host_init());
    for (round = 0; round < (smoke ? 1u : 30u); ++round)
    for (kind = 0; kind < sizeof(expected)/sizeof(expected[0]); ++kind) {
        for (budget = 0; budget < 32; ++budget) {
            int64_t result; int triggered; unsigned oldDrops = dropped;
            if (!smoke) xrtMemDebugSnapshot(&before);
            armed = caught = finalized = 0;
            result = checked_projection_host_case((int)kind);
            triggered = !smoke && xrtMemDebugFailTriggered();
            if (!smoke) xrtMemDebugFailClear();
            ++executions;
            if (triggered) ++faults;
            if (armed != 1 || dropped-oldDrops != 1 || finalized != 1 ||
                caught != (unsigned)triggered || result != (triggered ? 23 : expected[kind]) || xrtGetError())
                fprintf(stderr, "projection kind=%u budget=%u triggered=%d result=%lld caught=%u finally=%u drops=%u error=%d\n",
                    kind, budget, triggered, (long long)result, caught, finalized, dropped-oldDrops,
                    xrtGetError() ? (int)xrtErrorCode(xrtGetError()) : 0);
            assert(armed == 1 && dropped-oldDrops == 1 && finalized == 1);
            assert(caught == (unsigned)triggered && result == (triggered ? 23 : expected[kind]) && !xrtGetError());
            if (!smoke) {
                xrtMemDebugSnapshot(&after);
                if (before.LiveCount != after.LiveCount || before.LiveBytes != after.LiveBytes)
                    fprintf(stderr, "projection kind=%u budget=%u live=%lld bytes=%lld\n", kind, budget,
                        (long long)(after.LiveCount-before.LiveCount), (long long)(after.LiveBytes-before.LiveBytes));
                assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
                assert(after.AllocCount-before.AllocCount == after.FreeCount-before.FreeCount);
                assert(before.InvalidFreeCount == after.InvalidFreeCount && before.DoubleFreeCount == after.DoubleFreeCount);
                assert(before.UseAfterFreeCount == after.UseAfterFreeCount);
            }
            if (!triggered) break;
        }
        assert(budget < 32);
    }
    checked_projection_host_fini();
    if (!smoke) {
        xrtMemDebugSnapshot(&after);
        assert(initial.LiveCount == after.LiveCount && initial.LiveBytes == after.LiveBytes);
        assert(initial.InvalidFreeCount == after.InvalidFreeCount && initial.DoubleFreeCount == after.DoubleFreeCount);
        assert(initial.UseAfterFreeCount == after.UseAfterFreeCount && faults >= 300);
        printf("checked projection: %u executions, %u allocation faults, %llu balanced allocations; null success, closures/strings, fields/index/cursor, catch/finally\n",
            executions, faults, (unsigned long long)(after.AllocCount-initial.AllocCount));
    } else puts("checked projection generated C-host smoke passed");
    return 0;
}
