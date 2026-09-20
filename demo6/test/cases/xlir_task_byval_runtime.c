#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_ERROR
#define XRT_MODULE_SEM
#define XRT_MODULE_ATOMIC
#include "lib/xrt/xrt.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
extern int task_byval_init(void);
extern void task_byval_fini(void);
static xsem* proceed;
static xatomic32 entered = {0};
static bool report_live(const xmemdebugallocation* allocation, void* user)
{
    (void)user;
    fprintf(stderr, "task byval live: %llu bytes at %s:%u\n",
        (unsigned long long)allocation->Size,
        allocation->File ? allocation->File : "?", allocation->Line);
    return true;
}
/* The caller publishes this barrier after spawn operand cleanup. Thus this
 * test isolates consuming parameter slots from the separately failing XRT
 * concurrent nonfinal-release contract; it does not certify that contract. */
void task_byval_enter(void)
{
    xrtAtomic32FetchAdd(&entered, 1, XMEMORY_SEQ_CST);
    assert(xrtSemWaitFor(proceed, 15000000) == XWAIT_OK);
}
void task_byval_release(void) { assert(xrtSemPost(proceed)); }
int main(int argc, char** argv)
{
    int smoke = argc == 2 && !strcmp(argv[1], "--smoke");
    unsigned round; xmemdebugsnapshot before, after;
    assert(smoke || (argc == 1 && xrtMemDebugEnable(true)));
    if (!smoke) xrtMemDebugSnapshot(&before);
    proceed = xrtSemCreate(0, 1); assert(proceed);
    for (round = 0; round < (smoke ? 1u : 30u); ++round) {
        int ok = task_byval_init();
        if (!ok && xrtGetError()) fprintf(stderr, "task byval: %s\n", xrtErrorMessage(xrtGetError()));
        assert(ok); task_byval_fini(); assert(!xrtGetError());
    }
    assert(xrtAtomic32Load(&entered, XMEMORY_SEQ_CST) == (uint32_t)(8 * round));
    xrtSemDestroy(proceed);
    if (!smoke) {
        xrtMemDebugSnapshot(&after);
        if (before.LiveCount != after.LiveCount || before.LiveBytes != after.LiveBytes) {
            fprintf(stderr, "task byval balance: live=%llu bytes=%llu allocations=%llu frees=%llu\n",
                (unsigned long long)(after.LiveCount - before.LiveCount),
                (unsigned long long)(after.LiveBytes - before.LiveBytes),
                (unsigned long long)(after.AllocCount - before.AllocCount),
                (unsigned long long)(after.FreeCount - before.FreeCount));
            xrtMemDebugVisitLive(report_live, NULL);
        }
        assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
        assert(after.AllocCount - before.AllocCount == after.FreeCount - before.FreeCount);
        assert(before.InvalidFreeCount == after.InvalidFreeCount && before.DoubleFreeCount == after.DoubleFreeCount);
        assert(before.UseAfterFreeCount == after.UseAfterFreeCount);
        printf("task byval: %u module rounds, %u consuming invocations, %llu balanced allocations\n", round, 8 * round,
            (unsigned long long)(after.AllocCount - before.AllocCount));
    } else puts("task byval generated C-host smoke passed");
    return 0;
}
