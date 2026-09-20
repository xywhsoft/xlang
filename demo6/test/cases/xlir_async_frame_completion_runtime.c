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
extern void frame_completion_prepare(void), frame_completion_finish(void);
extern xvalue* frame_completion_new(int64_t);
extern xfuture* frame_completion_launch(xvalue*, xvalue*, xfuture*, bool);
extern void frame_completion_produce(xpromise*, int64_t*);
static xvalue *heldFirst, *heldSecond;
static xfuture* activeOutput;
static int raises[4], failMode, smoke, faultIndex = -1;
static unsigned drops[4], sequence[3], sequenceCount, executions, observations, faults, launchFailures;
static int64_t payload = 10;
int frame_completion_dropped(int id)
{
    assert(id >= 1 && id <= 3 && !drops[id]); ++drops[id];
    assert(sequenceCount < 3); sequence[sequenceCount++] = (unsigned)id;
    if (activeOutput) assert(!xrtFutureDone(activeOutput));
    return raises[id];
}
int frame_completion_failure(void) { return failMode; }
void frame_completion_arm(void)
{
    assert(heldFirst && heldSecond);
    xrtValueRelease(heldFirst); heldFirst = NULL;
    xrtValueRelease(heldSecond); heldSecond = NULL;
    assert(!drops[1] && !drops[2] && !xrtGetError());
    if (faultIndex >= 0) assert(xrtMemDebugFailAfter((uint64)faultIndex));
}
xfuture* frame_completion_input(void)
{
    xfuture* future = NULL; xpromise* promise = xrtPromiseCreate(&future, NULL);
    assert(promise && future && xrtPromiseResolve(promise, &payload)); xrtPromiseDestroy(promise); return future;
}
static void observe(const xfutureresult* input, xpromise* promise, void* data)
{
    (void)data; assert(input && drops[1] == 1 && drops[2] == 1);
    if (input->State != XFUTURE_RESOLVED) assert(drops[3] == 1);
    ++observations; assert(xrtPromiseResolve(promise, &payload));
}
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after; xrtMemDebugSnapshot(&after);
    assert(after.LiveCount == before->LiveCount && after.LiveBytes == before->LiveBytes);
    assert(after.AllocCount-before->AllocCount == after.FreeCount-before->FreeCount);
    assert(after.DoubleFreeCount == before->DoubleFreeCount && after.InvalidFreeCount == before->InvalidFreeCount && after.UseAfterFreeCount == before->UseAfterFreeCount);
}
static void run_case(unsigned flow, unsigned mode, unsigned dropMode, int fault)
{
    static const int configs[6][3] = {{0,0,0},{1,1,1},{1,2,1},{2,1,2},{1,0,2},{2,0,1}};
    xmemdebugsnapshot before; xfuture *input = NULL, *result, *observed = NULL;
    xpromise* promise; xfutureresult outcome; unsigned i, oldObservations = observations;
    if (!smoke) xrtMemDebugSnapshot(&before);
    memset(drops, 0, sizeof(drops)); sequenceCount = 0; failMode = (int)mode; faultIndex = fault;
    for (i = 1; i < 4; ++i) raises[i] = configs[dropMode][i-1];
    promise = xrtPromiseCreate(&input, NULL); assert(promise && input);
    if (flow == 1) assert(xrtPromiseResolve(promise, &payload));
    heldFirst = frame_completion_new(1); heldSecond = frame_completion_new(2); assert(heldFirst && heldSecond);
    result = frame_completion_launch(heldFirst, heldSecond, input, flow == 0);
    if (flow == 2) {
        assert(!xrtFutureDone(result)); activeOutput = result;
        observed = xrtFutureContinue(result, observe, NULL); assert(observed);
        frame_completion_produce(promise, &payload);
        xlir_async_test_wait_retired(); activeOutput = NULL;
        assert(observations == oldObservations + 1 && xrtFutureState(observed) == XFUTURE_RESOLVED);
    }
    xlir_async_test_wait_retired();
    if (fault >= 0) { assert(xlir_async_test_take_fault()); ++faults; }
    assert(!heldFirst && !heldSecond && drops[1] == 1 && drops[2] == 1);
    if (!result) {
        assert(flow == 0 && fault >= 0 && fault < 2 && xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_MEMORY);
        xrtClearError(); ++launchFailures;
    } else {
    assert(xrtFutureResult(result, &outcome) && !xrtGetError());
    if (fault >= 0) {
        assert(outcome.State == XFUTURE_FAILED && outcome.Error && xrtErrorKind(outcome.Error) == XERR_MEMORY && !outcome.Value);
    } else if (mode == 3 || mode == 4) {
        assert(outcome.State == (mode == 3 ? XFUTURE_CANCELLED : XFUTURE_CLOSED));
    } else if (mode || dropMode) {
        unsigned firstDrop = configs[dropMode][1] ? 2u : 1u;
        int noDiagnostic = mode == 2 || (!mode && raises[firstDrop] == 2);
        assert(outcome.State == XFUTURE_FAILED && outcome.Error && !outcome.Value);
        if (noDiagnostic) assert(xrtErrorKind(outcome.Error) == XERR_INTERNAL);
        else assert(xrtErrorCode(outcome.Error) == (mode ? 73 : 90 + (int)firstDrop));
    } else assert(outcome.State == XFUTURE_RESOLVED && outcome.Value && !outcome.Error && !drops[3]);
    }
    if (mode || fault >= 0) assert(sequenceCount == 3 && sequence[0] == 3 && sequence[1] == 2 && sequence[2] == 1);
    else assert(sequence[0] == 2 && sequence[1] == 1);
    memset(raises, 0, sizeof(raises));
    if (result) xrtFutureDestroy(result);
    assert(drops[3] == 1);
    if (observed) xrtFutureDestroy(observed);
    xrtPromiseDestroy(promise); xrtFutureDestroy(input); assert(!xrtGetError());
    if (!smoke) { balanced(&before); } ++executions;
}
int main(int argc, char** argv)
{
    unsigned round, flow, mode, dropMode, fault; xmemdebugsnapshot before, after;
    smoke = argc == 2 && !strcmp(argv[1], "--smoke"); assert(smoke || (argc == 1 && xrtMemDebugEnable(true)));
    if (!smoke) { xrtMemDebugSnapshot(&before); } frame_completion_prepare();
    for (round = 0; round < (smoke ? 1u : 40u); ++round)
    for (flow = 0; flow < 3; ++flow) for (mode = 0; mode < 5; ++mode) for (dropMode = 0; dropMode < 6; ++dropMode)
        run_case(flow, mode, dropMode, -1);
    if (!smoke) for (round = 0; round < 40; ++round) for (flow = 0; flow < 3; ++flow)
    for (dropMode = 0; dropMode < 6; ++dropMode) for (fault = 0; fault < (flow ? 1u : 3u); ++fault)
        run_case(flow, 0, dropMode, (int)fault);
    frame_completion_finish();
    if (!smoke) { xrtMemDebugSnapshot(&after); balanced(&before);
        assert(executions == 4800 && faults == 1200 && launchFailures == 480 && observations == 1440);
        printf("async frame completion: %u executions, %u publication observers, %u real allocation failures (%u pre-Promise launch failures), %llu balanced allocations; early/ready/pending, RETURN/THROW/cancel/closed, ordered retirement, diagnostic-free first failure, producer isolation\n", executions, observations, faults, launchFailures, (unsigned long long)(after.AllocCount-before.AllocCount));
    } else printf("async frame completion C-host smoke: %u executions\n", executions);
    return 0;
}
