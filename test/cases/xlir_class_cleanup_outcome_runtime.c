#define XRT_MODULE_ERROR
#define XRT_MODULE_MEMORY_DEBUG
#include "lib/xrt/xrt.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

void class_cleanup_case(int, int, int);
int class_cleanup_local_probe(int);
static int faults;
void class_cleanup_oom(void)
{
    xerror* error;
    assert(xrtMemDebugFailAfter(0));
    error = xrtErrorCreate(XERR_STATE, "class-cleanup", 0, "diagnostic allocation failed");
    assert(error == NULL && xrtMemDebugFailTriggered());
    xrtMemDebugFailClear(); ++faults;
}
int main(int argc, char** argv)
{
    int iteration, pending, mode, reentry, cases = 0;
    int smoke = argc == 2 && strcmp(argv[1], "--smoke") == 0;
    xmemdebugsnapshot before, after;
    if (argc == 2 && strcmp(argv[1], "--probe-scopes") == 0)
        return class_cleanup_local_probe(0) && class_cleanup_local_probe(1) ? 0 : 1;
    assert(smoke || (argc == 1 && xrtMemDebugEnable(true)));
    class_cleanup_case(0, 0, 0);
    if (!smoke) xrtMemDebugSnapshot(&before);
    for (iteration = 0; iteration < 100; ++iteration)
        for (pending = 0; pending < 5; ++pending)
            for (mode = 0; mode < 6; ++mode)
                for (reentry = 0; reentry < 2; ++reentry) {
                    if (smoke && mode == 3) continue;
                    class_cleanup_case(pending, mode, reentry); ++cases;
                }
    if (!smoke) {
        xrtMemDebugSnapshot(&after);
        assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
        assert(after.AllocCount - before.AllocCount == after.FreeCount - before.FreeCount);
        assert(before.InvalidFreeCount == after.InvalidFreeCount && before.DoubleFreeCount == after.DoubleFreeCount);
        assert(before.UseAfterFreeCount == after.UseAfterFreeCount && faults == 1000);
        printf("class cleanup: %d cases, %d real diagnostic allocation failures, %llu balanced allocations\n",
            cases, faults, (unsigned long long)(after.AllocCount - before.AllocCount));
    } else printf("class cleanup smoke: %d cases passed\n", cases);
    return 0;
}
