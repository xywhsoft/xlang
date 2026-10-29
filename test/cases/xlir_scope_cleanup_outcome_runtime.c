#define XRT_MODULE_ERROR
#define XRT_MODULE_MEMORY_DEBUG
#include "lib/xrt/xrt.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

void scope_cleanup_case(int, int);
static int faults;
void scope_cleanup_oom(void)
{
    xerror* error;
    assert(xrtMemDebugFailAfter(0));
    error = xrtErrorCreate(XERR_STATE, "scope-cleanup", 0, "diagnostic allocation failed");
    assert(error == NULL && xrtMemDebugFailTriggered());
    xrtMemDebugFailClear(); ++faults;
}
int main(int argc, char** argv)
{
    int iteration, probe, fault, cases = 0;
    int smoke = argc == 2 && strcmp(argv[1], "--smoke") == 0;
    xmemdebugsnapshot before, after;
    assert(smoke || (argc == 1 && xrtMemDebugEnable(true)));
    scope_cleanup_case(0, 0);
    if (!smoke) xrtMemDebugSnapshot(&before);
    for (iteration = 0; iteration < 100; ++iteration)
        for (probe = 0; probe < 7; ++probe)
            for (fault = 0; fault < 6; ++fault) {
                if (smoke && fault == 3) continue;
                scope_cleanup_case(probe, fault); ++cases;
            }
    if (!smoke) {
        xrtMemDebugSnapshot(&after);
        assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
        assert(after.AllocCount - before.AllocCount == after.FreeCount - before.FreeCount);
        assert(before.InvalidFreeCount == after.InvalidFreeCount && before.DoubleFreeCount == after.DoubleFreeCount);
        assert(before.UseAfterFreeCount == after.UseAfterFreeCount && faults == 700);
        printf("scope cleanup: %d cases, %d real diagnostic allocation failures, %llu balanced allocations\n",
            cases, faults, (unsigned long long)(after.AllocCount - before.AllocCount));
    } else printf("scope cleanup smoke: %d cases passed\n", cases);
    return 0;
}
