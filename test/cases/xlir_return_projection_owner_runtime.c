#define XRT_MODULE_ERROR
#define XRT_MODULE_MEMORY_DEBUG
#include "lib/xrt/xrt.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
void return_projection_case(int, int);
static int failures;
void return_projection_oom(void)
{
    xerror* error;
    assert(xrtMemDebugFailAfter(0));
    error = xrtErrorCreate(XERR_STATE, "return-projection", 0, "diagnostic allocation failure");
    assert(error == NULL && xrtMemDebugFailTriggered()); xrtMemDebugFailClear(); ++failures;
}
int main(int argc, char** argv)
{
    int iteration, lane, mode, cases = 0;
    int smoke = argc == 2 && strcmp(argv[1], "--smoke") == 0;
    xmemdebugsnapshot before, after;
    assert(argc == 1 || smoke); assert(smoke || xrtMemDebugEnable(true));
    for (lane = 0; lane < 15; ++lane) return_projection_case(lane, 0);
    if (!smoke) xrtMemDebugSnapshot(&before);
    for (iteration = 0; iteration < 100; ++iteration)
        for (lane = 0; lane < 15; ++lane)
            for (mode = 0; mode < 7; ++mode) {
                if (smoke && (mode == 3 || mode == 6)) continue;
                return_projection_case(lane, mode); ++cases;
            }
    if (!smoke) {
        xrtMemDebugSnapshot(&after);
        assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
        assert(after.AllocCount - before.AllocCount == after.FreeCount - before.FreeCount);
        assert(before.InvalidFreeCount == after.InvalidFreeCount && before.DoubleFreeCount == after.DoubleFreeCount);
        assert(before.UseAfterFreeCount == after.UseAfterFreeCount && failures == 3000);
        printf("return projection owners: %d cases, %d real diagnostic allocation failures, %llu balanced allocations\n",
            cases, failures, (unsigned long long)(after.AllocCount - before.AllocCount));
    } else printf("return projection owner smoke: %d cases passed\n", cases);
    return 0;
}
