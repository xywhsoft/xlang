#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_ERROR
#include "lib/xrt/xrt.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
extern void product_await_prepare(void);
extern void product_await_finish(void);
extern unsigned product_await_cycle(unsigned, unsigned, int, int);
int main(int argc, char** argv)
{
    static const int drops[] = {0,10,20};
    unsigned round, first, second, drop, mode, executions = 0, suspensions = 0;
    int smoke = argc == 2 && !strcmp(argv[1], "--smoke");
    xmemdebugsnapshot initial, before, after;
    assert(smoke || (argc == 1 && xrtMemDebugEnable(true)));
    if (!smoke) xrtMemDebugSnapshot(&initial);
    product_await_prepare();
    for (round = 0; round < (smoke ? 1u : 25u); ++round)
    for (first = 0; first < 4; ++first)
    for (second = 0; second < 4; ++second)
    for (drop = 0; drop < 3; ++drop)
    for (mode = 0; mode < 2; ++mode) {
        if (!smoke) xrtMemDebugSnapshot(&before);
        suspensions += product_await_cycle(first, second, drops[drop], (int)mode); ++executions;
        if (!smoke) {
            xrtMemDebugSnapshot(&after);
            assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
            assert(after.AllocCount-before.AllocCount == after.FreeCount-before.FreeCount);
            assert(before.InvalidFreeCount == after.InvalidFreeCount && before.DoubleFreeCount == after.DoubleFreeCount);
            assert(before.UseAfterFreeCount == after.UseAfterFreeCount);
        }
    }
    product_await_finish();
    if (!smoke) {
        xrtMemDebugSnapshot(&after);
        assert(initial.LiveCount == after.LiveCount && initial.LiveBytes == after.LiveBytes);
        assert(after.AllocCount-initial.AllocCount == after.FreeCount-initial.FreeCount);
        printf("product await owner: %u executions, %u genuine suspensions, %llu balanced allocations; two ordered targets, partial commit, reject/cancel/close, original failure and diagnostic-free cleanup\n",
            executions, suspensions, (unsigned long long)(after.AllocCount-initial.AllocCount));
    } else puts("product await owner generated C-host smoke passed");
    return 0;
}
