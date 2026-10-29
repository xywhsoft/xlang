/* Execute the actual generated frame and real Watch/successor transport on a
 * host-driven queue. No producer or executor can run while Inspect is active.
 * The normal worker policy is separately checked by the terminal/source gate. */
#define xl6ContinuationExecutorReserveOwned frame_test_reserve
#include "xlir_frame_ownership.c"
#undef xl6ContinuationExecutorReserveOwned
#include <assert.h>
#include <stdio.h>

static XL6_ContinuationQueue* queue;
static xfuture* sources[2];
static xvalue* captured;
static unsigned roots;
XL6_ContinuationAdmission frame_test_reserve(XL6_ContinuationExecutor* executor,
    XL6_ContinuationTicket* ticket, XL6_ContinuationProc run,
    XL6_ContinuationProc release, void* data, const XL6_CodeOwner* owner)
{
    (void)executor; assert(owner == NULL); ++roots;
    return xl6ContinuationReserve(queue, ticket, run, release, data);
}
xfuture* frame_graph_source(int64_t stage)
{ assert(stage == 0 || stage == 1); return xrtFutureRef(sources[stage]); }
void frame_graph_capture(xvalue* value)
{ assert(!captured && value); captured = xrtValueRetain(value); assert(captured); }

static void graph(unsigned round, unsigned mode, unsigned stage, bool alias)
{
    /* Inspect the actual capture graph at each await. The cell case retains
     * its original input capture and its cell payload separately. Replacing
     * that payload with [42] introduces its three value/container nodes and
     * two edges while the original input capture is still live. */
    static const size_t nodes[][2] = {{13,13}, {14,17}, {10,10}};
    static const size_t edges[][2] = {{23,23}, {23,25}, {17,17}};
    xrtownershipref anchors[4], slots[5];
    xrtownershipresult result = {0}; bool live[4] = {0}; xvalue* retained = alias ? xrtValueRetain(captured) : NULL;
    anchors[0] = xrtFutureOwnership(sources[0]); anchors[1] = xrtFutureOwnership(sources[1]);
    anchors[2] = xrtValueOwnership(captured); anchors[3] = xrtFutureOwnership(xrtValueGetFuture(captured));
    slots[0] = slots[1] = anchors[0]; slots[2] = slots[3] = anchors[1]; slots[4] = anchors[2];
    assert(xrtOwnershipInspectReachable(anchors, 4, slots, 5, live, &result, NULL, NULL));
    if (!round && !alias) {
        printf("frame mode=%u stage=%u nodes=%zu edges=%zu roots=%zu reachable=%zu\n", mode, stage,
            result.NodeCount, result.EdgeCount, result.ExternalRootCount, result.ReachableAnchorCount); fflush(stdout);
    }
    /* Even if ALL host source/output references are accounted as internal,
     * the independent queue execution lease keeps the activation/output live. */
    assert(result.ExternalRootCount == 1u + (alias ? 1u : 0u) && live[3] && live[2] == alias);
    assert(result.NodeCount == nodes[mode][stage] && result.EdgeCount == edges[mode][stage]);
    assert(result.ReachableAnchorCount == (mode == 0 && stage == 1 ? 3u : 2u) + (alias ? 1u : 0u));
    xrtValueRelease(retained);
}
static void resolve_one(xpromise* producer)
{
    int64_t* result = xrtMalloc(sizeof(*result)); assert(result); *result = 1;
    assert(xl6PayloadResolveOwned(producer, result, XL6_FUTURE_PAYLOAD_VALUE));
}
int main(void)
{
    unsigned transactions = 0, handoffs = 0;
    assert(xrtMemDebugEnable(true));
    for (unsigned round = 0; round < 100; ++round) for (unsigned mode = 0; mode < 3; ++mode)
    for (unsigned outcome = 0; outcome < 5; ++outcome) {
        xmemdebugsnapshot before, after; xpromise* producers[2]; xfuture* output; xerror* primary = NULL;
        xrtMemDebugSnapshot(&before); roots = 0;
        assert(xlir_module_init()); queue = xl6ContinuationQueueCreate(1); assert(queue);
        for (unsigned i = 0; i < 2; ++i) { producers[i] = xrtPromiseCreate(&sources[i], NULL); assert(producers[i]); }
        frame_graph_build(mode); assert(captured && !xrtGetError() && roots == 1);
        /* Ready entry is now scheduled before the first source await. Drive
         * it to that real pending await, then inspect the original states. */
        assert(xl6ContinuationRunOne(queue));
        output = xrtValueGetFuture(captured); assert(output && xrtFutureState(output) == XFUTURE_PENDING);
        graph(round || outcome, mode, 0, false); graph(1, mode, 0, true);
        xl6ContinuationQueueClose(queue); /* successor inherits after Close */
        resolve_one(producers[0]); assert(xl6ContinuationRunOne(queue)); ++handoffs;
        assert(xrtFutureState(output) == XFUTURE_PENDING && roots == 1);
        graph(round || outcome, mode, 1, false); graph(1, mode, 1, true);
        if (outcome == 0) resolve_one(producers[1]);
        else if (outcome == 1) {
            primary = xrtErrorCreate(XERR_INTERNAL, "native", 7, "original source failure");
            assert(primary && xrtPromiseReject(producers[1], primary));
        } else if (outcome == 2) assert(xrtPromiseClose(producers[1]));
        else if (outcome == 3) assert(xrtPromiseCancel(producers[1]));
        else { assert(xrtFutureCancel(output)); resolve_one(producers[1]); }
        for (unsigned drained = 0; xl6ContinuationRunOne(queue); ++drained) { assert(drained < 3); ++handoffs; }
        assert(xl6ContinuationQueueDestroy(queue)); queue = NULL;
        if (outcome == 0) {
            assert(xrtFutureState(output) == XFUTURE_RESOLVED && xrtFutureValue(output));
            assert(*(int64_t*)xrtFutureValue(output) == (mode == 0 ? 125 : 44));
        } else {
            xfutureresult result = {0}; assert(xrtFutureResult(output, &result));
            assert(result.State == (outcome == 3 || outcome == 4 ? XFUTURE_CANCELLED :
                outcome == 2 ? XFUTURE_CLOSED : XFUTURE_FAILED));
            if (outcome == 1) assert(result.Error == primary);
        }
        xrtErrorFree(primary);
        for (unsigned i = 0; i < 2; ++i) { xrtPromiseDestroy(producers[i]); xrtFutureDestroy(sources[i]); sources[i] = NULL; }
        xlir_module_fini(); assert(!xrtGetError());
        { xrtownershipref root = xrtValueOwnership(captured); xrtownershipresult result; bool live = true;
          assert(xrtOwnershipInspectReachable(&root, 1, &root, 1, &live, &result, NULL, NULL));
          assert(!live && !result.ExternalRootCount); }
        xrtValueRelease(captured); captured = NULL; ++transactions;
        xrtClearError(); xrtMemDebugSnapshot(&after);
        assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
        assert(after.AllocCount - before.AllocCount == after.FreeCount - before.FreeCount);
        assert(before.InvalidFreeCount == after.InvalidFreeCount && before.DoubleFreeCount == after.DoubleFreeCount);
        assert(before.UseAfterFreeCount == after.UseAfterFreeCount);
    }
    printf("Generated frame ownership: %u transactions, %u real Watch retirements, duplicate captures, cells, errors, native aliases, exact memory balance\n", transactions, handoffs);
    assert(transactions == 1500 && handoffs == 3100);
    return 0;
}
