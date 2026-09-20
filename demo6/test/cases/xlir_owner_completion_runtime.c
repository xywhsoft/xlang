#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_ERROR
#define XRT_MODULE_FUTURE
#include "lib/xrt/xrt.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
extern int owner_completion_init(void);
extern void owner_completion_fini(void);
extern void owner_completion_cycle(int, int);
extern void owner_completion_fail(void);
xfuture* owner_completion_future(void)
{
    static int64_t payload = 41;
    xfuture* result = NULL;
    xpromise* promise = xrtPromiseCreate(&result, NULL);
    assert(promise && result && xrtPromiseResolve(promise, &payload));
    xrtPromiseDestroy(promise); owner_completion_fail(); return result;
}
int main(int argc, char** argv)
{
    unsigned round, mode, kind, executions = 0;
    int smoke = argc == 2 && !strcmp(argv[1], "--smoke");
    xmemdebugsnapshot initial, before, after;
    assert(smoke || (argc == 1 && xrtMemDebugEnable(true)));
    if (!smoke) xrtMemDebugSnapshot(&initial);
    assert(owner_completion_init());
    for (round = 0; round < (smoke ? 1u : 50u); ++round)
    for (mode = 0; mode < 2; ++mode)
    for (kind = 0; kind < 8; ++kind) {
        if (!smoke) xrtMemDebugSnapshot(&before);
        owner_completion_cycle((int)mode, (int)kind); ++executions;
        if (!smoke) {
            xrtMemDebugSnapshot(&after);
            if (before.LiveCount != after.LiveCount || before.LiveBytes != after.LiveBytes)
                fprintf(stderr, "owner completion kind=%u mode=%u live=%lld bytes=%lld\n", kind, mode,
                    (long long)(after.LiveCount-before.LiveCount), (long long)(after.LiveBytes-before.LiveBytes));
            assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
            assert(after.AllocCount-before.AllocCount == after.FreeCount-before.FreeCount);
            assert(before.InvalidFreeCount == after.InvalidFreeCount && before.DoubleFreeCount == after.DoubleFreeCount);
            assert(before.UseAfterFreeCount == after.UseAfterFreeCount);
        }
    }
    owner_completion_fini(); assert(!xrtGetError());
    if (!smoke) {
        xrtMemDebugSnapshot(&after);
        assert(initial.LiveCount == after.LiveCount && initial.LiveBytes == after.LiveBytes);
        assert(after.AllocCount-initial.AllocCount == after.FreeCount-initial.FreeCount);
        printf("owner completion: %u executions, %llu balanced allocations; partial construction, native failure payloads, first failure bit, raising destructor, catch/finally\n",
            executions, (unsigned long long)(after.AllocCount-initial.AllocCount));
    } else puts("owner completion generated C-host smoke passed");
    return 0;
}
