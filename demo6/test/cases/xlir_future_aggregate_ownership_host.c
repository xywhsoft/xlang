/* Actual generated aggregate calls and payload boxers. The module remains
 * mapped throughout; graph completeness alone is not unload permission. */
#include "xlir_future_aggregate_ownership.c"
#include "src/api/xl_future_bridge.h"
#include "src/runtime/xl6_future_payload.h"
#include <assert.h>

static xfuture* sources[2];
static xvalue* captured;
xfuture* aggregate_source(int64_t index)
{ assert(index >= 0 && index < 2); return xrtFutureRef(sources[index]); }
xfuture* aggregate_closure_source(int64_t index) { return aggregate_source(index); }
void aggregate_capture(xvalue* value)
{ assert(value && !captured); captured = xrtValueRetain(value); assert(captured); }

static void inspect(xrtownershipref* anchors, size_t anchor_count, xrtownershipref* slots, size_t slot_count,
    size_t nodes, size_t edges, size_t roots)
{
    xrtownershipresult result = {0};
    assert(xrtOwnershipInspect(anchors, anchor_count, slots, slot_count, &result));
    if (result.NodeCount != nodes || result.EdgeCount != edges || result.ExternalRootCount != roots)
        fprintf(stderr, "aggregate graph %zu/%zu/%zu expected %zu/%zu/%zu\n", result.NodeCount, result.EdgeCount, result.ExternalRootCount, nodes, edges, roots);
    assert(result.NodeCount == nodes && result.EdgeCount == edges && result.ExternalRootCount == roots);
}
static void publish(xpromise* producer, unsigned mode, int64_t value)
{
    if (mode == 6) {
        xvalue** box;
        aggregate_make_closure(value); assert(captured && !xrtGetError());
        box = xrtMalloc(sizeof(*box)); assert(box); *box = captured; captured = NULL;
        assert(xl6PayloadResolveOwned(producer, box, XL6_FUTURE_PAYLOAD_XVALUE));
    } else {
        int64_t* box = xrtMalloc(sizeof(*box)); assert(box); *box = value;
        assert(xl6PayloadResolveOwned(producer, box, XL6_FUTURE_PAYLOAD_VALUE));
    }
}
static void invoke(xvalue* callable, int64_t expected)
{
    xrtcallframe frame = {0}; xrtcallresult result = XRT_CALL_RESULT_INIT; int64_t value = 0;
    assert(xrtValueInvoke(callable, &frame, &result));
    assert(xrtCallResultCount(&result) == 1 && xrtValueGetInt(xrtCallResultGet(&result, 0), &value) && value == expected);
    xrtCallResultClear(&result);
}
static void graph_round(unsigned mode, unsigned outcome)
{
    xpromise* producers[2]; xvalue* output; xfuture* future; xrtownershipref slots[5];
    xerror* original = NULL; bool all = mode == 0 || mode == 3 || mode == 6;
    bool any = mode == 2 || mode == 5; size_t nodes, edges;
    producers[0] = xrtPromiseCreate(&sources[0], NULL); producers[1] = xrtPromiseCreate(&sources[1], NULL);
    assert(producers[0] && producers[1]); aggregate_build(mode); assert(captured && !xrtGetError());
    output = captured; captured = NULL; future = xrtValueGetFuture(output); assert(future && !xrtFutureDone(future));
    slots[0] = xrtFutureOwnership(sources[0]); slots[1] = xrtFutureOwnership(sources[1]);
    slots[2] = xrtPromiseOwnership(producers[0]); slots[3] = xrtPromiseOwnership(producers[1]); slots[4] = xrtValueOwnership(output);
    /* Real mapper context and owned cancellation watch are separate nodes;
     * projected source watches retain the shared group, not an operation root. */
    inspect(slots, 1, slots, 5, 10, 21, 0);
    if (outcome == 2) assert(xrtFutureCancel(future));
    else {
        if (outcome == 1) {
            original = xrtErrorCreate(XERR_PROTOCOL, "aggregate.test", 42, "source failure"); assert(original);
            assert(xrtPromiseReject(producers[1], original));
        } else if (outcome == 3) assert(xrtPromiseClose(producers[1]));
        else if (outcome == 4) assert(xrtPromiseCancel(producers[1]));
        else publish(producers[1], mode, 84000);
        if (all) assert(!xrtFutureDone(future)); else assert(xrtFutureDone(future));
        publish(producers[0], mode, 42000);
        if (outcome == 1 && !any) assert(xrtFutureError(future) == original);
    }
    xrtErrorFree(original);
    xrtPromiseDestroy(producers[0]); xrtPromiseDestroy(producers[1]);
    xrtFutureDestroy(sources[0]); xrtFutureDestroy(sources[1]); sources[0] = sources[1] = NULL;
    if (outcome == 2) { assert(xrtFutureState(future) == XFUTURE_CANCELLED); nodes = edges = 3; }
    else if (any) { assert(xrtFutureState(future) == XFUTURE_RESOLVED && *(int64_t*)xrtFutureValue(future) == 2); nodes = edges = 3; }
    /* XRT Forward retains the source only for successful borrowed payloads.
     * Failed output retains the Error directly; closed output has no owner. */
    else if (outcome == 1) { assert(xrtFutureState(future) == XFUTURE_FAILED); nodes = edges = 4; }
    else if (outcome == 3) { assert(xrtFutureState(future) == XFUTURE_CLOSED); nodes = edges = 3; }
    else if (outcome == 4) { assert(xrtFutureState(future) == XFUTURE_CANCELLED); nodes = edges = 3; }
    else if (all) {
        xvalue* array = *(xvalue**)xrtFutureValue(future); assert(xrtValueCount(array) == 3);
        for (size_t i = 0; i < 3; ++i) {
            int64_t expected = i == 2 ? 84000 : 42000, value;
            xvalue* item = xrtValueArrayGet(array, i);
            if (mode == 6) invoke(item, expected);
            else assert(xrtValueGetInt(item, &value) && value == expected);
        }
        if (mode == 6) assert(xrtValueArrayGet(array, 0) == xrtValueArrayGet(array, 1));
        nodes = mode == 6 ? 11 : 8; edges = mode == 6 ? 12 : 8;
    } else { assert(*(int64_t*)xrtFutureValue(future) == 84000); nodes = edges = 5; }
    slots[0] = xrtValueOwnership(output); inspect(slots, 1, slots, 1, nodes, edges, 0);
    { xvalue* alias = xrtValueRetain(output); inspect(slots, 1, slots, 1, nodes, edges, 1); xrtValueRelease(alias); }
    xrtValueRelease(output); assert(!captured && !xrtGetError());
}
static xvalue* box_scalar(const void* payload) { return xrtValueInt(*(const int64_t*)payload); }
static void launch_failures(void)
{
    unsigned rejected = 0, mapped_failures = 0;
    for (unsigned mode = 0; mode < 4; ++mode) for (unsigned ready = 0; ready < 2; ++ready) {
        unsigned accepted = 0, failed = 0, mapping_failed = 0;
        for (unsigned point = 0; point < 24; ++point) {
            xfuture* inputs[3]; xfuture* output; xpromise* producers[2]; xrtownershipref anchors[2], slots[4];
            xmemdebugsnapshot before, after; bool triggered;
            xrtMemDebugSnapshot(&before);
            producers[0] = xrtPromiseCreate(&sources[0], NULL); producers[1] = xrtPromiseCreate(&sources[1], NULL);
            assert(producers[0] && producers[1]); inputs[0] = sources[0]; inputs[1] = sources[0]; inputs[2] = sources[1];
            if (ready) { publish(producers[0], 0, 42000); publish(producers[1], 0, 84000); }
            assert(xrtMemDebugFailAfter(point));
            output = mode == 0 ? xl6FutureAllXValues(inputs, 3, box_scalar) :
                mode == 1 ? xl6FutureRaceChecked(inputs, 3) : mode == 2 ? xl6FutureWhenAnyIndex(inputs, 3) : xl6FutureAllChecked(inputs, 3);
            triggered = xrtMemDebugFailTriggered(); xrtMemDebugFailClear(); xrtClearError();
            if (!output) {
                ++failed; ++rejected; assert(triggered);
                for (size_t i = 0; i < 2; ++i) {
                    xcancel* cancel = xrtFutureCancelToken(sources[i]); assert(cancel && !xrtCancelRequested(cancel)); xrtCancelDestroy(cancel);
                    assert(xrtFutureDone(sources[i]) == (bool)ready);
                    anchors[i] = slots[i] = xrtFutureOwnership(sources[i]); slots[i + 2] = xrtPromiseOwnership(producers[i]);
                }
                inspect(anchors, 2, slots, 4, 4, 6, 0);
            } else {
                ++accepted;
                if (!ready) assert(xrtFutureCancel(output));
                else if (xrtFutureState(output) == XFUTURE_FAILED) {
                    ++mapping_failed; ++mapped_failures;
                    assert(triggered && xrtErrorKind(xrtFutureError(output)) == XERR_MEMORY);
                } else assert(xrtFutureState(output) == XFUTURE_RESOLVED);
                { xrtownershipresult graph = {0}; xrtownershipref root = xrtFutureOwnership(output);
                  assert(xrtOwnershipInspect(&root, 1, NULL, 0, &graph)); }
                xrtFutureDestroy(output);
            }
            xrtPromiseDestroy(producers[0]); xrtPromiseDestroy(producers[1]);
            xrtFutureDestroy(sources[0]); xrtFutureDestroy(sources[1]); sources[0] = sources[1] = NULL;
            xrtClearError(); xrtMemDebugSnapshot(&after);
            assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
            assert(after.AllocCount - before.AllocCount == after.FreeCount - before.FreeCount);
            assert(before.InvalidFreeCount == after.InvalidFreeCount && before.DoubleFreeCount == after.DoubleFreeCount);
        }
        assert(failed && accepted);
        if (ready && (mode == 0 || mode == 2)) assert(mapping_failed); /* Race/void do not allocate a new payload. */
    }
    printf("Aggregate bridge launch: 192 probes, %u preparation failures, %u accepted mapping failures\n", rejected, mapped_failures);
}
int main(void)
{
    assert(xrtMemDebugEnable(true));
    for (unsigned round = 0; round < 100; ++round) {
        xmemdebugsnapshot before, after; xrtMemDebugSnapshot(&before); assert(xlir_module_init());
        for (unsigned mode = 0; mode < 7; ++mode)
            for (unsigned outcome = 0; outcome < 5; ++outcome) graph_round(mode, outcome);
        xlir_module_fini(); assert(!xrtGetError()); xrtMemDebugSnapshot(&after);
        assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
        assert(after.AllocCount - before.AllocCount == after.FreeCount - before.FreeCount);
        assert(before.InvalidFreeCount == after.InvalidFreeCount && before.DoubleFreeCount == after.DoubleFreeCount);
    }
    launch_failures();
    puts("Future aggregate ownership: 3500 generated transactions, pending and terminal graphs, source errors, cancellation, closure aliases and exact memory");
    return 0;
}
