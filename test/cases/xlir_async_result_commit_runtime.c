#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_ERROR
#define XRT_MODULE_FUTURE
#include "lib/xrt/xrt.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "xlir_async_source_sync.h"
extern void result_commit_prepare(void), result_commit_finish(void);
extern xfuture* result_commit_launch(unsigned, xfuture*, bool, xerror*);
extern void result_commit_check(unsigned, xfuture*, xfuture*, xerror*);
extern void result_commit_complete(xpromise*, int64_t*);
static int faultIndex = -1, raisingDrop, smoke;
static unsigned drops, arms, executions, faults, rejected, launchFailures;
static xmemdebugsnapshot armedAt;
static xerror* seed;
static int64_t payload = 10;
void result_commit_arm(void)
{
    ++arms;
    if (!smoke) xrtMemDebugSnapshot(&armedAt);
    if (faultIndex >= 0) assert(xrtMemDebugFailAfter((uint64)faultIndex));
}
int result_commit_dropped(void) { ++drops; return raisingDrop; }
xfuture* result_commit_input(void)
{
    xfuture* future = NULL; xpromise* promise = xrtPromiseCreate(&future, NULL);
    assert(promise && future && xrtPromiseResolve(promise, &payload)); xrtPromiseDestroy(promise); return future;
}
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after; xrtMemDebugSnapshot(&after);
    assert(after.LiveCount == before->LiveCount && after.LiveBytes == before->LiveBytes);
    assert(after.AllocCount-before->AllocCount == after.FreeCount-before->FreeCount);
    assert(after.DoubleFreeCount == before->DoubleFreeCount && after.InvalidFreeCount == before->InvalidFreeCount && after.UseAfterFreeCount == before->UseAfterFreeCount);
}
static unsigned run_case(unsigned kind, unsigned flow, int fault, int raising)
{
    xmemdebugsnapshot before, after; unsigned beforeDrops = drops, beforeArms = arms, allocations = 0;
    xfuture *input = NULL, *result; xpromise* promise; xfutureresult outcome;
    if (!smoke) xrtMemDebugSnapshot(&before);
    promise = xrtPromiseCreate(&input, NULL); assert(promise && input);
    if (flow == 1) assert(xrtPromiseResolve(promise, &payload));
    faultIndex = fault; raisingDrop = raising;
    result = result_commit_launch(kind, input, flow == 0, seed);
    if (flow == 2) { assert(result && !xrtFutureDone(result)); result_commit_complete(promise, &payload); }
    xlir_async_test_wait_retired();
    assert(arms == beforeArms + 1);
    if (fault >= 0) { assert(xlir_async_test_take_fault()); ++faults; }
    else if (!smoke) { xrtMemDebugSnapshot(&after); allocations = (unsigned)(after.AllocCount-armedAt.AllocCount); assert(allocations > 0 && allocations < 16); }
    if (!result) {
        assert(fault >= 0 && flow == 0 && xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_MEMORY);
        xrtClearError(); ++launchFailures;
    } else {
        assert(xrtFutureResult(result, &outcome));
        if (fault >= 0) {
            if (outcome.State != XFUTURE_FAILED || !outcome.Error || xrtErrorKind(outcome.Error) != XERR_MEMORY)
                fprintf(stderr, "result commit: kind=%u flow=%u fault=%d raising=%d state=%d code=%d\n", kind, flow, fault, raising, (int)outcome.State, outcome.Error ? xrtErrorCode(outcome.Error) : 0);
            assert(outcome.State == XFUTURE_FAILED && outcome.Error && xrtErrorKind(outcome.Error) == XERR_MEMORY && outcome.Value == NULL);
            ++rejected;
        } else {
            assert(outcome.State == XFUTURE_RESOLVED && !outcome.Error);
            result_commit_check(kind, result, input, seed);
            assert(drops == beforeDrops);
        }
        assert(!xrtGetError()); xrtFutureDestroy(result);
    }
    raisingDrop = 0; faultIndex = -1;
    assert(drops == beforeDrops + (kind == 2 || kind == 3 ? 1u : 0u));
    xrtPromiseDestroy(promise); xrtFutureDestroy(input); assert(!xrtGetError());
    if (!smoke) balanced(&before);
    ++executions; return allocations;
}
int main(int argc, char** argv)
{
    unsigned round, kind, flow, failure, raising, allocations[5][3]; xmemdebugsnapshot initial, after;
    smoke = argc == 2 && !strcmp(argv[1], "--smoke"); assert(smoke || (argc == 1 && xrtMemDebugEnable(true)));
    if (!smoke) xrtMemDebugSnapshot(&initial);
    seed = xrtErrorCreate(XERR_STATE, "result commit", 73, "error as data"); assert(seed);
    result_commit_prepare();
    for (kind = 0; kind < 5; ++kind) for (flow = 0; flow < 3; ++flow) {
        allocations[kind][flow] = run_case(kind, flow, -1, 0);
        if (!smoke) assert(allocations[kind][flow] == (flow ? 1u : 3u));
        if (!smoke) printf("result commit baseline: kind=%u flow=%u post-arm allocations=%u\n", kind, flow, allocations[kind][flow]);
    }
    if (!smoke) for (round = 0; round < 50; ++round)
    for (kind = 0; kind < 5; ++kind) for (flow = 0; flow < 3; ++flow)
    for (failure = 0; failure < allocations[kind][flow]; ++failure)
    for (raising = 0; raising < (kind == 2 || kind == 3 ? 3u : 1u); ++raising)
        run_case(kind, flow, (int)failure, (int)raising);
    result_commit_finish(); xrtErrorFree(seed); assert(!xrtGetError());
    if (!smoke) {
        assert(executions == 2265 && faults == 2250 && rejected == 1350 && launchFailures == 900);
        xrtMemDebugSnapshot(&after); balanced(&initial);
        printf("async result commit: %u executions, %u real allocation failures, %u failed Futures, %u pre-Promise launch failures, %llu balanced allocations; five payload ABIs, early/ready/pending, diagnostic/diagnostic-free raising destructors, isolated producer\n",
            executions, faults, rejected, launchFailures, (unsigned long long)(after.AllocCount-initial.AllocCount));
    } else printf("async result commit C-host smoke: %u executions\n", executions);
    return 0;
}
