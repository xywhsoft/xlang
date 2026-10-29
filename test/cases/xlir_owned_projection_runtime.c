#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_ERROR
#include "lib/xrt/xrt.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
extern int projection_host_init(void);
extern void projection_host_fini(void);
extern int64_t projection_host_case(int);
static unsigned budget, armed, dropped, observed, caught;
static xerror* sentinel;
static int smoke, raisingDrop;
void projection_arm(void)
{
    assert(!armed++);
    if (!smoke) assert(xrtMemDebugFailAfter(budget));
}
void projection_dropped(void) { ++dropped; if (raisingDrop) xrtSetError(sentinel); }
void projection_observe(int64_t count) { assert(count == 10); ++observed; }
void projection_raise(void) { assert(sentinel); xrtSetError(sentinel); }
void projection_caught(void) { ++caught; }
void projection_raise_on_drop(void) { raisingDrop = 1; }
int main(int argc, char** argv)
{
    unsigned round, kind, faults = 0;
    xmemdebugsnapshot initial, before, after;
    smoke = argc == 2 && strcmp(argv[1], "--smoke") == 0;
    assert(smoke || (argc == 1 && xrtMemDebugEnable(true)));
    if (!smoke) xrtMemDebugSnapshot(&initial);
    sentinel = xrtErrorCreate(XERR_STATE, "projection", 77, "case body failure");
    assert(sentinel);
    assert(projection_host_init());
    for (round = 0; round < (smoke ? 1u : 50u); ++round)
    for (kind = 0; kind < 16; ++kind) {
        for (budget = 0; budget < 32; ++budget) {
            int64_t result;
            int triggered;
            unsigned oldDrops = dropped;
            if (!smoke) xrtMemDebugSnapshot(&before);
            armed = 0; observed = 0; caught = 0; raisingDrop = 0;
            result = projection_host_case((int)kind);
            triggered = !smoke && xrtMemDebugFailTriggered();
            if (!smoke) xrtMemDebugFailClear();
            assert(armed == 1);
            assert(dropped - oldDrops == (kind < 2 ? 1u : kind >= 14 ? 2u : 0u));
            if (triggered) ++faults;
            if (kind == 13) assert(result == 17 && caught == 1 && !xrtGetError());
            else if (triggered) assert(result == 0 && xrtGetError());
            else if (kind == 15) assert(result == 0 && xrtGetError() && xrtErrorCode(xrtGetError()) == 77);
            else assert(result == (kind < 4 ? 5 : kind == 9 ? 20 : 10) && !xrtGetError());
            if (kind == 11 || kind == 12) assert(observed == (triggered ? 0u : 1u));
            xrtClearError();
            if (!smoke) {
                xrtMemDebugSnapshot(&after);
                if (before.LiveCount != after.LiveCount || before.LiveBytes != after.LiveBytes)
                    fprintf(stderr, "projection kind %u budget %u: live delta %lld, byte delta %lld\n",
                        kind, budget, (long long)(after.LiveCount - before.LiveCount),
                        (long long)(after.LiveBytes - before.LiveBytes));
                assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
                assert(after.AllocCount - before.AllocCount == after.FreeCount - before.FreeCount);
                assert(before.InvalidFreeCount == after.InvalidFreeCount && before.DoubleFreeCount == after.DoubleFreeCount);
                assert(before.UseAfterFreeCount == after.UseAfterFreeCount);
            }
            if (!triggered) break;
        }
        assert(budget < 32);
        if (smoke) printf("owned projection %u: generated C-host smoke passed\n", kind);
        else if (round == 49) printf("owned projection %u: %u allocation failures per sweep, success and rollback balanced\n", kind, budget);
    }
    projection_host_fini();
    xrtErrorFree(sentinel); sentinel = NULL;
    assert(!xrtGetError());
    if (!smoke) {
        xrtMemDebugSnapshot(&after);
        assert(initial.LiveCount == after.LiveCount && initial.LiveBytes == after.LiveBytes);
        assert(initial.InvalidFreeCount == after.InvalidFreeCount && initial.DoubleFreeCount == after.DoubleFreeCount);
        assert(initial.UseAfterFreeCount == after.UseAfterFreeCount && faults >= 1200);
        printf("owned projection: %u actual allocation failures, %llu balanced allocations\n",
            faults, (unsigned long long)(after.AllocCount - initial.AllocCount));
    }
    return 0;
}
