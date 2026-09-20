/* Real generated closure environments inspected with the current XRT.
 * The whole graph is quiescent and generated code remains mapped throughout.
 * This tests graph integration, not module automatic code pinning. */
#include "xlir_capture_ownership.c"
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
    static const size_t nodes[] = {6,12,7,7,7,9,9};
    static const size_t edges[] = {5,12,6,7,6,8,8};
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
        /* Independent shell / shared backing, not just the same-shell alias. */
        alias = xrtValueClone(captured); assert(alias != captured);
        assert(xrtOwnershipInspectReachable(&root, 1, &root, 1, &reachable, &result, NULL, NULL));
        assert(result.ExternalRootCount == 1);
        /* The external backing reaches children, not the original shell. */
        assert(!reachable);
        invoke(xrtValueArrayGet(alias, 0), 43);
        invoke(xrtValueArrayGet(captured, 1), 43);
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
