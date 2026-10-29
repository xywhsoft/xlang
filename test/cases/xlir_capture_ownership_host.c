/* Real generated closure environments inspected with the current XRT.
 * The whole graph is quiescent and generated code remains mapped throughout.
 * This tests graph integration, not module automatic code pinning. */
#include "xlir_capture_ownership.c"
#include "src/runtime/xl6_array_erased.h"
#include <assert.h>
#include <string.h>

static xvalue* captured;
void graph_capture(xvalue* value)
{
    assert(value != NULL && captured == NULL);
    captured = xrtValueRetain(value);
    assert(captured != NULL);
}

static void invoke(xvalue* callable, int64_t expected)
{
    xrtcallframe frame = {0}; xrtcallresult result = XRT_CALL_RESULT_INIT;
    int64_t actual = 0;
    assert(xrtValueInvoke(callable, &frame, &result));
    assert(xrtCallResultCount(&result) == 1);
    assert(xrtValueGetInt(xrtCallResultGet(&result, 0), &actual) && actual == expected);
    xrtCallResultClear(&result);
}

static void inspect(unsigned mode, unsigned round)
{
    /* The bound class receiver (mode 4) owns a resident finalizer record.
     * That physical node/edge already exists in the frozen v168 producer;
     * it is not an external root and must not be hidden from this count. */
    /* Primitive native Array storage owns an unboxed int64 slot, not the
     * former wire Array's separate integer Value node and edge. */
    /* The bound receiver also owns its ordinary runtime-type Value field. */
    static const size_t nodes[] = {5,11,6,6,8,8,8};
    static const size_t edges[] = {4,11,5,6,7,7,7};
    xrtownershipref root = xrtValueOwnership(captured);
    xrtownershipresult result = {0}; bool reachable = true;
    xvalue* alias;
    assert(xrtOwnershipInspectReachable(&root, 1, NULL, 0, &reachable, &result, NULL, NULL));
    if (!round) printf("mode %u nodes=%zu edges=%zu external=%zu\n", mode,
        result.NodeCount, result.EdgeCount, result.ExternalRootCount);
    assert(result.NodeCount == nodes[mode] && result.EdgeCount == edges[mode]);
    assert(result.ExternalRootCount == 1 && reachable);
    /* Subtract exactly this host's captured slot; anchors themselves are not
     * roots. A separate native retain must still keep the entire graph live. */
    assert(xrtOwnershipInspectReachable(&root, 1, &root, 1, &reachable, &result, NULL, NULL));
    assert(result.ExternalRootCount == 0 && !reachable);
    alias = xrtValueRetain(captured); assert(alias == captured);
    assert(xrtOwnershipInspectReachable(&root, 1, &root, 1, &reachable, &result, NULL, NULL));
    assert(result.ExternalRootCount == 1 && reachable);
    xrtValueRelease(alias);
    if (mode == 1) {
        /* Raw XRT handles preserve identity; language shallow clone copies
         * native Array storage and keeps each callable element reference. */
        alias = xrtValueClone(captured); assert(alias == captured);
        xrtValueRelease(alias);
        alias = xl6ValueCloneV1(captured); assert(alias != captured);
        assert(xrtOwnershipInspectReachable(&root, 1, &root, 1, &reachable, &result, NULL, NULL));
        assert(result.ExternalRootCount == 2);
        /* Both shared callable elements are external roots; the cloned
         * storage does not keep the original container shell reachable. */
        assert(!reachable);
        xvalue* first = xl6ArrayErasedReadV1(alias, 0);
        assert(first && !xrtGetError()); invoke(first, 43); xrtValueRelease(first);
        xvalue* second = xl6ArrayErasedReadV1(captured, 1);
        assert(second && !xrtGetError()); invoke(second, 43); xrtValueRelease(second);
        xrtValueRelease(alias);
    } else invoke(captured, mode == 3 ? 84 : 42);
    assert(xrtOwnershipInspectReachable(&root, 1, &root, 1, &reachable, &result, NULL, NULL));
    assert(result.ExternalRootCount == 0 && !reachable);
}

static void entry(void) {}
static bool reject_visit(xrtownershipref ref, void* context)
{ (void)ref; (void)context; return false; }
static bool reject_environment(const void* data, XL6_ModuleOwnershipVisit visit, void* context)
{ (void)data; (void)visit; (void)context; return false; }
static void drop_environment(void* data) { *(int*)data = 1; }

static void invalid_graph(void)
{
    int opaque = 0;
    /* Native arbitrary-environment tests intentionally exercise legacy V1;
     * generated V2 environments use closed layouts, not these callbacks. */
    xlir_closure* closure = xl6ClosureRuntimeV1Get()->Create(xlir_closure_owner, entry, NULL, &opaque, NULL, NULL, NULL, 0, NULL);
    /* Resident creation rejects absent lifecycle contracts before publication.
     * Separately retain the original inspection-rollback check using a real
     * admitted adapter whose Trace refuses the graph; do not skip that case. */
    assert(closure == NULL && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT && opaque == 0);
    xrtClearError();
    closure = xl6ClosureRuntimeV1Get()->Create(xlir_closure_owner, entry, NULL, &opaque, drop_environment, NULL, NULL, 0, reject_environment);
    xrtownershipref root = xlir_closure_ownership(closure);
    xrtownershipresult before = {11,22,33,44}, result = before;
    bool reachable = true;
    assert(closure != NULL);
    assert(!xrtOwnershipInspectReachable(&root, 1, NULL, 0, &reachable, &result, NULL, NULL));
    assert(!memcmp(&result, &before, sizeof(result)) && reachable);
    xrtClearError(); xlir_closure_release(closure); assert(opaque == 1);
    graph_build(0); assert(captured != NULL);
    /* Adapter failures must propagate through native callable -> closure. */
    root = xrtValueOwnership(captured);
    assert(!root.Ops->Trace(root.Data, reject_visit, NULL));
    xrtValueRelease(captured); captured = NULL;
}

int main(void)
{
    unsigned round, mode; xmemdebugsnapshot before, after;
    assert(xrtMemDebugEnable(true));
    for (round = 0; round < 100; ++round) {
        xrtClearError(); xrtMemDebugSnapshot(&before);
        for (mode = 0; mode < 7; ++mode) {
            graph_build(mode); assert(captured != NULL && xrtGetError() == NULL);
            inspect(mode, round);
            xrtValueRelease(captured); captured = NULL;
        }
        invalid_graph(); assert(xrtGetError() == NULL);
        xrtMemDebugSnapshot(&after);
        assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
        assert(after.AllocCount - before.AllocCount == after.FreeCount - before.FreeCount);
    }
    puts("capture ownership: 700 generated graphs, aliases, calls and exact memory balance complete");
    return 0;
}
