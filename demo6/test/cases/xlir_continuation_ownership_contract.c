#define XRT_MODULE_FUTURE
#define XRT_MODULE_VALUE
#define XRT_MODULE_MEMORY_DEBUG
#include "lib/xrt/xrt.h"
#include "src/runtime/xl6_continuation_future.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct frame {
    XL6_ContinuationTicket ticket;
    xfuturewatch watch;
    xfuture* source;
    xpromise* outer;
    xvalue *first, *second;
    unsigned runs, releases;
    bool failTrace;
} frame;
static bool trace(const void* data, xrtownershipvisitor visit, void* context)
{
    const frame* value = (const frame*)data;
    return !value->failTrace && visit(xrtFutureOwnership(value->source), context) &&
        visit(xrtPromiseOwnership(value->outer), context) &&
        visit(xrtValueOwnership(value->first), context) && visit(xrtValueOwnership(value->second), context);
}
static void rejected(frame* value)
{
    xrtownershipref root = xl6ContinuationOwnership(&value->ticket);
    xrtownershipresult old = {11,22,33,44}, result = old; bool live = true;
    assert(!xrtOwnershipInspectReachable(&root, 1, NULL, 0, &live, &result, NULL, NULL));
    assert(live && !memcmp(&old, &result, sizeof(result))); xrtClearError();
}
static void run(void* data)
{
    frame* value = (frame*)data;
    assert(value->ticket.ownershipRefs == 1); rejected(value);
    ++value->runs; assert(xrtPromiseResolve(value->outer, NULL));
}
static void release(void* data)
{
    frame* value = (frame*)data;
    assert(value->ticket.ownershipRefs == 0 && value->ticket.queue == NULL);
    ++value->releases;
    xrtValueRelease(value->first); xrtValueRelease(value->second);
    xrtFutureDestroy(value->source); xrtPromiseDestroy(value->outer);
}
static void balance(xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after; xrtClearError(); xrtMemDebugSnapshot(&after);
    assert(before->LiveCount == after.LiveCount && before->LiveBytes == after.LiveBytes);
    assert(after.AllocCount - before->AllocCount == after.FreeCount - before->FreeCount);
    assert(before->InvalidFreeCount == after.InvalidFreeCount && before->DoubleFreeCount == after.DoubleFreeCount);
    assert(before->UseAfterFreeCount == after.UseAfterFreeCount);
}
static void inspect(frame* value, xfuture* source, xfuture* output, bool pending, size_t aliases)
{
    xrtownershipref roots[3], slots[3]; xrtownershipresult result = {0}; bool live[3] = {0};
    roots[0] = xl6ContinuationOwnership(&value->ticket); roots[1] = xrtFutureOwnership(source); roots[2] = xrtFutureOwnership(output);
    slots[0] = slots[1] = roots[1]; slots[2] = roots[2];
    assert(xrtOwnershipInspectReachable(roots, 3, slots, 3, live, &result, NULL, NULL));
    assert(live[0] && live[1] && live[2] && result.ReachableAnchorCount == 3);
    assert(result.NodeCount == 6 && result.EdgeCount == (pending ? 10u : 9u));
    assert(result.ExternalRootCount == 1 + aliases); /* genuine queue credit, never erased */
}
typedef struct code_owner { frame* value; unsigned leases, drops; } code_owner;
static int acquire_code(void* data)
{ code_owner* owner = data; ++owner->leases; return 1; }
static void release_code(void* data)
{
    code_owner* owner = data;
    assert(owner->leases == 1 && owner->value->runs == 1 && owner->value->releases == 1);
    --owner->leases; ++owner->drops;
}
static void opaque_code_owner(void)
{
    xmemdebugsnapshot before; frame value = {0}; code_owner code = {&value,0,0};
    XL6_CodeOwner owner = {sizeof(owner), &code, acquire_code, release_code};
    XL6_ContinuationQueue* queue; xfuture *source, *output; xpromise* producer;
    xrtMemDebugSnapshot(&before); queue = xl6ContinuationQueueCreate(1); assert(queue);
    producer = xrtPromiseCreate(&source, NULL); assert(producer);
    value.source = xrtFutureRef(source); value.outer = xrtPromiseCreate(&output, NULL); assert(value.outer);
    assert(xl6ContinuationReserveOwned(queue, &value.ticket, run, release, &value, &owner) == XL6_CONTINUATION_ADMITTED);
    assert(xl6ContinuationTraceBind(&value.ticket, trace));
    assert(xl6ContinuationWatchRegister(&value.ticket, &value.watch, source)); xl6ContinuationCommit(&value.ticket);
    /* Data alone is described, but the acquired host owner is not. Refuse
     * inspection instead of silently losing the actual code-owner edge. */
    rejected(&value); assert(code.leases == 1 && !code.drops);
    assert(xrtPromiseResolve(producer, NULL)); rejected(&value);
    xl6ContinuationQueueClose(queue); assert(xl6ContinuationRunOne(queue));
    assert(code.leases == 0 && code.drops == 1 && xl6ContinuationQueueDestroy(queue));
    xrtFutureDestroy(source); xrtFutureDestroy(output); xrtPromiseDestroy(producer); balance(&before);
}
int main(void)
{
    unsigned failures = 0; assert(xrtMemDebugEnable(true));
    for (unsigned round = 0; round < 100; ++round) for (unsigned outcome = 0; outcome < 4; ++outcome) {
        frame value = {0}; xmemdebugsnapshot before; xfuture *source, *output; xpromise* producer;
        XL6_ContinuationQueue* queue; xvalue* alias;
        xrtMemDebugSnapshot(&before);
        queue = xl6ContinuationQueueCreate(1); assert(queue);
        producer = xrtPromiseCreate(&source, NULL); assert(producer);
        value.source = xrtFutureRef(source); value.outer = xrtPromiseCreate(&output, NULL); assert(value.outer);
        value.first = xrtValueInt(42); assert(value.first); value.second = xrtValueRetain(value.first);
        assert(xl6ContinuationReserve(queue, &value.ticket, run, release, &value) == XL6_CONTINUATION_ADMITTED);
        rejected(&value);
        assert(xl6ContinuationTraceBind(&value.ticket, trace));
        assert(!xl6ContinuationTraceBind(&value.ticket, trace)); xrtClearError();
        assert(xl6ContinuationWatchRegister(&value.ticket, &value.watch, source));
        assert(value.ticket.ownershipRefs == 2 && !xl6ContinuationAbort(&value.ticket));
        rejected(&value); /* frame still caller-owned until Commit */
        xl6ContinuationCommit(&value.ticket); inspect(&value, source, output, true, 0);
        alias = xrtValueRetain(value.first); inspect(&value, source, output, true, 1); xrtValueRelease(alias);
        value.failTrace = true; rejected(&value); value.failTrace = false;
        for (unsigned point = 0; point < 6; ++point) {
            xrtownershipref anchor = xl6ContinuationOwnership(&value.ticket);
            xrtownershipresult result = {11,22,33,44}, old = result;
            bool accepted;
            assert(xrtMemDebugFailAfter(point));
            accepted = xrtOwnershipInspect(&anchor, 1, NULL, 0, &result);
            if (xrtMemDebugFailTriggered()) {
                assert(!accepted && !memcmp(&result, &old, sizeof(result))); ++failures;
            } else assert(accepted);
            xrtMemDebugFailClear(); xrtClearError();
            assert(!value.runs && !value.releases && value.ticket.ownershipRefs == 2);
        }
        if (outcome == 0) assert(xrtPromiseResolve(producer, NULL));
        else if (outcome == 1) { xerror* error = xrtErrorCreate(XERR_INTERNAL, "test", 1, "failure");
            assert(error && xrtPromiseReject(producer, error)); xrtErrorFree(error); }
        else if (outcome == 2) assert(xrtPromiseClose(producer));
        else assert(xrtPromiseCancel(producer));
        /* Failure adds an error node; root/storage-lifetime accounting remains
         * exact in the success/closed/cancelled shapes below. */
        if (outcome != 1) inspect(&value, source, output, false, 0);
        assert(value.ticket.ownershipRefs == 1 && !value.runs && !value.releases);
        xl6ContinuationQueueClose(queue); assert(xl6ContinuationRunOne(queue));
        assert(value.runs == 1 && value.releases == 1 && xl6ContinuationQueueDestroy(queue));
        xrtFutureDestroy(source); xrtPromiseDestroy(producer); xrtFutureDestroy(output); balance(&before);
    }
    for (unsigned round = 0; round < 100; ++round) opaque_code_owner();
    assert(failures == 1600);
    printf("Continuation ownership: 400 real Watch lifecycles, 2400 inspection probes, %u actual failures, queue roots and exact memory balance\n", failures);
    puts("Opaque code owners: 100 refusals with unchanged graph and owner release after physical frame destruction");
    return 0;
}
