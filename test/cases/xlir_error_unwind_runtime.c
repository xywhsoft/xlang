#define XRT_MODULE_ERROR
#define XRT_MODULE_MEMORY_DEBUG
#include "lib/xrt/xrt.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int unwind_run_case(int, int, int);
void unwind_finish(void);
void unwind_pending_success(void);
static int faults;
void unwind_oom(void)
{
    xerror* error;
    assert(xrtMemDebugFailAfter(0));
    error = xrtErrorCreate(XERR_STATE, "unwind", 101, "failed diagnostic allocation");
    assert(error == NULL && xrtMemDebugFailTriggered());
    xrtMemDebugFailClear(); ++faults;
}
int main(int argc, char** argv)
{
    xmemdebugsnapshot before, after;
    int iteration, original, cleanup, lane, cases = 0;
    int smoke = argc == 2 && strcmp(argv[1], "--smoke") == 0;
    assert(smoke || (argc == 1 && xrtMemDebugEnable(true)));
    for (lane = 0; lane < 25; ++lane) if (!unwind_run_case(1, 0, lane)) return 1;
    unwind_pending_success();
    if (!smoke) xrtMemDebugSnapshot(&before);
    for (iteration = 0; iteration < 100; ++iteration) {
        for (original = 1; original <= (smoke ? 2 : 3); ++original)
            for (cleanup = 0; cleanup < 5; ++cleanup)
                for (lane = 0; lane < 25; ++lane) {
                    if (!unwind_run_case(original, cleanup >= 3 ? cleanup + 1 : cleanup, lane)) return 1;
                    ++cases;
                }
        unwind_pending_success(); ++cases;
    }
    if (!smoke) {
        xrtMemDebugSnapshot(&after);
        assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
        assert(after.AllocCount - before.AllocCount == after.FreeCount - before.FreeCount);
        assert(before.DoubleFreeCount == after.DoubleFreeCount && before.InvalidFreeCount == after.InvalidFreeCount);
        assert(before.UseAfterFreeCount == after.UseAfterFreeCount && faults == 12500);
        printf("error unwind: %d cases, %d real diagnostic allocation failures, %llu balanced allocations\n",
            cases, faults, (unsigned long long)(after.AllocCount - before.AllocCount));
    } else printf("error unwind smoke: %d cases, first failure, reverse cleanup and catch isolation passed\n", cases);
    unwind_finish(); return 0;
}
