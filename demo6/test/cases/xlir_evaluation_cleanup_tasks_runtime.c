#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_ERROR
#include "lib/xrt/xrt.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
extern int evaluation_tasks_host_init(void);
extern void evaluation_tasks_host_fini(void);
static xerror* sentinel;
static int raising;
static unsigned dropped, after, caught;
void evaluation_task_drop(void) { ++dropped; if (raising) xrtSetError(sentinel); }
void evaluation_task_after(int64_t count) { assert(count == 2); ++after; }
void evaluation_task_caught(void) { ++caught; }
int main(int argc, char** argv)
{
    unsigned round, mode;
    int smoke = argc == 2 && strcmp(argv[1], "--smoke") == 0;
    xmemdebugsnapshot initial, before, final;
    assert(smoke || (argc == 1 && xrtMemDebugEnable(true)));
    if (!smoke) xrtMemDebugSnapshot(&initial);
    sentinel = xrtErrorCreate(XERR_STATE, "evaluation-task", 77, "operand cleanup failure");
    assert(sentinel);
    for (round = 0; round < (smoke ? 1u : 25u); ++round)
    for (mode = 0; mode < 2; ++mode) {
        if (!smoke) xrtMemDebugSnapshot(&before);
        dropped = after = caught = 0; raising = (int)mode;
        assert(evaluation_tasks_host_init());
        evaluation_tasks_host_fini();
        assert(!xrtGetError() && dropped == 1 && after == (mode ? 0u : 1u) && caught == mode);
        if (!smoke) {
            xrtMemDebugSnapshot(&final);
            assert(before.LiveCount == final.LiveCount && before.LiveBytes == final.LiveBytes);
            assert(final.AllocCount - before.AllocCount == final.FreeCount - before.FreeCount);
            assert(before.InvalidFreeCount == final.InvalidFreeCount && before.DoubleFreeCount == final.DoubleFreeCount);
            assert(before.UseAfterFreeCount == final.UseAfterFreeCount);
        }
    }
    xrtErrorFree(sentinel);
    if (!smoke) {
        xrtMemDebugSnapshot(&final);
        assert(initial.LiveCount == final.LiveCount && initial.LiveBytes == final.LiveBytes);
        printf("evaluation task operands: 50 module activations, %llu balanced allocations\n",
            (unsigned long long)(final.AllocCount - initial.AllocCount));
    } else puts("evaluation task operands: generated C-host smoke passed");
    return 0;
}
