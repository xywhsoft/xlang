#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_THREAD
#include "lib/xrt/xrt.h"
#include "src/runtime/xl6_continuation_queue.h"
#include <assert.h>
#include <stdio.h>
#include <stdatomic.h>

typedef struct counts { unsigned runs, releases; uint64_t thread; } counts;
typedef struct item {
    XL6_ContinuationTicket ticket;
    XL6_ContinuationQueue* queue;
    counts* observed;
    bool heap;
} item;
static void run(void* data)
{
    item* value = (item*)data;
    ++value->observed->runs;
    value->observed->thread = xrtThreadCurrentId();
    assert(!xl6ContinuationQueueDestroy(value->queue));
}
static void release(void* data)
{
    item* value = (item*)data;
    assert(value->observed->runs == 1 && value->observed->releases == 0);
    ++value->observed->releases;
    /* The queue lease must survive even the last embedded-ticket free. */
    assert(!xl6ContinuationQueueDestroy(value->queue));
    if (value->heap) xrtFree(value);
}
static void event(XL6_ContinuationTicket* ticket, unsigned kind)
{
    if (kind == 0) xl6ContinuationCommit(ticket);
    else if (kind == 1) xl6ContinuationReady(ticket);
    else xl6ContinuationTransportReleased(ticket);
}
static void balanced(xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;
    xrtClearError(); xrtMemDebugSnapshot(&after);
    assert(before->LiveCount == after.LiveCount && before->LiveBytes == after.LiveBytes);
    assert(after.AllocCount - before->AllocCount == after.FreeCount - before->FreeCount);
    assert(before->InvalidFreeCount == after.InvalidFreeCount && before->DoubleFreeCount == after.DoubleFreeCount);
    assert(before->UseAfterFreeCount == after.UseAfterFreeCount);
}
typedef struct signal_arg { XL6_ContinuationTicket* ticket; unsigned kind; atomic_int* start; } signal_arg;
static int32_t signal_thread(void* data)
{
    signal_arg* arg = (signal_arg*)data;
    while (!atomic_load_explicit(arg->start, memory_order_acquire)) xrtThreadYield();
    event(arg->ticket, arg->kind);
    return 0;
}
int main(void)
{
    static const unsigned orders[6][3] = {{0,1,2},{0,2,1},{1,0,2},{1,2,0},{2,0,1},{2,1,0}};
    unsigned round, order, failures = 0;
    assert(xrtMemDebugEnable(true));
    for (round = 0; round < 100; ++round)
    for (order = 0; order < 6; ++order) {
        xmemdebugsnapshot before;
        XL6_ContinuationQueue* queue;
        counts observed = {0}; item value = {0}, overflow = {0};
        xerror* ambient;
        xrtMemDebugSnapshot(&before);
        queue = xl6ContinuationQueueCreate(1); assert(queue);
        value.queue = queue; value.observed = &observed;
        assert(xl6ContinuationReserve(queue, &value.ticket, run, release, &value) == XL6_CONTINUATION_ADMITTED);
        assert(xl6ContinuationReserve(queue, &overflow.ticket, run, release, &overflow) == XL6_CONTINUATION_FULL);
        assert(xl6ContinuationAbort(&value.ticket));
        assert(!observed.runs && !observed.releases);
        assert(xl6ContinuationReserve(queue, &value.ticket, run, release, &value) == XL6_CONTINUATION_ADMITTED);
        xl6ContinuationQueueClose(queue);
        assert(xl6ContinuationReserve(queue, &overflow.ticket, run, release, &overflow) == XL6_CONTINUATION_CLOSED);
        assert(!xl6ContinuationQueueDestroy(queue));
        ambient = xrtErrorCreate(XERR_STATE, "queue", 81, "unrelated producer error");
        assert(ambient); xrtSetErrorTake(ambient);
        assert(xrtMemDebugFailAfter(0));
        event(&value.ticket, orders[order][0]); assert(!xl6ContinuationRunOne(queue));
        event(&value.ticket, orders[order][1]); assert(!xl6ContinuationRunOne(queue));
        event(&value.ticket, orders[order][2]);
        assert(!observed.runs && !observed.releases);
        assert(!xrtMemDebugFailTriggered()); xrtMemDebugFailClear();
        assert(xrtGetError() == ambient); xrtClearError();
        assert(xl6ContinuationRunOne(queue)); assert(!xl6ContinuationRunOne(queue));
        assert(observed.runs == 1 && observed.releases == 1);
        assert(xl6ContinuationQueueDestroy(queue)); balanced(&before);
    }
    for (round = 0; round < 50; ++round) {
        xmemdebugsnapshot before; XL6_ContinuationQueue* queue;
        counts a = {0}, b = {0}; item first = {0}, second = {0};
        xrtMemDebugSnapshot(&before);
        queue = xl6ContinuationQueueCreate(2); assert(queue);
        first.queue = second.queue = queue; first.observed = &a; second.observed = &b;
        assert(xl6ContinuationReserve(queue, &first.ticket, run, release, &first) == XL6_CONTINUATION_ADMITTED);
        assert(xl6ContinuationReserve(queue, &second.ticket, run, release, &second) == XL6_CONTINUATION_ADMITTED);
        xl6ContinuationQueueClose(queue);
        /* FIFO follows complete publication, not the earlier reservation. */
        event(&second.ticket, 0); event(&second.ticket, 1); event(&second.ticket, 2);
        event(&first.ticket, 0); event(&first.ticket, 1); event(&first.ticket, 2);
        assert(xl6ContinuationRunOne(queue)); assert(!a.runs && b.runs == 1);
        assert(xl6ContinuationRunOne(queue)); assert(a.runs == 1 && b.runs == 1);
        assert(!xl6ContinuationRunOne(queue));
        assert(xl6ContinuationQueueDestroy(queue)); balanced(&before);
    }
    for (round = 0; round < 100; ++round) {
        xmemdebugsnapshot before;
        XL6_ContinuationQueue* queue;
        counts observed = {0}; item* value; unsigned i;
        xthread* threads[3]; signal_arg args[3]; atomic_int start;
        atomic_init(&start, 0);
        xrtMemDebugSnapshot(&before);
        queue = xl6ContinuationQueueCreate(1); assert(queue);
        value = (item*)xrtCalloc(1, sizeof(*value)); assert(value);
        value->queue = queue; value->observed = &observed; value->heap = true;
        assert(xl6ContinuationReserve(queue, &value->ticket, run, release, value) == XL6_CONTINUATION_ADMITTED);
        xl6ContinuationQueueClose(queue);
        for (i = 0; i < 3; ++i) {
            args[i] = (signal_arg){&value->ticket, i, &start};
            threads[i] = xrtThreadCreate(signal_thread, &args[i], 0); assert(threads[i]);
        }
        atomic_store_explicit(&start, 1, memory_order_release);
        /* Drain concurrently with the final producer's return; the ticket is
         * freed by release before any producer thread handle is joined. */
        while (!xl6ContinuationRunOne(queue)) xrtThreadYield();
        for (i = 0; i < 3; ++i) { assert(xrtThreadWait(threads[i]) == XWAIT_OK); xrtThreadDestroy(threads[i]); }
        assert(observed.runs == 1 && observed.releases == 1 && observed.thread == xrtThreadCurrentId());
        assert(xl6ContinuationQueueDestroy(queue)); balanced(&before);
    }
    for (round = 0; round < 20; ++round) {
        xmemdebugsnapshot before; XL6_ContinuationQueue* queue;
        xrtMemDebugSnapshot(&before); assert(xrtMemDebugFailAfter(round));
        queue = xl6ContinuationQueueCreate(1);
        if (xrtMemDebugFailTriggered()) { assert(!queue); ++failures; }
        xrtMemDebugFailClear();
        if (queue) { xl6ContinuationQueueClose(queue); assert(xl6ContinuationQueueDestroy(queue)); }
        balanced(&before);
    }
    assert(failures > 0);
    printf("continuation queue: 600 event-order cases, 50 FIFO pairs, 100 concurrent handoffs, %u creation faults, allocation-free publication, preserved diagnostics, balanced memory\n", failures);
    return 0;
}
