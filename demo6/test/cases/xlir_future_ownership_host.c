/* Real generated Future/closure/Value graphs. The one pending source is owned
 * exclusively by this host and cannot complete during its inspection. All
 * other graph inspections happen after worker join; code always stays mapped. */
#include "xlir_future_ownership.c"
#include <assert.h>
#include <windows.h>

static xfuture* source;
static xvalue* captured;
static void opaque_watch_noop(void* data) { (void)data; }
xfuture* future_graph_source(void) { assert(source); return xrtFutureRef(source); }
void future_graph_capture(xvalue* value)
{ assert(captured == NULL && value != NULL); captured = xrtValueRetain(value); assert(captured); }

static void invoke_closure(xlir_closure* closure)
{
    int64_t answer = 0;
    assert(closure != NULL);
    assert(((bool (*)(void*, int64_t*))closure->invoke)(closure->environment, &answer));
    assert(answer == 42);
}
static void invoke_value(xvalue* callable)
{
    xrtcallframe frame = {0}; xrtcallresult result = XRT_CALL_RESULT_INIT;
    int64_t answer = 0;
    assert(xrtValueInvoke(callable, &frame, &result));
    assert(xrtCallResultCount(&result) == 1 && xrtValueGetInt(xrtCallResultGet(&result, 0), &answer) && answer == 42);
    xrtCallResultClear(&result);
}
int main(void)
{
    /* Task language transport adds a forwarding Future and a child cancel
     * token; Value-callable boxing adds callable/closure nodes. Resumed,
     * continuation and direct task results publish into one terminal Future. */
    static const size_t expected_nodes[] = {6,8,3,8,4,4,4};
    static const size_t expected_edges[] = {7,9,3,9,4,4,4};
    unsigned round, mode;
    assert(xrtMemDebugEnable(true));
    for (round = 0; round < 100; ++round) {
        xvalue* roots[7]; xpromise* producer; xmemdebugsnapshot before, after;
        xrtMemDebugSnapshot(&before);
        assert(xlir_module_init());
        producer = xrtPromiseCreate(&source, NULL); assert(producer);
        for (mode = 0; mode < 7; ++mode) {
            future_graph_build(mode); roots[mode] = captured; captured = NULL;
            assert(roots[mode] && !xrtGetError());
        }
        { /* The source's committed frame is now described. Its queue execution
           * lease is a genuine root, independent of the Watch storage lease. */
          xrtownershipref anchor = xrtFutureOwnership(source);
          xrtownershipresult old = {11,22,33,44}, result = old; bool live = true; xfuturewatch opaque;
          assert(xrtOwnershipInspectReachable(&anchor, 1, NULL, 0, &live, &result, NULL, NULL));
          assert(live && result.NodeCount == 5 && result.EdgeCount == 6 && result.ExternalRootCount == 3);
          /* Preserve the former fail-closed assertion for an ACTUAL opaque
           * native waiter; do not convert unknown callback data to a leaf. */
          assert(xrtFutureWatchInit(&opaque, opaque_watch_noop, opaque_watch_noop, NULL));
          assert(xrtFutureWatchAdd(source, &opaque) == XFUTURE_WATCH_PENDING); result = old;
          assert(!xrtOwnershipInspectReachable(&anchor, 1, NULL, 0, &live, &result, NULL, NULL));
          assert(live && !memcmp(&old, &result, sizeof(result))); xrtClearError();
          assert(xrtFutureWatchDetach(source, &opaque)); }
        { int64_t* value = xrtMalloc(sizeof(*value)); assert(value); *value = 42;
          assert(xl6PayloadResolveOwned(producer, value, XL6_FUTURE_PAYLOAD_VALUE)); }
        xrtPromiseDestroy(producer);
        for (mode = 0; mode < 7; ++mode)
            if (mode != 3) assert(xrtFutureWait(xrtValueGetFuture(roots[mode])) == XWAIT_OK);
        { DWORD started = GetTickCount();
          while (!xlir_continuation_quiesce()) { assert(GetTickCount() - started < 15000); Sleep(1); } }
        xrtFutureDestroy(source); source = NULL;
        xlir_module_fini(); assert(!xrtGetError());
        for (mode = 0; mode < 7; ++mode) {
            xrtownershipref root = xrtValueOwnership(roots[mode]);
            xrtownershipresult graph = {0}; bool live = true; xvalue* alias;
            assert(xrtOwnershipInspectReachable(&root, 1, &root, 1, &live, &graph, NULL, NULL));
            if (!round) printf("mode=%u nodes=%zu edges=%zu roots=%zu\n", mode, graph.NodeCount, graph.EdgeCount, graph.ExternalRootCount);
            assert(graph.ExternalRootCount == 0 && !live);
            assert(graph.NodeCount == expected_nodes[mode] && graph.EdgeCount == expected_edges[mode]);
            alias = xrtValueRetain(roots[mode]);
            assert(xrtOwnershipInspectReachable(&root, 1, &root, 1, &live, &graph, NULL, NULL) && live && graph.ExternalRootCount == 1);
            if (mode == 3) invoke_value(alias);
            else {
                xfuture* future = xrtValueGetFuture(alias); void* payload;
                assert(future && xrtFutureState(future) == XFUTURE_RESOLVED); payload = xrtFutureValue(future); assert(payload);
                if (mode == 1) invoke_value(*(xvalue**)payload);
                else if (mode == 2) assert(strcmp(*(char**)payload, "resident") == 0);
                else invoke_closure(*(xlir_closure**)payload);
            }
            xrtValueRelease(alias); xrtValueRelease(roots[mode]);
        }
        assert(!xrtGetError()); xrtMemDebugSnapshot(&after);
        assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
        assert(after.AllocCount - before.AllocCount == after.FreeCount - before.FreeCount);
        assert(before.InvalidFreeCount == after.InvalidFreeCount && before.DoubleFreeCount == after.DoubleFreeCount);
    }
    puts("Future ownership: 700 generated graphs, actual suspension, task spawn, aliases, invocations and exact memory balance");
    return 0;
}
