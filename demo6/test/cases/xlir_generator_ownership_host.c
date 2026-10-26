/* Every inspected graph is host-quiescent and generated code stays resident.
 * The queue substitution only controls scheduling; physical Watch/ticket and
 * generator/iterator implementations are the actual generated/source code. */
#define xl6ContinuationExecutorReserveOwned generator_test_reserve
#ifndef XLIR_GENERATOR_OWNERSHIP_SOURCE
#define XLIR_GENERATOR_OWNERSHIP_SOURCE "xlir_generator_ownership.c"
#endif
#include XLIR_GENERATOR_OWNERSHIP_SOURCE
#undef xl6ContinuationExecutorReserveOwned
#ifndef XLIR_GENERATOR_OWNERSHIP_BINDING
#define XLIR_GENERATOR_OWNERSHIP_BINDING "xlir_generator_ownership.bind.h"
#endif
#include XLIR_GENERATOR_OWNERSHIP_BINDING
#include "src/runtime/xl6_sequence.h"
#include "src/runtime/xl6_continuation_future.h"
#include <assert.h>
#include <stdio.h>

static XL6_ContinuationQueue* queue;
static xfuture* sources[2];
static xvalue* captured;
static xlir_generator* activeGenerator; /* borrowed inspection probe only */
static unsigned marks, busyRefusals, roots, inspectionProbes, inspectionFailures;
XL6_ContinuationAdmission generator_test_reserve(XL6_ContinuationExecutor* executor,
    XL6_ContinuationTicket* ticket, XL6_ContinuationProc run,
    XL6_ContinuationProc release, void* data, const XL6_CodeOwner* owner)
{
    (void)executor; assert(owner == NULL); ++roots;
    return xl6ContinuationReserve(queue, ticket, run, release, data);
}
xfuture* generator_graph_source(int64_t stage)
{ assert(stage == 0 || stage == 1); assert(sources[stage]); return xrtFutureRef(sources[stage]); }
void generator_graph_capture(xvalue* value)
{ assert(value && !captured); captured = xrtValueRetain(value); assert(captured); }
void generator_graph_mark(int64_t kind)
{
    if (kind == 0) ++marks; else assert(kind == 1);
    if (activeGenerator) {
        xrtownershipref ref = xlir_generator_ownership(activeGenerator);
        xrtownershipresult original = {91,92,93,94}, result = original;
        assert(activeGenerator->running && !xrtOwnershipInspect(&ref, 1, &ref, 1, &result));
        assert(!memcmp(&result, &original, sizeof(result))); xrtClearError(); ++busyRefusals;
    }
}
static void memory_equal(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after; xrtClearError(); xrtMemDebugSnapshot(&after);
    assert(before->LiveCount == after.LiveCount && before->LiveBytes == after.LiveBytes);
    assert(after.AllocCount-before->AllocCount == after.FreeCount-before->FreeCount);
    assert(before->InvalidFreeCount == after.InvalidFreeCount && before->DoubleFreeCount == after.DoubleFreeCount &&
        before->UseAfterFreeCount == after.UseAfterFreeCount);
}
static xlir_generator_step next(xlir_generator* generator)
{
    xlir_generator_step step; activeGenerator = generator;
    step = xlir_generator_next(generator); activeGenerator = NULL; return step;
}
static void close_generator(xlir_generator* generator)
{
    xl6_error_result result; activeGenerator = generator;
    result = xlir_generator_close(generator); activeGenerator = NULL;
    assert(result.ok && !result.error && !xrtGetError());
}
static bool count_edge(xrtownershipref ref, void* context)
{
    assert(ref.Data && ref.Ops); ++*(size_t*)context; return true;
}
typedef struct FrameEdgeProbe { xrtownershipref target; size_t count; } FrameEdgeProbe;
static bool frame_edge(xrtownershipref ref, void* context)
{
    FrameEdgeProbe* probe = context;
    if (ref.Data == probe->target.Data && ref.Ops == probe->target.Ops) ++probe->count;
    return true;
}
static void native_array_graph(const xrtownershipref* anchors, size_t anchorCount,
    const xrtownershipref* slots, size_t slotCount, bool nativeShared)
{
    /* Classify real adapters, not pointer names or generated frame offsets.
     * The original Array and its snapshot each have a shell and one native
     * owner. Each owner traces both duplicate elements. The iterator owns the
     * snapshot shell AND its current projection; View is not another edge.
     * The async source also creates a native one-element shared Array, while
     * direct cases deliberately pass a host-created wire Value Array. */
    xrtownershipsnapshot* snapshot = NULL;
    size_t sequences = 0, sequenceEdges = 0, iterators = 0, iteratorEdges = 0;
    xrtownershipref frameNodes[2]; size_t frameCount = 0;
    assert(xrtOwnershipSnapshotCreate(anchors, anchorCount, slots, slotCount, NULL, NULL, &snapshot));
    for (size_t i = 0; i < xrtOwnershipSnapshotNodeCount(snapshot); ++i) {
        xrtownershipnode node; assert(xrtOwnershipSnapshotNode(snapshot, i, &node));
        if (xl6SequenceOwnershipAdapterV1(node.Reference)) {
            ++sequences; assert(node.Reference.Ops->Trace(node.Reference.Data, count_edge, &sequenceEdges));
        }
        if (xl6ValueIteratorOwnershipAdapterV1(node.Reference)) {
            ++iterators; assert(node.Reference.Ops->Trace(node.Reference.Data, count_edge, &iteratorEdges));
        }
        if (nativeShared) {
            const xrtownershippreparationv1* preparation = NULL;
            if (xl6ContinuationFrameOwnershipAdapterV1(node.Reference, &preparation)) {
                assert(preparation && frameCount < 2); frameNodes[frameCount++] = node.Reference;
            }
        }
    }
    if (nativeShared) {
        /* The stable producer and its current physical frame own each other.
         * Neither edge is a synthetic external root or a historical frame. */
        assert(frameCount == 2 && frameNodes[0].Ops != frameNodes[1].Ops);
        for (size_t i = 0; i < 2; ++i) {
            FrameEdgeProbe probe = {frameNodes[1-i], 0};
            assert(frameNodes[i].Ops->Trace(frameNodes[i].Data, frame_edge, &probe));
            assert(probe.count == 1);
        }
    }
    xrtOwnershipSnapshotDestroy(snapshot);
    printf("native array graph: sequences=%zu edges=%zu iterators=%zu edges=%zu\n",
        sequences, sequenceEdges, iterators, iteratorEdges); fflush(stdout);
    assert(sequences == (nativeShared ? 6u : 4u) && sequenceEdges == (nativeShared ? 8u : 6u) &&
        iterators == 1 && iteratorEdges == 2);
}
static void direct_graph(unsigned round, unsigned kind, unsigned stage, xlir_generator* generator,
    xvalue* shared, xrtownershipref taken, xrtownershipref nestedItem)
{
    /* Columns: unstarted, first yield/take, second yield/take, finished,
     * closed, nested child suspended, nested child's item transferred. */
    /* Value: control/frame + shared Value/backing/int + native Array shell/
     * owner + snapshot shell/owner + language iterator = 10. Relative to the
     * retired wire cursor, the native snapshot adds one node and three edges
     * (snapshot shell, duplicate element, owned current projection). Forward
     * adds its control/frame, not a second item edge to the child after TAKE. */
    static const size_t nodes[8][9] = {
        {5,10,10,10,10,4,4,0,0}, {5,6,6,6,6,4,4,0,0}, {4,4,4,4,4,3,3,0,0}, {2,3,3,3,3,1,1,0,0},
        {5,7,7,7,7,4,4,14,14}, {2,2,2,2,2,1,1,0,0}, {2,2,2,2,2,1,1,0,0}, {5,12,12,12,12,4,4,0,0}};
    static const size_t edges[8][9] = {
        {6,18,18,18,18,4,4,0,0}, {6,9,9,9,9,4,4,0,0}, {5,7,7,7,7,4,4,0,0}, {2,4,4,4,4,1,1,0,0},
        {6,9,9,9,9,4,4,25,25}, {2,2,2,2,2,1,1,0,0}, {2,2,2,2,2,1,1,0,0}, {6,22,22,22,22,4,4,0,0}};
    xrtownershipref root = xlir_generator_ownership(generator), slots[6];
    size_t count = 0; xrtownershipresult result; bool live = true;
    slots[count++] = root;
    if (shared) slots[count++] = xrtValueOwnership(shared);
    if (sources[0]) { slots[count] = xrtFutureOwnership(sources[0]); slots[count+1] = slots[count]; count += 2; }
    if (taken.Data) slots[count++] = taken;
    if (nestedItem.Data) slots[count++] = nestedItem;
    assert(count <= 6 && xrtOwnershipInspectReachable(&root, 1, slots, count, &live, &result, NULL, NULL));
    assert(!live && result.ExternalRootCount == 0);
    if (!round) { printf("generator kind=%u stage=%u nodes=%zu edges=%zu\n", kind, stage, result.NodeCount, result.EdgeCount); fflush(stdout); }
    assert(result.NodeCount == nodes[kind][stage] && result.EdgeCount == edges[kind][stage]);
    if (!round && (((kind == 0 || kind == 7) && stage >= 1 && stage <= 4) ||
        (kind == 4 && (stage == 7 || stage == 8)))) native_array_graph(&root, 1, slots, count, false);
    if (stage == 1) for (unsigned fault = 0; fault < 10; ++fault) {
        xrtownershipresult original = {91,92,93,94}, failed = original;
        bool reachable = true, ok, triggered; xmemdebugsnapshot before;
        void* frame = generator->frame; unsigned state = generator->state;
        bool hasItem = generator->has_item;
        xrtMemDebugSnapshot(&before); assert(xrtMemDebugFailAfter(fault));
        ok = xrtOwnershipInspectReachable(&root, 1, slots, count, &reachable, &failed, NULL, NULL);
        triggered = xrtMemDebugFailTriggered(); xrtMemDebugFailClear(); ++inspectionProbes;
        if (triggered) {
            ++inspectionFailures;
            assert(!ok && reachable && !memcmp(&original, &failed, sizeof(failed)));
        } else assert(ok && !reachable && failed.NodeCount == nodes[kind][stage] && failed.EdgeCount == edges[kind][stage]);
        assert(generator->frame == frame && generator->state == state && generator->has_item == hasItem);
        memory_equal(&before);
    }
    if (shared) {
        xvalue* alias = xrtValueRetain(shared); xrtownershipresult aliased;
        assert(alias == shared && xrtOwnershipInspectReachable(&root, 1, slots, count, &live, &aliased, NULL, NULL));
        assert(!live && aliased.ExternalRootCount == 1 && aliased.NodeCount == result.NodeCount && aliased.EdgeCount == result.EdgeCount);
        xrtValueRelease(alias);
    }
    if (stage == 2 && kind >= 1 && kind <= 3) {
        if (kind == 1) assert(xlir_closure_retain((xlir_closure*)taken.Data));
        else if (kind == 2) assert(xrtFutureRef((xfuture*)taken.Data));
        else assert(xrtErrorRef((xerror*)taken.Data));
        assert(xrtOwnershipInspectReachable(&root, 1, slots, count, &live, &result, NULL, NULL));
        assert(!live && result.ExternalRootCount == 1 && result.NodeCount == nodes[kind][stage] && result.EdgeCount == edges[kind][stage]);
        if (kind == 1) xlir_closure_release((xlir_closure*)taken.Data);
        else if (kind == 2) xrtFutureDestroy((xfuture*)taken.Data);
        else xrtErrorFree((xerror*)taken.Data);
    }
}
typedef union Item {
    xvalue* value; xlir_closure* closure; xfuture* future; xerror* error;
    xlir_generator* generator; xlir_string string; int64_t scalar;
} Item;
/* Item is scratch storage, not the ABI width of every yielded type. */
static size_t item_size(unsigned kind)
{
    switch (kind) {
    case 0: case 7: return sizeof(((Item*)0)->value);
    case 1: return sizeof(((Item*)0)->closure);
    case 2: return sizeof(((Item*)0)->future);
    case 3: return sizeof(((Item*)0)->error);
    case 4: return sizeof(((Item*)0)->generator);
    case 5: return sizeof(((Item*)0)->string);
    case 6: return sizeof(((Item*)0)->scalar);
    default: assert(0); return 0;
    }
}
static xrtownershipref item_ref(unsigned kind, Item item)
{
    if (kind == 0 || kind == 7) return xrtValueOwnership(item.value);
    if (kind == 1) return xlir_closure_ownership(item.closure);
    if (kind == 2) return xrtFutureOwnership(item.future);
    if (kind == 3) return xrtErrorOwnership(item.error);
    if (kind == 4) return xlir_generator_ownership(item.generator);
    return (xrtownershipref){NULL, NULL};
}
static void check_item(unsigned kind, unsigned index, Item item, xvalue* shared)
{
    if (kind == 0 || kind == 7) assert(item.value == shared);
    else if (kind == 1) {
        int64_t answer = 0;
        assert(((bool(*)(void*, int64_t*))item.closure->invoke)(item.closure->environment, &answer) && answer == 41);
    } else if (kind == 2) assert(item.future == sources[0]);
    else if (kind == 3) assert(item.error && !strcmp(xrtErrorMessage(item.error), "generator owned error"));
    else if (kind == 4) assert(item.generator);
    else if (kind == 5) assert(item.string.Data && item.string.Size == (index ? 13u : 12u) &&
        !memcmp(item.string.Data, index ? "second string" : "owned string", item.string.Size));
    else assert(item.scalar == (int64_t)(41 + index));
}
static void drop_item(unsigned kind, Item item)
{
    if (kind == 0 || kind == 7) xrtValueRelease(item.value);
    else if (kind == 1) xlir_closure_release(item.closure);
    else if (kind == 2) xrtFutureDestroy(item.future);
    else if (kind == 3) xrtErrorFree(item.error);
    else if (kind == 4) xlir_generator_destroy(item.generator);
    else if (kind == 5) xrtFree((void*)item.string.Data);
}
static void direct_case(unsigned round, unsigned kind, unsigned outcome)
{
    xmemdebugsnapshot before; xvalue* shared = NULL; xpromise* producer = NULL;
    xlir_generator* generator; xlir_result_generator started; xerror* firstError = NULL;
    xrtownershipref empty = {NULL, NULL};
    xrtMemDebugSnapshot(&before); marks = 0; assert(xlir_module_init());
    if (kind == 0 || kind == 1 || kind == 4 || kind == 7) {
        shared = xrtValueArray(); assert(shared && xrtValueArrayAppendNew(shared, xrtValueInt(41)));
    }
    if (kind == 2) { producer = xrtPromiseCreate(&sources[0], NULL); assert(producer); }
    switch (kind) {
    case 0: started = GENERATOR_GRAPH_0(shared); break;
    case 1: started = GENERATOR_GRAPH_1(shared); break;
    case 2: started = GENERATOR_GRAPH_2(); break;
    case 3: started = GENERATOR_GRAPH_3(); break;
    case 4: started = GENERATOR_GRAPH_4(shared); break;
    case 5: started = GENERATOR_GRAPH_5(); break;
    case 6: started = GENERATOR_GRAPH_6(); break;
    default: started = GENERATOR_GRAPH_7(shared); break;
    }
    assert(started.ok && started.value && !xrtGetError()); generator = started.value;
    direct_graph(round || outcome != 1, kind, 0, generator, shared, empty, empty);
    if (outcome) {
        for (unsigned itemIndex = 0; itemIndex < 2; ++itemIndex) {
            xlir_generator_step step = next(generator); Item item = {0};
            assert(step.ok && step.value && generator->has_item);
            direct_graph(round || outcome != 1, kind, itemIndex ? 3 : 1, generator, shared, empty, empty);
            if (outcome == 2) continue; /* actual current-item discard on next/close */
            assert(xlir_generator_item_get(generator, &item, item_size(kind), kind != 6));
            check_item(kind, itemIndex, item, shared);
            if (kind == 3) { if (!itemIndex) firstError = item.error; else assert(item.error == firstError); }
            direct_graph(round, kind, itemIndex ? 4 : 2, generator, shared, item_ref(kind, item), empty);
            if (kind == 4) {
                for (unsigned n = 0; n < 2; ++n) {
                    xlir_generator_step child = next(item.generator); xvalue* value = NULL;
                    assert(child.ok && child.value);
                    direct_graph(round || itemIndex || n, kind, 7, generator, shared, item_ref(kind, item), empty);
                    assert(xlir_generator_item_get(item.generator, &value, sizeof(value), true) && value == shared);
                    direct_graph(round || itemIndex || n, kind, 8, generator, shared, item_ref(kind, item), xrtValueOwnership(value));
                    xrtValueRelease(value);
                }
                { xlir_generator_step child = next(item.generator); assert(child.ok && !child.value); }
            }
            drop_item(kind, item);
        }
        if (outcome == 1) {
            xlir_generator_step step = next(generator); assert(step.ok && !step.value && !generator->frame);
            direct_graph(round, kind, 5, generator, shared, empty, empty);
        }
    }
    close_generator(generator);
    direct_graph(round || outcome != 2, kind, 6, generator, shared, empty, empty);
    xlir_generator_destroy(generator); xrtValueRelease(shared);
    if (producer) { xrtPromiseDestroy(producer); xrtFutureDestroy(sources[0]); sources[0] = NULL; }
    assert(marks == ((kind == 0 || kind == 7) && outcome ? 1u : kind == 4 && outcome == 1 ? 2u : 0u));
    xlir_module_fini(); assert(!xrtGetError()); memory_equal(&before);
}
static void async_graph(unsigned round, unsigned mode, unsigned stage)
{
    /* Current native Array/iterator storage plus the resident producer/frame
     * cycle, checked structurally below. Keep every real duplicate edge. */
    static const size_t nodes[2] = {17,19}, edges[2] = {30,34};
    xrtownershipref anchors[4], slots[5]; xrtownershipresult result; bool live[4];
    anchors[0] = xrtFutureOwnership(sources[0]); anchors[1] = xrtFutureOwnership(sources[1]);
    anchors[2] = xrtValueOwnership(captured); anchors[3] = xrtFutureOwnership(xrtValueGetFuture(captured));
    slots[0] = slots[1] = anchors[0]; slots[2] = slots[3] = anchors[1]; slots[4] = anchors[2];
    assert(xrtOwnershipInspectReachable(anchors, 4, slots, 5, live, &result, NULL, NULL));
    assert(result.ExternalRootCount == 1 && result.ReachableAnchorCount == 2 && !live[2] && live[3]); /* real queue execution root */
    if (!round) { printf("async iterator mode=%u stage=%u nodes=%zu edges=%zu reachable=%zu\n", mode, stage, result.NodeCount, result.EdgeCount, result.ReachableAnchorCount); fflush(stdout); }
    assert(result.NodeCount == nodes[mode] && result.EdgeCount == edges[mode]);
    if (!round) native_array_graph(anchors, 4, slots, 5, true);
}
static void resolve(xpromise* promise)
{
    int64_t* value = xrtMalloc(sizeof(*value)); assert(value); *value = 1;
    assert(xl6PayloadResolveOwned(promise, value, XL6_FUTURE_PAYLOAD_VALUE));
}
static void async_case(unsigned round, unsigned mode, unsigned outcome)
{
    xmemdebugsnapshot before; xpromise* producers[2]; xfuture* output; xerror* primary = NULL;
    xrtMemDebugSnapshot(&before); marks = roots = 0; assert(xlir_module_init());
    queue = xl6ContinuationQueueCreate(1); assert(queue);
    for (unsigned i = 0; i < 2; ++i) { producers[i] = xrtPromiseCreate(&sources[i], NULL); assert(producers[i]); }
    generator_graph_build(mode); assert(captured && roots == 1 && !xrtGetError());
    output = xrtValueGetFuture(captured); assert(output && xrtFutureState(output) == XFUTURE_PENDING);
    /* Launch now queues the initial activation too. Deliver it explicitly
     * before inspecting the original first-await graph; no worker may race
     * this host-controlled queue, and no extra ready work may remain. */
    assert(xl6ContinuationRunOne(queue) && !xl6ContinuationRunOne(queue));
    async_graph(round || outcome, mode, 0);
    xl6ContinuationQueueClose(queue); resolve(producers[0]); assert(xl6ContinuationRunOne(queue));
    assert(xrtFutureState(output) == XFUTURE_PENDING && roots == 1); async_graph(round || outcome, mode, 1);
    if (outcome == 0) resolve(producers[1]);
    else if (outcome == 1) {
        primary = xrtErrorCreate(XERR_INTERNAL, "generator graph", 9, "original failure");
        assert(primary && xrtPromiseReject(producers[1], primary));
    } else if (outcome == 2) assert(xrtPromiseClose(producers[1]));
    else if (outcome == 3) assert(xrtPromiseCancel(producers[1]));
    else { assert(xrtFutureCancel(output)); resolve(producers[1]); }
    assert(xl6ContinuationRunOne(queue) && !xl6ContinuationRunOne(queue));
    assert(xl6ContinuationQueueDestroy(queue)); queue = NULL;
    if (outcome == 0) assert(xrtFutureState(output) == XFUTURE_RESOLVED && *(int64_t*)xrtFutureValue(output) == 84);
    else {
        xfutureresult result = {0}; assert(xrtFutureResult(output, &result));
        assert(result.State == (outcome >= 3 ? XFUTURE_CANCELLED : outcome == 2 ? XFUTURE_CLOSED : XFUTURE_FAILED));
        if (outcome == 1) assert(result.Error == primary);
    }
    assert(marks == mode); xrtErrorFree(primary);
    for (unsigned i = 0; i < 2; ++i) { xrtPromiseDestroy(producers[i]); xrtFutureDestroy(sources[i]); sources[i] = NULL; }
    xrtValueRelease(captured); captured = NULL; xlir_module_fini(); assert(!xrtGetError()); memory_equal(&before);
}
int main(void)
{
    assert(xrtMemDebugEnable(true));
    for (unsigned round = 0; round < 100; ++round) {
        for (unsigned kind = 0; kind < 8; ++kind) for (unsigned outcome = 0; outcome < 3; ++outcome) direct_case(round, kind, outcome);
        for (unsigned mode = 0; mode < 2; ++mode) for (unsigned outcome = 0; outcome < 5; ++outcome) async_case(round, mode, outcome);
    }
    assert(busyRefusals == 1800 && inspectionProbes == 16000 && inspectionFailures == 6600);
    printf("Generated generator ownership: 2400 lifecycles, 1000 async transactions, 2000 real suspensions, %u running inspection refusals, %u inspection probes, %u actual failures, exact memory balance\n",
        busyRefusals, inspectionProbes, inspectionFailures);
    return 0;
}
