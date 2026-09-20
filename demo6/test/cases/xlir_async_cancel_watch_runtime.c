#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_ERROR
#define XRT_MODULE_VALUE
#define XRT_MODULE_FUTURE
#define XRT_MODULE_FUTURE_CONTINUE
#include "lib/xrt/xrt.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "xlir_async_source_sync.h"
extern void cancel_watch_prepare(void), cancel_watch_finish(void);
extern xvalue* cancel_watch_new(int64_t);
extern xfuture* cancel_watch_launch(xvalue*, xvalue*, xfuture*, xfuture*, int);
extern void cancel_watch_produce(xpromise*, unsigned, int64_t*, xerror*);
static unsigned drops[3], sequence[2], sequenceCount, finallyCount, caughtCount, afterCount;
static unsigned executions, observations, cancellations;
static int raisingFinally, raisingDrop, smoke, ambient;
static xfuture* activeOutput;
static xerror *primary, *secondary;
static int64_t payload = 10;
int cancel_watch_dropped(int id)
{
    assert(id >= 1 && id <= 2 && !drops[id]); ++drops[id];
    assert(sequenceCount < 2); sequence[sequenceCount++] = (unsigned)id;
    if (activeOutput) assert(!xrtFutureDone(activeOutput));
    return raisingDrop;
}
int cancel_watch_finalized(void) { ++finallyCount; return raisingFinally; }
int cancel_watch_ambient(void) { return ambient; }
void cancel_watch_caught(void) { ++caughtCount; }
void cancel_watch_after(void) { ++afterCount; }
xfuture* cancel_watch_input(void)
{
    xfuture* future = NULL; xpromise* promise = xrtPromiseCreate(&future, NULL);
    assert(promise && future && xrtPromiseResolve(promise, &payload)); xrtPromiseDestroy(promise); return future;
}
static void observe(const xfutureresult* input, xpromise* output, void* data)
{
    (void)data; assert(input && drops[1] == 1 && drops[2] == 1);
    ++observations; assert(xrtPromiseResolve(output, &payload));
}
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after; xrtMemDebugSnapshot(&after);
    assert(after.LiveCount == before->LiveCount && after.LiveBytes == before->LiveBytes);
    assert(after.AllocCount-before->AllocCount == after.FreeCount-before->FreeCount);
    assert(after.DoubleFreeCount == before->DoubleFreeCount && after.InvalidFreeCount == before->InvalidFreeCount && after.UseAfterFreeCount == before->UseAfterFreeCount);
}
static void request_cancel(xfuture* result)
{
    assert(xrtFutureCancel(result)); ++cancellations;
    assert(!xrtFutureCancel(result) && !xrtFutureDone(result) && !xrtGetError());
}
static void run_case(unsigned mode, unsigned first, unsigned second, unsigned ready, int stage, int finalRaises, int dropRaises)
{
    xmemdebugsnapshot before; xfuture *input = NULL, *cleanup = NULL, *result, *observed;
    xpromise *promise, *cleanupPromise; xvalue *a, *b; xfutureresult outcome;
    unsigned effectiveFirst = stage == 0 ? 2 : first;
    unsigned effectiveSecond = stage == 1 ? 2 : second;
    unsigned effective = effectiveFirst, expectedFinally = mode >= 2 && effectiveSecond ? 0 : 1;
    unsigned beforeObserved = observations;
    int code = effective == 1 ? 88 : 0, internal = 0; int64_t expectedValue = 13;
    if (!smoke) xrtMemDebugSnapshot(&before);
    memset(drops, 0, sizeof(drops)); sequenceCount = 0; finallyCount = caughtCount = afterCount = 0;
    raisingFinally = finalRaises; raisingDrop = dropRaises;
    promise = xrtPromiseCreate(&input, NULL); cleanupPromise = xrtPromiseCreate(&cleanup, NULL); assert(promise && cleanupPromise);
    if (ready) cancel_watch_produce(cleanupPromise, second, &payload, secondary);
    a = cancel_watch_new(1); b = cancel_watch_new(2); assert(a && b);
    result = cancel_watch_launch(a, b, input, cleanup, (int)mode); assert(result && !xrtFutureDone(result));
    xrtValueRelease(a); xrtValueRelease(b); assert(!drops[1] && !drops[2] && !xrtGetError());
    activeOutput = result; observed = xrtFutureContinue(result, observe, NULL); assert(observed);
    if (stage == 0) request_cancel(result);
    cancel_watch_produce(promise, first, &payload, primary);
    if (mode >= 2 && !ready) {
        xlir_async_test_wait_watches(2);
        /* A parameter dead before this await may retire at the handoff. Its
         * timing is not a source lifetime guarantee; error isolation still is. */
        assert(!xrtFutureDone(result) && !finallyCount);
        if (stage == 1) request_cancel(result);
        cancel_watch_produce(cleanupPromise, second, &payload, secondary);
    }
    xlir_async_test_wait_retired(); activeOutput = NULL;
    if (mode == 1 && effectiveFirst == 2) { effective = 0; code = 0; expectedValue = 37; }
    if (mode >= 2 && effectiveSecond) { effective = effectiveSecond; code = effectiveSecond == 1 ? 99 : 0; }
    if (mode == 4 && expectedFinally && !finalRaises) { effective = 0; code = 0; expectedValue = 53; }
    if (expectedFinally && finalRaises) { effective = 1; code = finalRaises == 1 ? 77 : 0; internal = finalRaises == 2; }
    if (!effective && dropRaises) { effective = 1; code = dropRaises == 1 ? 92 : 0; internal = dropRaises == 2; }
    assert(xrtFutureResult(result, &outcome) && !xrtGetError());
    if (outcome.State != (effective == 0 ? XFUTURE_RESOLVED : effective == 1 ? XFUTURE_FAILED : effective == 2 ? XFUTURE_CANCELLED : XFUTURE_CLOSED))
        fprintf(stderr, "cancel watch: mode=%u first=%u second=%u ready=%u stage=%d finally=%d drop=%d state=%d effective=%u code=%d\n", mode, first, second, ready, stage, finalRaises, dropRaises, outcome.State, effective, outcome.Error ? xrtErrorCode(outcome.Error) : 0);
    assert(outcome.State == (effective == 0 ? XFUTURE_RESOLVED : effective == 1 ? XFUTURE_FAILED : effective == 2 ? XFUTURE_CANCELLED : XFUTURE_CLOSED));
    if (effective == 1) { assert(outcome.Error && !outcome.Value); if (internal) assert(xrtErrorKind(outcome.Error) == XERR_INTERNAL); else assert(xrtErrorCode(outcome.Error) == code); }
    else assert(!outcome.Error);
    if (!effective) assert(outcome.Value && *(int64_t*)outcome.Value == expectedValue);
    assert(finallyCount == expectedFinally && caughtCount == (effectiveFirst == 2 ? 1u : 0u) && afterCount == (effectiveFirst == 0 ? 1u : 0u));
    assert(sequenceCount == 2 && sequence[0] == 2 && sequence[1] == 1 && observations == beforeObserved + 1);
    assert(!xrtFutureCancel(result) && !xrtGetError() && xrtFutureState(observed) == XFUTURE_RESOLVED);
    xrtFutureDestroy(observed); xrtFutureDestroy(result); xrtPromiseDestroy(promise); xrtPromiseDestroy(cleanupPromise);
    xrtFutureDestroy(input); xrtFutureDestroy(cleanup); assert(!xrtGetError());
    if (!smoke) balanced(&before);
    ++executions;
}
int main(int argc, char** argv)
{
    unsigned round, mode, first, second, ready; int stage, finalRaises, dropRaises; xmemdebugsnapshot before, after;
    smoke = argc == 2 && !strcmp(argv[1], "--smoke"); assert(smoke || (argc == 1 && xrtMemDebugEnable(true)));
    if (!smoke) xrtMemDebugSnapshot(&before);
    primary = xrtErrorCreate(XERR_STATE, "cancel watch", 88, "first failed"); secondary = xrtErrorCreate(XERR_STATE, "cancel watch", 99, "cleanup await failed"); assert(primary && secondary);
    cancel_watch_prepare();
    for (ambient = 0; ambient < 3; ++ambient)
    for (round = 0; round < (smoke ? 1u : 5u); ++round) for (mode = 0; mode < 5; ++mode)
    for (first = 0; first < 4; ++first) for (second = 0; second < 4; ++second) for (ready = 0; ready < 2; ++ready)
    for (stage = -1; stage <= 1; ++stage) for (finalRaises = 0; finalRaises < 3; ++finalRaises) for (dropRaises = 0; dropRaises < 3; ++dropRaises) {
        if ((mode < 2 && (second || ready || stage == 1)) || (stage == 1 && ready)) continue;
        run_case(mode, first, second, ready, stage, finalRaises, dropRaises);
    }
    cancel_watch_finish(); xrtErrorFree(primary); xrtErrorFree(secondary);
    if (!smoke) { xrtMemDebugSnapshot(&after); balanced(&before);
        assert(executions == 34560 && cancellations == 20520 && observations == executions);
        printf("async cancel watch: %u executions, %u cancellation requests, %u publication observers, %llu balanced allocations; cancelled await/catch/rethrow/finally, once-only delivery across cleanup await, all input outcomes, raising cleanup, clean/diagnostic/diagnostic-free producer isolation\n", executions, cancellations, observations, (unsigned long long)(after.AllocCount-before.AllocCount));
    } else printf("async cancel watch C-host smoke: %u executions\n", executions);
    return 0;
}
