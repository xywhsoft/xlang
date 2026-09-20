#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_ERROR
#define XRT_MODULE_FUTURE
#define XRT_MODULE_VALUE
#define XRT_MODULE_ATOMIC
#define XRT_MODULE_SEM
#include "lib/xrt/xrt.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
extern int pool_outcome_init(void);
extern void pool_outcome_fini(void);
extern xfuture* pool_outcome_start(unsigned, int64_t);
extern unsigned pool_outcome_ready_watch_probe(void);
extern unsigned pool_outcome_watch_size(void);
static xsem *entered, *proceed;
static xatomic32 drops = {0}, calls = {0};
static int raiseDrop;
void pool_outcome_drop(void)
{
    xrtAtomic32FetchAdd(&drops, 1, XMEMORY_SEQ_CST);
    if (raiseDrop) xrtSetErrorKind(XERR_STATE);
}
int64_t pool_outcome_value(int64_t mode)
{
    xrtAtomic32FetchAdd(&calls, 1, XMEMORY_SEQ_CST);
    if (mode == 4) {
        assert(xrtSemPost(entered));
        assert(xrtSemWaitFor(proceed, 15000000) == XWAIT_OK);
    } else if (mode != 0) xrtSetErrorKind(mode == 1 ? XERR_STATE : mode == 2 ? XERR_CANCELLED : XERR_CLOSED);
    return 10;
}
static unsigned drop_count(void) { return xrtAtomic32Load(&drops, XMEMORY_SEQ_CST); }
static unsigned call_count(void) { return xrtAtomic32Load(&calls, XMEMORY_SEQ_CST); }
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after; xrtMemDebugSnapshot(&after);
    if (before->LiveCount != after.LiveCount || before->LiveBytes != after.LiveBytes)
        fprintf(stderr, "pool outcome live %llu/%llu -> %llu/%llu\n", (unsigned long long)before->LiveCount,
            (unsigned long long)before->LiveBytes, (unsigned long long)after.LiveCount, (unsigned long long)after.LiveBytes);
    assert(before->LiveCount == after.LiveCount && before->LiveBytes == after.LiveBytes);
    assert(after.AllocCount - before->AllocCount == after.FreeCount - before->FreeCount);
    assert(before->InvalidFreeCount == after.InvalidFreeCount && before->DoubleFreeCount == after.DoubleFreeCount);
    assert(before->UseAfterFreeCount == after.UseAfterFreeCount);
}
int main(int argc, char** argv)
{
    unsigned round, kind, mode, executions = 0, cancels = 0, faults = 0;
    int smoke = argc == 2 && strcmp(argv[1], "--smoke") == 0;
    xmemdebugsnapshot initial, before, final;
    assert(smoke || (argc == 1 && xrtMemDebugEnable(true)));
    if (!smoke) xrtMemDebugSnapshot(&initial);
    entered = xrtSemCreate(0, 1); proceed = xrtSemCreate(0, 1); assert(entered && proceed);
    if (!smoke) xrtMemDebugSnapshot(&before);
    assert(pool_outcome_watch_size() == sizeof(xfuturewatch));
    assert(pool_outcome_ready_watch_probe() == 25);
    if (!smoke) balanced(&before);
    for (round = 0; round < (smoke ? 1u : 30u); ++round)
    for (kind = 0; kind < 4; ++kind)
    for (mode = 0; mode < (kind == 3 ? 2u : 4u); ++mode) {
        xfuture *future, *alias;
        unsigned firstDrop = drop_count();
        xfuturestate expected = kind == 3 ? (mode ? XFUTURE_RESOLVED : XFUTURE_FAILED)
            : mode == 0 ? XFUTURE_RESOLVED : mode == 1 ? XFUTURE_FAILED : mode == 2 ? XFUTURE_CANCELLED : XFUTURE_CLOSED;
        if (!smoke) xrtMemDebugSnapshot(&before);
        assert(pool_outcome_init());
        future = pool_outcome_start(kind, mode); assert(future && !xrtGetError());
        assert(xrtFutureWait(future) == 0 && xrtFutureState(future) == expected);
        alias = xrtFutureRef(future); assert(alias);
        pool_outcome_fini(); assert(!xrtGetError());
        if (expected == XFUTURE_RESOLVED) {
            const void* value = xrtFutureValue(future); assert(value && !xrtFutureError(future));
            if (kind == 0 || kind == 3) assert(*(const int64_t*)value == (kind == 0 ? 10 : 2));
            else if (kind == 1) assert(!strcmp(*(char* const*)value, "owned payload"));
            else assert(*(xvalue* const*)value && drop_count() == firstDrop);
        } else if (expected == XFUTURE_FAILED) assert(xrtFutureError(future));
        else assert(!xrtFutureError(future));
        xrtFutureDestroy(future);
        if (kind == 2 && mode == 0) assert(drop_count() == firstDrop);
        xrtFutureDestroy(alias);
        assert(drop_count() == firstDrop + (kind == 2 ? 1u : 0u) && !xrtGetError());
        if (!smoke) balanced(&before);
        ++executions;
    }
    /* Cancellation is a request, not an invented terminal state. It reaches
     * the blocked producer through the output -> task parent token chain. */
    for (round = 0; round < (smoke ? 1u : 30u); ++round)
    for (mode = 0; mode < 4; ++mode) {
        xfuture* future;
        unsigned firstDrop = drop_count();
        if (!smoke) xrtMemDebugSnapshot(&before);
        raiseDrop = (mode & 1) != 0;
        assert(pool_outcome_init()); future = pool_outcome_start(2, 4); assert(future);
        assert(xrtSemWaitFor(entered, 15000000) == XWAIT_OK && !xrtFutureDone(future));
        if (mode < 2) {
            assert(xrtFutureCancel(future));
            assert(!xrtFutureDone(future));
            assert(xrtSemPost(proceed));
            assert(xrtFutureWait(future) == 0 && xrtFutureState(future) == XFUTURE_CANCELLED && !xrtFutureError(future));
            xrtFutureDestroy(future); ++cancels;
        } else {
            /* Dropping the last consumer does not strand the producer or its
             * owned result; pool quiescence waits for watch release as well. */
            xrtFutureDestroy(future);
            assert(xrtSemPost(proceed));
        }
        pool_outcome_fini();
        assert(drop_count() == firstDrop + 1 && !xrtGetError());
        raiseDrop = 0;
        if (!smoke) balanced(&before);
        ++executions;
    }
    if (!smoke) {
        unsigned budget;
        for (budget = 0; budget < 128; ++budget) {
            xfuture* future; int triggered; unsigned beforeCalls = call_count();
            xrtMemDebugSnapshot(&before); assert(pool_outcome_init());
            assert(xrtMemDebugFailAfter(budget));
            future = pool_outcome_start(0, 0);
            triggered = xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
            if (triggered) {
                assert(future == NULL && xrtGetError()); xrtClearError(); ++faults;
            } else {
                assert(future && !xrtGetError() && xrtFutureWait(future) == 0 && xrtFutureState(future) == XFUTURE_RESOLVED);
                xrtFutureDestroy(future);
            }
            pool_outcome_fini(); assert(!xrtGetError());
            assert(call_count() == beforeCalls + (triggered ? 0u : 1u));
            balanced(&before);
            if (!triggered) break;
        }
        assert(budget < 128 && faults >= 5);
    }
    assert(xrtSemDestroy(entered) && xrtSemDestroy(proceed));
    if (!smoke) {
        balanced(&initial); xrtMemDebugSnapshot(&final);
        printf("pool outcome runtime: %u executions, %u reverse cancellations, %u pre-submit faults, %llu balanced allocations\n",
            executions, cancels, faults, (unsigned long long)(final.AllocCount - initial.AllocCount));
    } else puts("pool outcome generated C-host smoke passed");
    return 0;
}
