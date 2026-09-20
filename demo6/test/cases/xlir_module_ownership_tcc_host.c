/* Actual generated global views and bound host identity. The host explicitly
 * owns a shared code lease / accepted ticket: this is NOT automatic object pin.
 * All inspection runs on this sole thread with the manual queue stopped. */
#define JOINT_TCC_ASYNC
#define XL_ENABLE_EXE_COMPILER 1
#define JOINT_TCC_SOURCE_FILE "test/cases/xlir_module_ownership_artifact.c"
#define main unused_module_lifetime_main
#include "xlir_one_shot_tcc_host.c"
#undef main
#include "src/runtime/xl6_code_lease.h"

static xvalue* graph_alias;
static unsigned graph_runs, graph_releases, graph_refusals, graph_probes, graph_failures;
static int64_t (*graph_run)(void);
typedef struct GraphFrame { XL6_ContinuationTicket ticket; xvalue* value; unsigned index; } GraphFrame;
static GraphFrame graph_frames[2];
static xrtownershipresult graph_inspect(xrtownershipref anchor, size_t nodes, size_t edges, size_t roots)
{
    xrtownershipresult result = {0};
    xlApiLock(); assert(xrtOwnershipInspect(&anchor, 1, NULL, 0, &result)); xlApiUnlock();
    if (result.NodeCount != nodes || result.EdgeCount != edges || result.ExternalRootCount != roots)
        fprintf(stderr, "graph shape actual=%zu/%zu/%zu expected=%zu/%zu/%zu\n",
            result.NodeCount, result.EdgeCount, result.ExternalRootCount, nodes, edges, roots);
    assert(result.NodeCount == nodes && result.EdgeCount == edges && result.ExternalRootCount == roots &&
        result.ReachableAnchorCount == 1);
    return result;
}
static void graph_refuse(xrtownershipref anchor)
{
    xrtownershipresult result = {71, 72, 73, 74}, saved = result; bool reachable = true;
    xlApiLock();
    assert(!xrtOwnershipInspectReachable(&anchor, 1, NULL, 0, &reachable, &result, NULL, NULL));
    xlApiUnlock();
    assert(reachable && !memcmp(&result, &saved, sizeof(result))); xrtClearError(); ++graph_refusals;
}
static void graph_faults(xrtownershipref anchor, const xrtownershipresult* expected)
{
    for (size_t point = 0; point < 12; ++point) {
        xrtownershipresult result = {81, 82, 83, 84}, saved = result;
        xmemdebugsnapshot before, after; bool reachable = false, ok, failed;
        xrtMemDebugSnapshot(&before); assert(xrtMemDebugFailAfter(point));
        xlApiLock(); ok = xrtOwnershipInspectReachable(&anchor, 1, NULL, 0, &reachable, &result, NULL, NULL); xlApiUnlock();
        failed = xrtMemDebugFailTriggered(); xrtMemDebugFailClear(); ++graph_probes;
        if (failed) {
            ++graph_failures; assert(!ok && !reachable && !memcmp(&result, &saved, sizeof(result)));
        } else assert(ok && reachable && !memcmp(&result, expected, sizeof(result)));
        xrtClearError(); xrtMemDebugSnapshot(&after);
        assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
    }
}
static void graph_capture(xvalue* value)
{
    int64_t number = 0;
    assert(value && !atomic_load(&deleted) && xrtValueGetInt(value, &number) && number == 42);
    if (module->activeCalls) graph_refuse((xrtownershipref){module, &xlModuleOwnershipOps});
    if (!graph_alias) graph_alias = xrtValueRetain(value);
}
static bool graph_frame_trace(const void* data, xrtownershipvisitor visit, void* context)
{
    const GraphFrame* frame = data;
    return visit != NULL && visit(xrtValueOwnership(frame->value), context);
}
static void graph_frame_run(void* data);
static void graph_frame_release(void* data)
{
    GraphFrame* frame = data;
    assert(!atomic_load(&deleted) && module->codeLeases == 2);
    graph_refuse(xl6ContinuationOwnership(&frame->ticket));
    xrtValueRelease(frame->value); frame->value = NULL; ++graph_releases;
}
static void graph_frame_run(void* data)
{
    GraphFrame* frame = data;
    assert(module->destroyRequested && !atomic_load(&deleted) && module->codeLeases == 2);
    graph_refuse(xl6ContinuationOwnership(&frame->ticket));
    assert(graph_run() == 42 && xrtGetError() == NULL); ++graph_runs;
    if (frame->index == 0) {
        GraphFrame* next = &graph_frames[1]; next->index = 1;
        next->value = xrtValueRetain(frame->value);
        assert(xl6ContinuationReserveSuccessor(&frame->ticket, &next->ticket,
            graph_frame_run, graph_frame_release, next) == XL6_CONTINUATION_ADMITTED);
        assert(next->ticket.codeLease == frame->ticket.codeLease && next->ticket.codeLease != NULL);
        assert(xl6ContinuationTraceBind(&next->ticket, graph_frame_trace));
        xl6ContinuationReady(&next->ticket); xl6ContinuationTransportReleased(&next->ticket);
        xl6ContinuationCommit(&next->ticket);
        graph_refuse(xl6ContinuationOwnership(&next->ticket));
    }
}
int main(void)
{
    unsigned round, path;
    assert(xrtMemDebugEnable(true)); expected_cleanups = 0; provider_init();
    for (round = 0; round < 25; ++round) for (path = 0; path < 2; ++path) {
        typedef void (*BindCapture)(void (*)(xvalue*));
        typedef const XL6_CodeOwner* (*GetOwner)(void);
        BindCapture bind; GetOwner get_owner; const XL6_CodeOwner* owner;
        XL6_CodeLease* lease = NULL; xrtownershipref view; xrtownershipresult shape;
        XL6_ContinuationQueue* queue = NULL;
        xmemdebugsnapshot before, after; void* symbol;
        memset(&context, 0, sizeof(context)); memset(graph_frames, 0, sizeof(graph_frames));
        graph_alias = NULL; graph_runs = graph_releases = 0;
        atomic_store(&deleted, 0); atomic_store(&cleanups, 0);
        xrtClearError(); xrtMemDebugSnapshot(&before); load_artifact();
        symbol = lookup(compiled_state, "module_graph_bind_capture"); memcpy(&bind, &symbol, sizeof(bind));
        symbol = lookup(compiled_state, "module_graph_code_owner"); memcpy(&get_owner, &symbol, sizeof(get_owner));
        symbol = lookup(compiled_state, "module_graph_run"); memcpy(&graph_run, &symbol, sizeof(graph_run));
        assert(bind && get_owner && graph_run); bind(graph_capture);
        xl__tcc.tcc_get_symbol = lookup; xl__tcc.tcc_delete = delete_state;
        xlApiLock(); module = xlCreateModuleFromState(&context, compiled_state); xlApiUnlock(); assert(module);
        owner = get_owner(); assert(owner->context == module && owner->size == sizeof(XL6_CodeOwnerGraph) &&
            xl6CodeOwnerGraphOps(owner) == &xlModuleOwnershipOps);
        view = (xrtownershipref){module, xl6CodeOwnerGraphOps(owner)};
        graph_inspect(view, 1, 0, 1); /* uninitialized module owns no globals */
        assert(xlActivateModule(&context, module, NULL));
        /* module + scalar Value + array Value/backing/two items + closure + code lease = 8.
         * The closure's unique environment is folded into its owning node;
         * its capture is a second real edge to the same scalar Value. The
         * closure owns its resident lease, which in turn owns the module. */
        graph_inspect(view, 8, 9, 1);
        xlApiLock(); assert(xlModuleBeginCallUnlocked(module)); xlApiUnlock();
        assert(graph_run() == 42 && graph_alias && xrtGetError() == NULL);
        xlApiLock(); xlModuleEndCallUnlocked(module); xlApiUnlock();
        graph_inspect(view, 8, 9, 2); /* native Retain remains visible */
        /* Unknown, truncated and old capabilities may still acquire code, but
         * cannot claim a complete graph. Each acquired reference is returned. */
        for (unsigned abi = 0; abi < 3; ++abi) {
            XL6_CodeOwnerGraph opaque = { *owner, 0, &xlModuleOwnershipOps };
            opaque.base.size = abi == 0 ? sizeof(XL6_CodeOwner) : abi == 1 ? sizeof(opaque) - 1 : sizeof(opaque);
            assert(xl6CodeLeaseAcquire(&opaque.base, &lease) == XL6_CODE_LEASE_ACQUIRED);
            graph_refuse(xl6CodeLeaseOwnership(lease)); xl6CodeLeaseRelease(lease); lease = NULL;
        }
        assert(module->codeLeases == 1 && xrtMemDebugFailAfter(0));
        assert(xl6CodeLeaseAcquire(owner, &lease) == XL6_CODE_LEASE_RESOURCE_FAILURE && !lease &&
            xrtMemDebugFailTriggered()); xrtMemDebugFailClear(); xrtClearError(); assert(module->codeLeases == 1);
        if (path == 0) {
            assert(xl6CodeLeaseAcquire(owner, &lease) == XL6_CODE_LEASE_ACQUIRED);
            assert(xl6CodeLeaseRetain(lease) == lease && xl6CodeLeaseRetain(lease) == lease);
            assert(module->codeLeases == 2);
            view = xl6CodeLeaseOwnership(lease); graph_inspect(view, 9, 10, 3);
            xlDestroyModule(module); assert(!atomic_load(&deleted));
            { XL6_CodeLease* denied = lease; assert(xl6CodeLeaseAcquire(owner, &denied) == XL6_CODE_LEASE_CLOSED && !denied); }
            graph_inspect(view, 9, 10, 2);
            xrtValueRelease(graph_alias); graph_alias = NULL;
            shape = graph_inspect(view, 9, 10, 1); graph_faults(view, &shape);
            xl6CodeLeaseRelease(lease); xl6CodeLeaseRelease(lease);
            assert(!atomic_load(&deleted) && module->codeLeases == 2);
            xl6CodeLeaseRelease(lease); /* resident tail, may unmap module */
        } else {
            GraphFrame* frame = &graph_frames[0]; queue = xl6ContinuationQueueCreate(1); assert(queue);
            frame->value = xrtValueRetain(graph_alias);
            assert(xl6ContinuationReserveOwned(queue, &frame->ticket, graph_frame_run, graph_frame_release, frame, owner) == XL6_CONTINUATION_ADMITTED);
            assert(xl6ContinuationTraceBind(&frame->ticket, graph_frame_trace));
            xl6ContinuationCommit(&frame->ticket);
            view = xl6ContinuationOwnership(&frame->ticket); graph_inspect(view, 10, 12, 3);
            xlDestroyModule(module); assert(!atomic_load(&deleted)); xl6ContinuationQueueClose(queue);
            shape = graph_inspect(view, 10, 12, 2); graph_faults(view, &shape);
            xl6ContinuationReady(&frame->ticket); xl6ContinuationTransportReleased(&frame->ticket);
            assert(xl6ContinuationRunOne(queue) && !atomic_load(&deleted) && graph_runs == 1 && graph_releases == 1);
            view = xl6ContinuationOwnership(&graph_frames[1].ticket); graph_inspect(view, 10, 12, 2);
            xrtValueRelease(graph_alias); graph_alias = NULL;
            graph_inspect(view, 10, 12, 1);
            assert(xl6ContinuationRunOne(queue) && graph_runs == 2 && graph_releases == 2);
            assert(atomic_load(&deleted) && xl6ContinuationQueueDestroy(queue));
            /* The accepted callback retained one last native alias while it
             * ran. This scalar owns no generated code and survives unload. */
            assert(graph_alias); xrtValueRelease(graph_alias); graph_alias = NULL;
        }
        finish(); assert(!graph_alias && xrtGetError() == NULL);
        xrtFree(context.lastError); xrtMemDebugSnapshot(&after);
        assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes &&
            after.AllocCount-before.AllocCount == after.FreeCount-before.FreeCount);
    }
    assert(graph_refusals == 325 && graph_probes == 600 && graph_failures == 200);
    printf("module ownership: 50 real TCC lifetimes, 25 successor handoffs, %u atomic refusals, %u inspection probes, %u actual inspection failures; explicit code capabilities, not automatic object pin\n",
        graph_refusals, graph_probes, graph_failures);
    return 0;
}
