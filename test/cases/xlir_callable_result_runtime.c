#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_ERROR
#include "lib/xrt/xrt.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
extern int result_init(void);
extern void result_language_check(void);
extern void result_prepare(unsigned);
extern bool result_run(unsigned);
extern void result_finish(unsigned, bool);
extern void result_fini(void);
static void balanced(const xmemdebugsnapshot* before, unsigned kind, unsigned budget)
{
    xmemdebugsnapshot after; xrtMemDebugSnapshot(&after);
    if (before->LiveCount != after.LiveCount || before->LiveBytes != after.LiveBytes)
        fprintf(stderr, "callable result kind=%u budget=%u live=%lld bytes=%lld\n", kind, budget,
            (long long)(after.LiveCount - before->LiveCount), (long long)(after.LiveBytes - before->LiveBytes));
    assert(before->LiveCount == after.LiveCount && before->LiveBytes == after.LiveBytes);
    assert(after.AllocCount - before->AllocCount == after.FreeCount - before->FreeCount);
    assert(before->InvalidFreeCount == after.InvalidFreeCount && before->DoubleFreeCount == after.DoubleFreeCount);
    assert(before->UseAfterFreeCount == after.UseAfterFreeCount);
}
int main(int argc, char** argv)
{
    int smoke = argc == 2 && !strcmp(argv[1], "--smoke");
    unsigned round, kind, budget, faults = 0, executions = 0;
    xmemdebugsnapshot initial, before, final;
    assert(smoke || (argc == 1 && xrtMemDebugEnable(true)));
    if (!smoke) xrtMemDebugSnapshot(&initial);
    assert(result_init()); result_language_check();
    for (round = 0; round < (smoke ? 1u : 20u); ++round)
    for (kind = 0; kind < 10; ++kind) {
        for (budget = 0; budget < 48; ++budget) {
            bool ok; int triggered;
            if (!smoke) xrtMemDebugSnapshot(&before);
            result_prepare(kind);
            if (!smoke) assert(xrtMemDebugFailAfter(budget));
            ok = result_run(kind);
            triggered = !smoke && xrtMemDebugFailTriggered();
            if (!smoke) xrtMemDebugFailClear();
            ++executions;
            if (triggered) { ++faults; assert(!ok && xrtGetError()); }
            else if (kind == 0 || kind == 3 || kind == 9) assert(ok && !xrtGetError());
            else assert(!ok && xrtGetError());
            if (kind == 1 || kind == 2 || (!ok && kind >= 4 && kind <= 8)) {
                const xerror* error = xrtGetError(); assert(error);
                while (xrtErrorCause(error)) error = xrtErrorCause(error);
                if (kind == 1 || kind == 2) assert(xrtErrorCode(error) == 88);
                else assert(xrtErrorCode(error) != 77 && xrtErrorCode(error) != 99);
            }
            xrtClearError(); result_finish(kind, ok); assert(!xrtGetError());
            if (!smoke) balanced(&before, kind, budget);
            if (!triggered) break;
        }
        assert(budget < 48);
    }
    result_fini(); assert(!xrtGetError());
    if (!smoke) {
        balanced(&initial, 99, 0); xrtMemDebugSnapshot(&final);
        printf("callable result: %u executions, %u allocation faults, %llu balanced allocations; product/closure, partial decoding/boxing, overflow, primary errors, detached frames, transactional commit\n",
            executions, faults, (unsigned long long)(final.AllocCount - initial.AllocCount));
    } else puts("callable result generated C-host smoke passed");
    return 0;
}
