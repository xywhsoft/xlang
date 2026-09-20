#define XRT_MODULE_ERROR
#define XRT_MODULE_MEMORY_DEBUG
#include "lib/xrt/xrt.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int module_cleanup_case(int, int, int, int);
static int faults;
void module_cleanup_oom(void)
{
    xerror* error;
    assert(xrtMemDebugFailAfter(0));
    error = xrtErrorCreate(XERR_STATE, "module-cleanup", 0, "diagnostic allocation failure");
    assert(error == NULL && xrtMemDebugFailTriggered());
    xrtMemDebugFailClear(); ++faults;
}

int main(int argc, char** argv)
{
    xmemdebugsnapshot before, after;
    int iteration, pending, cleanup, reenter, init, cases = 0;
    int smoke = argc == 2 && strcmp(argv[1], "--smoke") == 0;
    assert(smoke || (argc == 1 && xrtMemDebugEnable(true)));
    module_cleanup_case(0, 0, 0, 0);
    module_cleanup_case(1, 0, 0, 0);
    if (!smoke) xrtMemDebugSnapshot(&before);
    for (iteration = 0; iteration < 100; ++iteration) {
        for (pending = 0; pending < 5; ++pending)
            for (cleanup = 0; cleanup < (smoke ? 3 : 4); ++cleanup)
                for (reenter = 0; reenter < 2; ++reenter) {
                    module_cleanup_case(0, pending, cleanup, reenter); ++cases;
                }
        for (init = 1; init < (smoke ? 3 : 4); ++init)
            for (pending = 0; pending <= 4; pending += 4)
                for (cleanup = 0; cleanup < (smoke ? 3 : 4); ++cleanup)
                    for (reenter = 0; reenter < 2; ++reenter) {
                        module_cleanup_case(init, pending, cleanup, reenter); ++cases;
                    }
    }
    if (!smoke) {
        xrtMemDebugSnapshot(&after);
        assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
        assert(after.AllocCount - before.AllocCount == after.FreeCount - before.FreeCount);
        assert(before.DoubleFreeCount == after.DoubleFreeCount && before.InvalidFreeCount == after.InvalidFreeCount);
        assert(before.UseAfterFreeCount == after.UseAfterFreeCount && faults == 6800);
        printf("module cleanup: %d cases, %d real diagnostic allocation failures, %llu balanced allocations\n",
            cases, faults, (unsigned long long)(after.AllocCount - before.AllocCount));
    } else printf("module cleanup smoke: %d cases passed\n", cases);
    return 0;
}
