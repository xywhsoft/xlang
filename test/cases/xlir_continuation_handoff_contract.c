#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_THREAD
#define XRT_MODULE_TIME
#define XRT_MODULE_FUTURE
#ifndef XRT_MODULE_WAIT
#define XRT_MODULE_WAIT
#endif
#include "lib/xrt/xrt.h"
#include "release/include/xl_clock.h"
#include "src/runtime/xl6_continuation_executor.h"
#include "src/runtime/xl6_continuation_future.h"
#include <assert.h>
#include <stdio.h>
#include <stdatomic.h>

static const unsigned orders[6][3] = {{0,1,2},{0,2,1},{1,0,2},{1,2,0},{2,0,1},{2,1,0}};
static unsigned watch_fault;
/* Only the transport TU redirects this public ABI. Real future completion,
 * queue publication and workers remain unchanged. Not allocator injection. */
xfuturewatchresult handoff_watch_add(xfuture* source, xfuturewatch* watch)
{
    if (watch_fault == 1) xrtSetErrorKind(XERR_MEMORY);
    if (watch_fault) return XFUTURE_WATCH_ERROR;
    return xrtFutureWatchAdd(source, watch);
}
static void count_event(void* data) { ++*(unsigned*)data; }
static void signal_event(XL6_ContinuationTicket* ticket, unsigned event)
{
    if (event == 0) xl6ContinuationCommit(ticket);
    else if (event == 1) xl6ContinuationReady(ticket);
    else xl6ContinuationTransportReleased(ticket);
}
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after; xrtClearError(); xrtMemDebugSnapshot(&after);
    assert(before->LiveCount == after.LiveCount && before->LiveBytes == after.LiveBytes);
    assert(after.AllocCount - before->AllocCount == after.FreeCount - before->FreeCount);
    assert(before->InvalidFreeCount == after.InvalidFreeCount && before->DoubleFreeCount == after.DoubleFreeCount);
    assert(before->UseAfterFreeCount == after.UseAfterFreeCount);
}
typedef struct chain {
    XL6_ContinuationTicket ticket;
    XL6_ContinuationQueue* queue;
    unsigned index, length, order;
    bool closed;
    unsigned *runs, *releases;
} chain;
static void chain_run(void*);
static void chain_release(void*);
static chain* chain_next(chain* current)
{
    chain* next = (chain*)xrtCalloc(1, sizeof(*next)); assert(next);
    next->queue = current->queue; next->index = current->index + 1;
    next->length = current->length; next->order = current->order;
    next->closed = current->closed;
    next->runs = current->runs; next->releases = current->releases; return next;
}
static void chain_run(void* data)
{
    chain* current = (chain*)data; XL6_ContinuationTicket extra = {0}; unsigned i;
    assert(*current->runs == current->index && *current->releases == current->index);
    ++*current->runs;
    if (current->index + 1 == current->length) return;
    chain* next = chain_next(current);
    assert(xl6ContinuationReserve(current->queue, &extra, chain_run, chain_release, NULL) ==
        (current->closed ? XL6_CONTINUATION_CLOSED : XL6_CONTINUATION_FULL));
    assert(xl6ContinuationReserveSuccessor(&current->ticket, &current->ticket, chain_run, chain_release, NULL) == XL6_CONTINUATION_INVALID);
    assert(xl6ContinuationReserveSuccessor(&current->ticket, &next->ticket, chain_run, chain_release, next) == XL6_CONTINUATION_ADMITTED);
    assert(xl6ContinuationReserveSuccessor(&current->ticket, &extra, chain_run, chain_release, NULL) == XL6_CONTINUATION_INVALID);
    assert(xl6ContinuationAbort(&next->ticket));
    assert(!xl6ContinuationAbort(&next->ticket));
    assert(xl6ContinuationReserveSuccessor(&current->ticket, &next->ticket, chain_run, chain_release, next) == XL6_CONTINUATION_ADMITTED);
    assert(xrtMemDebugFailAfter(0));
    for (i = 0; i < 3; ++i) {
        signal_event(&next->ticket, orders[current->order][i]);
        assert(!xl6ContinuationRunOne(current->queue));
    }
    assert(!xrtMemDebugFailTriggered()); xrtMemDebugFailClear();
    assert(!xl6ContinuationAbort(&next->ticket));
}
static void chain_release(void* data)
{
    chain* current = (chain*)data; XL6_ContinuationTicket extra = {0};
    assert(*current->runs == current->index + 1 && *current->releases == current->index);
    assert(!xl6ContinuationRunOne(current->queue));
    assert(!xl6ContinuationQueueDestroy(current->queue));
    assert(xl6ContinuationReserveSuccessor(&current->ticket, &extra, chain_run, chain_release, NULL) == XL6_CONTINUATION_INVALID);
    ++*current->releases; xrtFree(current);
}

