#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_THREAD
#define XRT_MODULE_FUTURE
#include "lib/xrt/xrt.h"
#include "src/runtime/xl6_continuation_executor.h"
#include <assert.h>
#include <stdatomic.h>
#include <stdio.h>

typedef struct observation {
    atomic_int done, phase, unblocked;
    unsigned runs[2], releases[2];
    uint64_t threads[2];
    XL6_ContinuationBlockingAdmission admission;
} observation;
typedef struct work {
    XL6_ContinuationTicket ticket;
    XL6_ContinuationExecutor* executor;
    observation* observed;
    unsigned index, mode;
    bool blocking;
} work;
static void await_count(atomic_int* value, int count)
{
    xdeadline deadline = xrtDeadlineAfter(5000000);
    while (atomic_load_explicit(value, memory_order_acquire) != count) {
        assert(!xrtDeadlineExpired(deadline)); xrtThreadYield();
    }
}
static void execute(void* data)
{
    work* item = (work*)data;
    observation* seen = item->observed;
    unsigned i = item->index;
    ++seen->runs[i]; seen->threads[i] = xrtThreadCurrentId();
    assert(!xl6ContinuationExecutorDestroy(item->executor));
    if (item->blocking && i == 0) {
        if (item->mode == 2) assert(xrtMemDebugFailAfter(0));
        seen->admission = xl6ContinuationBlockingEnter(item->executor);
        if (item->mode == 2) {
            assert(xrtMemDebugFailTriggered()); xrtMemDebugFailClear(); xrtClearError();
        }
        atomic_store_explicit(&seen->phase, 1, memory_order_release);
        if (seen->admission == XL6_CONTINUATION_BLOCKING_ADMITTED) {
            assert(xl6ContinuationBlockingEnter(item->executor) == XL6_CONTINUATION_BLOCKING_ADMITTED);
            await_count(&seen->unblocked, 1);
            xl6ContinuationBlockingLeave(item->executor);
            xl6ContinuationBlockingLeave(item->executor);
        }
    } else if (item->blocking) atomic_store_explicit(&seen->unblocked, 1, memory_order_release);
    assert(xl6ContinuationComplete(&item->ticket));
    if (!item->blocking) atomic_store_explicit(&seen->phase, 1, memory_order_release);
}
static void retire(void* data)
{
    work* item = (work*)data; observation* seen = item->observed;
    assert(!xl6ContinuationExecutorDestroy(item->executor));
    ++seen->releases[item->index];
    xrtFree(item);
    atomic_fetch_add_explicit(&seen->done, 1, memory_order_release);
}
static work* reserve(XL6_ContinuationExecutor* executor, observation* seen, unsigned index, bool blocking, unsigned mode)
{
    work* item = (work*)xrtCalloc(1, sizeof(*item)); assert(item);
    item->executor = executor; item->observed = seen; item->index = index; item->blocking = blocking; item->mode = mode;
    assert(xl6ContinuationExecutorReserve(executor, &item->ticket, execute, retire, item) == XL6_CONTINUATION_ADMITTED);
    return item;
}
static void publish(work* item)
{
    xl6ContinuationReady(&item->ticket);
    xl6ContinuationTransportReleased(&item->ticket);
    xl6ContinuationCommit(&item->ticket);
}
static void finish(XL6_ContinuationExecutor* executor)
{
    xdeadline deadline = xrtDeadlineAfter(5000000);
    while (!xl6ContinuationExecutorDestroy(executor)) {
        assert(!xrtDeadlineExpired(deadline)); xrtThreadYield();
    }
}
static void balanced(xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after; xrtClearError(); xrtMemDebugSnapshot(&after);
    assert(before->LiveCount == after.LiveCount && before->LiveBytes == after.LiveBytes);
    assert(after.AllocCount - before->AllocCount == after.FreeCount - before->FreeCount);
    assert(before->InvalidFreeCount == after.InvalidFreeCount && before->DoubleFreeCount == after.DoubleFreeCount);
    assert(before->UseAfterFreeCount == after.UseAfterFreeCount);
}
typedef struct future_delivery {
    XL6_ContinuationTicket ticket;
    xfuturewatch watch;
    xfuture* source;
    observation* observed;
    xfuturestate expected;
} future_delivery;
typedef struct source_completion { xpromise* promise; xerror* error; unsigned mode; uint64_t thread; } source_completion;
static int64_t future_payload = 41;
static int32_t complete_source(void* data)
{
    source_completion* source = (source_completion*)data;
    xerror* ambient = xrtErrorCreate(XERR_STATE, "producer", 33, "unrelated diagnostic");
    bool ok; assert(ambient); xrtSetErrorTake(ambient);
    source->thread = xrtThreadCurrentId();
    ok = source->mode == 0 ? xrtPromiseResolve(source->promise, &future_payload) :
         source->mode == 1 ? xrtPromiseReject(source->promise, source->error) :
         source->mode == 2 ? xrtPromiseCancel(source->promise) : xrtPromiseClose(source->promise);
    assert(ok && xrtGetError() == ambient); xrtClearError(); return 0;
}
static void deliver_future(void* data)
{
    future_delivery* delivery = (future_delivery*)data;
    xfutureresult result; assert(xrtFutureResult(delivery->source, &result));
    assert(result.State == delivery->expected);
    if (result.State == XFUTURE_RESOLVED) assert(result.Value && *(int64_t*)result.Value == 41);
    if (result.State == XFUTURE_FAILED) assert(result.Error && xrtErrorCode(result.Error) == 27);
    delivery->observed->threads[0] = xrtThreadCurrentId();
    ++delivery->observed->runs[0];
}
static void retire_delivery(void* data)
{
    future_delivery* delivery = (future_delivery*)data;
    observation* observed = delivery->observed;
    xrtFutureDestroy(delivery->source); ++observed->releases[0]; xrtFree(delivery);
    atomic_fetch_add_explicit(&observed->done, 1, memory_order_release);
}
static void delivery_ready(void* data)
{ xl6ContinuationReady(&((future_delivery*)data)->ticket); }
static void delivery_watch_release(void* data)
{ xl6ContinuationTransportReleased(&((future_delivery*)data)->ticket); }
typedef struct saturation {
    atomic_int started, unblock, done;
    XL6_ContinuationBlockingAdmission admission[2];
    uint64_t threads[3];
} saturation;
typedef struct saturated_work {
    XL6_ContinuationTicket ticket;
    XL6_ContinuationExecutor* executor;
    saturation* seen;
    unsigned index;
} saturated_work;
static void saturated_run(void* data)
{
    saturated_work* item = (saturated_work*)data; unsigned i = item->index;
    saturation* seen = item->seen;
    seen->threads[i] = xrtThreadCurrentId();
    if (i < 2) {
        seen->admission[i] = xl6ContinuationBlockingEnter(item->executor);
        atomic_fetch_add_explicit(&seen->started, 1, memory_order_release);
        if (seen->admission[i] == XL6_CONTINUATION_BLOCKING_ADMITTED) {
            await_count(&seen->unblock, 1); xl6ContinuationBlockingLeave(item->executor);
        }
    } else atomic_store_explicit(&seen->unblock, 1, memory_order_release);
}
static void saturated_release(void* data)
{
    saturated_work* item = (saturated_work*)data; saturation* seen = item->seen;
    xrtFree(item); atomic_fetch_add_explicit(&seen->done, 1, memory_order_release);
}
int main(void)
{
    unsigned round, mode, creationFailures = 0;
    uint64_t producer = xrtThreadCurrentId();
    assert(xrtMemDebugEnable(true));
    for (round = 0; round < 100; ++round) {
        xmemdebugsnapshot before; XL6_ContinuationExecutor* executor; work* item;
        observation seen = {0}; XL6_ContinuationTicket extra = {0};
        xrtMemDebugSnapshot(&before);
        executor = xl6ContinuationExecutorCreate(1, 2, 2); assert(executor);
        assert(xl6ContinuationBlockingEnter(executor) == XL6_CONTINUATION_BLOCKING_ADMITTED);
        xl6ContinuationBlockingLeave(executor);
        item = reserve(executor, &seen, 0, false, 0);
        assert(xl6ContinuationExecutorReserve(executor, &extra, execute, retire, NULL) == XL6_CONTINUATION_FULL);
        xl6ContinuationExecutorClose(executor);
        assert(!xl6ContinuationExecutorDestroy(executor));
        assert(!xl6ContinuationExecutorDestroyCompleted(executor));
        assert(xl6ContinuationExecutorReserve(executor, &extra, execute, retire, NULL) == XL6_CONTINUATION_CLOSED);
        publish(item);
        if (round & 1) { await_count(&seen.done, 1); finish(executor); }
        else { await_count(&seen.phase, 1); assert(xl6ContinuationExecutorDestroyCompleted(executor)); }
        await_count(&seen.done, 1);
        assert(seen.runs[0] == 1 && seen.releases[0] == 1 && seen.threads[0] != producer);
        balanced(&before);
    }
    for (round = 0; round < 50; ++round)
    for (mode = 0; mode < 3; ++mode) {
        xmemdebugsnapshot before; XL6_ContinuationExecutor* executor;
        observation seen = {0}; work *a, *b;
        xrtMemDebugSnapshot(&before);
        executor = xl6ContinuationExecutorCreate(2, 1, mode == 1 ? 1 : 2); assert(executor);
        a = reserve(executor, &seen, 0, true, mode); b = reserve(executor, &seen, 1, true, mode);
        xl6ContinuationExecutorClose(executor);
        publish(a); await_count(&seen.phase, 1);
        publish(b); await_count(&seen.done, 2); finish(executor);
        assert(seen.admission == (mode == 0 ? XL6_CONTINUATION_BLOCKING_ADMITTED : mode == 1 ? XL6_CONTINUATION_BLOCKING_LIMIT : XL6_CONTINUATION_BLOCKING_RESOURCE_FAILURE));
        assert(seen.runs[0] == 1 && seen.runs[1] == 1 && seen.releases[0] == 1 && seen.releases[1] == 1);
        assert(seen.threads[0] != producer && seen.threads[1] != producer);
        if (mode == 0) assert(seen.threads[0] != seen.threads[1]);
        balanced(&before);
    }
    for (round = 0; round < 25; ++round)
    for (mode = 0; mode < 4; ++mode) {
        unsigned order;
        for (order = 0; order < 3; ++order) {
            static const xfuturestate states[4] = {XFUTURE_RESOLVED, XFUTURE_FAILED, XFUTURE_CANCELLED, XFUTURE_CLOSED};
            xmemdebugsnapshot before; observation seen = {0};
            XL6_ContinuationExecutor* executor; future_delivery* delivery;
            xfuture* input = NULL; xpromise* promise; xerror* error;
            xthread* producerThread = NULL; xfuturewatchresult watched; source_completion source;
            xrtMemDebugSnapshot(&before);
            executor = xl6ContinuationExecutorCreate(1, 1, 2); assert(executor);
            promise = xrtPromiseCreate(&input, NULL); error = xrtErrorCreate(XERR_STATE, "input", 27, "failed source");
            assert(promise && input && error);
            source = (source_completion){promise, error, mode, 0};
            delivery = (future_delivery*)xrtCalloc(1, sizeof(*delivery)); assert(delivery);
            delivery->source = xrtFutureRef(input); delivery->observed = &seen; delivery->expected = states[mode];
            assert(xl6ContinuationExecutorReserve(executor, &delivery->ticket, deliver_future, retire_delivery, delivery) == XL6_CONTINUATION_ADMITTED);
            xl6ContinuationExecutorClose(executor);
            if (order == 0) complete_source(&source);
            assert(xrtFutureWatchInit(&delivery->watch, delivery_ready, delivery_watch_release, delivery));
            watched = xrtFutureWatchAdd(input, &delivery->watch);
            assert(watched == (order == 0 ? XFUTURE_WATCH_READY : XFUTURE_WATCH_PENDING));
            if (order == 0) { delivery_ready(delivery); delivery_watch_release(delivery); }
            if (order == 1) xl6ContinuationCommit(&delivery->ticket);
            if (order != 0) {
                producerThread = xrtThreadCreate(complete_source, &source, 0); assert(producerThread);
                assert(xrtThreadWait(producerThread) == XWAIT_OK); xrtThreadDestroy(producerThread);
            }
            if (order != 1) xl6ContinuationCommit(&delivery->ticket);
            await_count(&seen.done, 1); finish(executor);
            assert(seen.runs[0] == 1 && seen.releases[0] == 1 && seen.threads[0] != source.thread);
            xrtFutureDestroy(input); xrtPromiseDestroy(promise); xrtErrorFree(error); balanced(&before);
        }
    }
    for (round = 0; round < 25; ++round)
    for (mode = 0; mode < 2; ++mode) {
        xmemdebugsnapshot before; XL6_ContinuationExecutor* executor;
        saturation seen = {0}; saturated_work* items[3]; unsigned i;
        xrtMemDebugSnapshot(&before);
        executor = xl6ContinuationExecutorCreate(3, 2, mode == 0 ? 3 : 2); assert(executor);
        for (i = 0; i < 3; ++i) {
            items[i] = (saturated_work*)xrtCalloc(1, sizeof(*items[i])); assert(items[i]);
            items[i]->executor = executor; items[i]->seen = &seen; items[i]->index = i;
            assert(xl6ContinuationExecutorReserve(executor, &items[i]->ticket, saturated_run, saturated_release, items[i]) == XL6_CONTINUATION_ADMITTED);
        }
        xl6ContinuationExecutorClose(executor);
        for (i = 0; i < 2; ++i) {
            xl6ContinuationReady(&items[i]->ticket); xl6ContinuationTransportReleased(&items[i]->ticket); xl6ContinuationCommit(&items[i]->ticket);
        }
        await_count(&seen.started, 2);
        xl6ContinuationReady(&items[2]->ticket); xl6ContinuationTransportReleased(&items[2]->ticket); xl6ContinuationCommit(&items[2]->ticket);
        await_count(&seen.done, 3); finish(executor);
        assert((seen.admission[0] == XL6_CONTINUATION_BLOCKING_ADMITTED) + (seen.admission[1] == XL6_CONTINUATION_BLOCKING_ADMITTED) == (mode == 0 ? 2 : 1));
        if (mode == 1) assert(seen.admission[0] == XL6_CONTINUATION_BLOCKING_LIMIT || seen.admission[1] == XL6_CONTINUATION_BLOCKING_LIMIT);
        else assert(seen.threads[0] != seen.threads[2] && seen.threads[1] != seen.threads[2]);
        balanced(&before);
    }
    for (round = 0; round < 30; ++round) {
        xmemdebugsnapshot before; XL6_ContinuationExecutor* executor;
        xrtMemDebugSnapshot(&before); assert(xrtMemDebugFailAfter(round));
        executor = xl6ContinuationExecutorCreate(2, 2, 3);
        if (xrtMemDebugFailTriggered()) { assert(!executor); ++creationFailures; }
        xrtMemDebugFailClear();
        if (executor) { xl6ContinuationExecutorClose(executor); finish(executor); }
        balanced(&before);
    }
    assert(creationFailures > 0);
    printf("continuation executor: 100 isolated deliveries, 300 real Future Watch handoffs, 150 blocking/limit/fault cases, 50 multiworker saturation cases, 50 compensation faults, %u creation faults, close/busy/join and balanced memory\n", creationFailures);
    return 0;
}
