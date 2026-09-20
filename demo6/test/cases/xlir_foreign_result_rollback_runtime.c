#define XRT_MODULE_ERROR
#define XRT_MODULE_MEMORY_DEBUG
#include "lib/xrt/xrt.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

void rollback_run_case(int, int, int, int, int);
void rollback_finish(void);
void rollback_set_lane(int);
int rollback_export_missing_failure_probe(void);
static int allocation_failures;

void rollback_inject_error_oom(void)
{
    xerror* error;
    assert(xrtMemDebugFailAfter(0));
    error = xrtErrorCreate(XERR_STATE, "factory", 101, "original error allocation failure");
    assert(error == NULL && xrtMemDebugFailTriggered());
    xrtMemDebugFailClear();
    ++allocation_failures;
}

int main(int argc, char** argv)
{
    xmemdebugsnapshot before, after;
    int lane, iteration, original, cleanup, empty, escape, cases = 0;
    int smoke = argc == 2 && strcmp(argv[1], "--smoke") == 0;
    if (argc == 2 && strcmp(argv[1], "--export-probe") == 0)
        return rollback_export_missing_failure_probe();
    assert(smoke || (argc == 1 && xrtMemDebugEnable(true)));
    for (lane = 0; lane < 3; ++lane) {
        rollback_set_lane(lane);
        rollback_run_case(0, 0, 0, 0, 0); /* Warm module/type/context singletons. */
        rollback_run_case(1, 3, 0, 0, 0);
    }
    if (!smoke) xrtMemDebugSnapshot(&before);
    for (lane = 0; lane < 3; ++lane) {
      rollback_set_lane(lane);
      for (iteration = 0; iteration < 100; ++iteration) {
        rollback_run_case(4, 0, 0, 0, 0); ++cases;
        if (lane == 0) { rollback_run_case(4, 0, 0, 0, 1); ++cases; }
        for (original = 1; original <= (smoke ? 2 : 3); ++original)
            for (cleanup = 0; cleanup <= 4; ++cleanup)
                for (empty = 0; empty <= 1; ++empty)
                    for (escape = 0; escape <= (lane == 0 ? 1 : 0); ++escape) {
                        rollback_run_case(original, cleanup, empty, 0, escape); ++cases;
                    }
        for (empty = 0; empty <= 1; ++empty) {
            rollback_run_case(0, 0, empty, 0, 0); ++cases;
            rollback_run_case(0, 0, empty, 1, 0); ++cases;
            rollback_run_case(1, 0, empty, 1, 0); ++cases;
            rollback_run_case(2, 0, empty, 1, 0); ++cases;
        }
      }
    }
    if (!smoke) {
        xrtMemDebugSnapshot(&after);
        assert(before.Enabled && after.Enabled);
        assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
        assert(after.AllocCount - before.AllocCount == after.FreeCount - before.FreeCount);
        assert(before.DoubleFreeCount == after.DoubleFreeCount);
        assert(before.InvalidFreeCount == after.InvalidFreeCount);
        assert(before.UseAfterFreeCount == after.UseAfterFreeCount);
        assert(allocation_failures == 4000);
        printf("foreign result rollback: %d cases, %d real diagnostic allocation failures, %llu balanced allocations\n",
            cases, allocation_failures, (unsigned long long)(after.AllocCount - before.AllocCount));
    } else {
        printf("foreign result rollback smoke: %d cases, original error priority, nested scopes and reentry passed\n", cases);
    }
    rollback_finish();
    return 0;
}