static void await_value(atomic_uint* value, unsigned wanted)
{
    XL6_DeadlineV1 deadline = xl6DeadlineAfterMillisecondsV1(5000);
    while (atomic_load_explicit(value, memory_order_acquire) != wanted) {
        assert(!xl6DeadlineExpiredV1(deadline)); xrtThreadYield();
    }
}
typedef struct concurrent_seen {
    atomic_uint release_entered, allow_release, release_done, child_done;
    unsigned child_runs, child_releases;
    xfuture* input;
    unsigned mode;
} concurrent_seen;
typedef struct concurrent_frame {
    XL6_ContinuationTicket ticket;
    XL6_ContinuationExecutor* executor;
    concurrent_seen* seen;
    unsigned index;
    xfuturewatch watch;
    xfuture* source;
} concurrent_frame;
static void concurrent_run(void*);
static void concurrent_release(void*);
static void concurrent_run(void* data)
{
    concurrent_frame* frame = (concurrent_frame*)data;
    if (frame->index == 0) {
        concurrent_frame* next = (concurrent_frame*)xrtCalloc(1, sizeof(*next)); assert(next);
        next->executor = frame->executor; next->seen = frame->seen; next->index = 1;
        next->source = xrtFutureRef(frame->seen->input); assert(next->source);
        assert(xl6ContinuationReserveSuccessor(&frame->ticket, &next->ticket, concurrent_run, concurrent_release, next) == XL6_CONTINUATION_ADMITTED);
        assert(xl6ContinuationWatchRegister(&next->ticket, &next->watch, next->source));
        xl6ContinuationCommit(&next->ticket);
    } else {
        static const xfuturestate states[] = {XFUTURE_RESOLVED, XFUTURE_FAILED, XFUTURE_CANCELLED, XFUTURE_CLOSED};
        xfutureresult outcome;
        assert(atomic_load_explicit(&frame->seen->release_done, memory_order_acquire) == 1);
        assert(xrtFutureResult(frame->source, &outcome) && outcome.State == states[frame->seen->mode]);
        if (outcome.State == XFUTURE_RESOLVED) assert(outcome.Value && *(int64_t*)outcome.Value == 47);
        if (outcome.State == XFUTURE_FAILED) assert(outcome.Error && xrtErrorCode(outcome.Error) == 48);
        ++frame->seen->child_runs;
    }
}
static void concurrent_release(void* data)
{
    concurrent_frame* frame = (concurrent_frame*)data; concurrent_seen* seen = frame->seen;
    assert(!xl6ContinuationExecutorDestroy(frame->executor));
    if (frame->index == 0) {
        atomic_store_explicit(&seen->release_entered, 1, memory_order_release);
        await_value(&seen->allow_release, 1); xrtFree(frame);
        atomic_store_explicit(&seen->release_done, 1, memory_order_release);
    } else {
        ++seen->child_releases; xrtFutureDestroy(frame->source); xrtFree(frame);
        atomic_store_explicit(&seen->child_done, 1, memory_order_release);
    }
}
static void complete_future(xpromise* promise, unsigned mode)
{
    static int64_t payload = 47;
    xerror* ambient = xrtErrorCreate(XERR_STATE, "producer", 49, "unrelated diagnostic");
    xerror* failure = xrtErrorCreate(XERR_STATE, "source", 48, "failed source");
    bool ok;
    assert(ambient && failure); xrtSetErrorTake(ambient);
    ok = mode == 0 ? xrtPromiseResolve(promise, &payload) : mode == 1 ? xrtPromiseReject(promise, failure) :
        mode == 2 ? xrtPromiseCancel(promise) : xrtPromiseClose(promise);
    assert(ok && xrtGetError() == ambient); xrtClearError(); xrtErrorFree(failure);
}
int main(void)
{
    unsigned round, order, closed, mode;
    assert(xrtMemDebugEnable(true));
    for (round = 0; round < 100; ++round)
    for (mode = 0; mode < 3; ++mode) {
        xmemdebugsnapshot before; XL6_ContinuationTicket ticket = {0}; xfuturewatch watch;
        XL6_ContinuationQueue* queue; xfuture* input = NULL; xpromise* promise; unsigned calls = 0;
        xrtMemDebugSnapshot(&before);
        queue = xl6ContinuationQueueCreate(1); promise = xrtPromiseCreate(&input, NULL); assert(queue && promise && input);
        assert(xl6ContinuationReserve(queue, &ticket, count_event, count_event, &calls) == XL6_CONTINUATION_ADMITTED);
        assert(!xl6ContinuationWatchRegister(&ticket, &watch, NULL)); xrtClearError();
        assert(xl6ContinuationAbort(&ticket));
        assert(xl6ContinuationReserve(queue, &ticket, count_event, count_event, &calls) == XL6_CONTINUATION_ADMITTED);
        watch_fault = mode;
        if (mode != 0) {
            assert(!xl6ContinuationWatchRegister(&ticket, &watch, input));
            assert((xrtGetError() != NULL) == (mode == 1)); xrtClearError();
            assert(xl6ContinuationAbort(&ticket) && calls == 0);
            assert(xl6ContinuationReserve(queue, &ticket, count_event, count_event, &calls) == XL6_CONTINUATION_ADMITTED);
        }
        watch_fault = 0;
        assert(xrtMemDebugFailAfter(0));
        assert(xl6ContinuationWatchRegister(&ticket, &watch, input));
        assert(!xrtMemDebugFailTriggered()); xrtMemDebugFailClear();
        assert(!xl6ContinuationAbort(&ticket));
        assert(!xl6ContinuationWatchRegister(&ticket, &watch, input)); xrtClearError();
        assert(!xl6ContinuationAbort(&ticket));
        complete_future(promise, 0); assert(!xl6ContinuationRunOne(queue) && calls == 0);
        xl6ContinuationCommit(&ticket); assert(xl6ContinuationRunOne(queue) && calls == 2);
        xl6ContinuationQueueClose(queue); assert(xl6ContinuationQueueDestroy(queue));
        xrtFutureDestroy(input); xrtPromiseDestroy(promise); balanced(&before);
    }
    for (round = 0; round < 20; ++round)
    for (closed = 0; closed < 2; ++closed)
    for (order = 0; order < 6; ++order) {
        xmemdebugsnapshot before; unsigned runs = 0, releases = 0;
        XL6_ContinuationTicket extra = {0}; XL6_ContinuationQueue* queue; chain* first;
        xrtMemDebugSnapshot(&before);
        queue = xl6ContinuationQueueCreate(1); assert(queue);
        first = (chain*)xrtCalloc(1, sizeof(*first)); assert(first);
        first->queue = queue; first->length = 32; first->order = order;
        first->closed = closed != 0;
        first->runs = &runs; first->releases = &releases;
        assert(xl6ContinuationReserveSuccessor(&first->ticket, &extra, chain_run, chain_release, NULL) == XL6_CONTINUATION_INVALID);
        assert(xl6ContinuationReserve(queue, &first->ticket, chain_run, chain_release, first) == XL6_CONTINUATION_ADMITTED);
        assert(xl6ContinuationReserveSuccessor(&first->ticket, &extra, chain_run, chain_release, NULL) == XL6_CONTINUATION_INVALID);
        assert(xl6ContinuationReserve(queue, &extra, chain_run, chain_release, NULL) == XL6_CONTINUATION_FULL);
        if (closed) xl6ContinuationQueueClose(queue);
        xl6ContinuationCommit(&first->ticket); xl6ContinuationReady(&first->ticket); xl6ContinuationTransportReleased(&first->ticket);
        while (xl6ContinuationRunOne(queue)) {}
        assert(runs == 32 && releases == 32 && xl6ContinuationQueueIsIdle(queue));
        if (!closed) {
            assert(xl6ContinuationReserve(queue, &extra, chain_run, chain_release, NULL) == XL6_CONTINUATION_ADMITTED);
            assert(xl6ContinuationAbort(&extra)); xl6ContinuationQueueClose(queue);
        }
        assert(xl6ContinuationQueueDestroy(queue)); balanced(&before);
    }
    for (round = 0; round < 25; ++round)
    for (order = 0; order < 3; ++order)
    for (mode = 0; mode < 4; ++mode) {
        xmemdebugsnapshot before; XL6_ContinuationExecutor* executor; concurrent_frame* first;
        concurrent_seen seen = {0}; XL6_DeadlineV1 deadline; xpromise* promise;
        seen.mode = mode;
        xrtMemDebugSnapshot(&before);
        promise = xrtPromiseCreate(&seen.input, NULL); assert(promise && seen.input);
        if (order == 0) complete_future(promise, mode);
        executor = xl6ContinuationExecutorCreate(1, 2, 2); assert(executor);
        first = (concurrent_frame*)xrtCalloc(1, sizeof(*first)); assert(first);
        first->executor = executor; first->seen = &seen;
        assert(xl6ContinuationExecutorReserve(executor, &first->ticket, concurrent_run, concurrent_release, first) == XL6_CONTINUATION_ADMITTED);
        xl6ContinuationExecutorClose(executor);
        xl6ContinuationReady(&first->ticket); xl6ContinuationTransportReleased(&first->ticket); xl6ContinuationCommit(&first->ticket);
        await_value(&seen.release_entered, 1);
        assert(atomic_load_explicit(&seen.child_done, memory_order_acquire) == 0);
        assert(!xl6ContinuationExecutorDestroy(executor));
        if (order == 1) complete_future(promise, mode);
        atomic_store_explicit(&seen.allow_release, 1, memory_order_release);
        if (order == 2) {
            await_value(&seen.release_done, 1);
            complete_future(promise, mode);
        }
        await_value(&seen.child_done, 1);
        deadline = xl6DeadlineAfterMillisecondsV1(5000);
        while (!xl6ContinuationExecutorDestroy(executor)) {
            assert(!xl6DeadlineExpiredV1(deadline)); xrtThreadYield();
        }
        assert(seen.child_runs == 1 && seen.child_releases == 1);
        xrtFutureDestroy(seen.input); xrtPromiseDestroy(promise); balanced(&before);
    }
    puts("continuation handoff: 240 open/closed capacity-one chains / 7680 frames, six event orders, abort/retry, capacity reuse; 300 registration/rollback checks (200 public-ABI failures), zero-allocation registration/publication; 300 two-worker real Future handoffs (4 outcomes x 3 retirement orders), producer diagnostics and memory balanced");
    return 0;
}
