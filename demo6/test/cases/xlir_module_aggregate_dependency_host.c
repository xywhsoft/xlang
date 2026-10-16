#include "src/runtime/xl6_future_wait.h"
static void bind_aggregate_callbacks(void);
static void inspect_aggregate_unrooted(struct xrtownershipref, bool);
static void begin_aggregate_probe(void);
#define MODULE_MIXED_CUSTOM_MAIN
#define MODULE_MIXED_CREATED bind_aggregate_callbacks
#define MODULE_EXECUTOR_UNROOTED inspect_aggregate_unrooted
#define MODULE_EXECUTOR_BEFORE_UNROOTED begin_aggregate_probe
#define JOINT_TCC_SOURCE_FILE "test/cases/xlir_module_aggregate_dependency_artifact.c"
#include "xlir_module_mixed_dependency_host.c"

static unsigned aggregate_mode, proof_plans, proof_members, proof_feeders, proof_budgets, proof_resources, proof_deferrals;
static unsigned proof_unprepared_queries;
static unsigned proof_native_views, late_completions;
static unsigned proof_snapshot_deferrals;
static bool probe_busy;
static XL6_OwnershipAdmissionV1 probe_resolve(xrtownershipref ref, void* context, const xrtownershipadapterv1** adapter,
    const xrtownershippreparationv1** preparation,const xrtownershipfinishv1** completion)
{
    XL6_OwnershipAdmissionV1 admitted=xlModuleProbeOwnershipV1(ref,context,adapter,preparation,completion);
    if(admitted==XL6_OWNERSHIP_ADMISSION_BUSY)probe_busy=true;
    return admitted;
}
static bool resolve_probe_capability(xrtownershipref ref,const xrtownershipadapterv1** adapter,const xrtownershippreparationv1** preparation)
{ const xrtownershipfinishv1* completion=NULL;return xlModuleResolveOwnershipV3(ref,module,adapter,preparation,&completion); }
static xfuture* aggregate_ready_future;
static int64_t aggregate_mode_get(void) { return aggregate_mode; }
static xfuture* aggregate_ready_get(void) { return xrtFutureRef(aggregate_ready_future); }
static void bind_aggregate_callbacks(void)
{
    typedef void (*Bind)(int64_t (*)(void), xfuture* (*)(void)); Bind bind;
    void* symbol = lookup(compiled_state, "aggregate_dependency_bind"); memcpy(&bind, &symbol, sizeof(bind));
    assert(bind); bind(aggregate_mode_get, aggregate_ready_get);
}
static void proof_balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after; xrtMemDebugSnapshot(&after);
    assert(after.LiveCount == before->LiveCount && after.LiveBytes == before->LiveBytes &&
        after.AllocCount - before->AllocCount == after.FreeCount - before->FreeCount &&
        after.InvalidFreeCount == before->InvalidFreeCount && after.DoubleFreeCount == before->DoubleFreeCount &&
        after.UseAfterFreeCount == before->UseAfterFreeCount);
}
typedef struct WaitProducerSearch { const void* token; xrtownershipref output, producer; unsigned found; } WaitProducerSearch;
typedef struct WaitQueueSearch { xrtownershipref queue; const xrtownershippreparationv1* preparation; unsigned edges; } WaitQueueSearch;
static bool find_wait_queue(xrtownershipref ref, void* data)
{
    WaitQueueSearch* search = data; const XL6_ContinuationStorageV2* storage[] = {xl6ContinuationProducedStorageV2Get()};
    const xrtownershippreparationv1 *preparation = NULL, *admitted = NULL; const xrtownershipadapterv1* adapter = NULL;
    const xrtownershipadapterv1* queue = xl6ContinuationQueueOwnershipAdapterV1(ref, storage, 1, &preparation);
    if (queue) {
        assert(resolve_probe_capability(ref, &adapter, &admitted) && adapter == queue && admitted == preparation);
        if (search->edges) assert(ref.Data == search->queue.Data && ref.Ops == search->queue.Ops);
        search->queue = ref; search->preparation = preparation; ++search->edges;
    }
    return true;
}
static bool find_wait_producer(xrtownershipref ref, void* data)
{
    WaitProducerSearch* search = data; XL6_FutureWaitV1 wait = {0};
    if (xl6ContinuationFutureWaitV1(ref, search->token, &wait)) {
        xrtownershipref output = xrtPromiseOwnership(wait.output);
        if (output.Data == search->output.Data && output.Ops == search->output.Ops) { search->producer = ref; ++search->found; }
    }
    return true;
}
typedef struct AggregateWaitSearch { const void* token; xrtownershipref output; unsigned found; } AggregateWaitSearch;
static bool check_aggregate_wait(xrtownershipref ref, void* data)
{
    AggregateWaitSearch* search = data; xfuturecombinewaitv1 wait = {0};
    if (xrtFutureCombineWaitV1(ref, search->token, &wait)) {
        xrtownershipref output = xrtPromiseOwnership(wait.Output); unsigned wrong = 0;
        assert(wait.size == sizeof(wait) && output.Data == search->output.Data && output.Ops == search->output.Ops);
        assert(!xrtFutureCombineWaitV1(ref, &wrong, &(xfuturecombinewaitv1){0}));
        if (aggregate_mode == 0 || aggregate_mode == 3) {
            assert(wait.Rule == XFUTURE_WAIT_ALL_TERMINAL && !wait.CancelRemaining && wait.Count == 3);
            assert(wait.Sources[0] == wait.Sources[2] && wait.Sources[1] == aggregate_ready_future);
            assert(xrtFutureState(wait.Sources[0]) == XFUTURE_PENDING && xrtFutureState(wait.Sources[1]) == XFUTURE_RESOLVED);
        } else {
            assert(wait.Rule == XFUTURE_WAIT_ANY_TERMINAL && wait.Count == 2 && wait.Sources[0] == wait.Sources[1]);
            assert(wait.CancelRemaining == (aggregate_mode == 1 || aggregate_mode == 4));
            assert(xrtFutureState(wait.Sources[0]) == XFUTURE_PENDING);
        }
        ++search->found;
    }
    return true;
}
static void assert_untouched(void)
{
    assert(!module->retirementFailed && !module->ownershipCleared && !atomic_load(&deleted) && !atomic_load(&global_drops));
    for (unsigned i = 0; i < cycle_width; ++i) {
        xcancel* cancel = xrtFutureCancelToken(cycle_outputs[i]); assert(cancel && !xrtCancelRequested(cancel)); xrtCancelDestroy(cancel);
        assert(!xrtFutureDone(cycle_outputs[i]) && !atomic_load(&catches[i]) && !atomic_load(&finals[i]));
        for (unsigned j = 0; j < 2; ++j) assert(!atomic_load(&callback_finally[i][j]));
    }
}
static void begin_aggregate_probe(void)
{
    /* Keep the last real Future root until the normal driver's previous
     * snapshot has aborted. Exclude that driver BEFORE dropping the root,
     * so a worker release cannot begin semantic retirement before the
     * independent proof obtains its own claims. No count is discounted. */
    xdeadline deadline=xrtDeadlineAfter(5000000);
    for(;;){
        xlApiLock();assert(!xl__ownershipDriverRunning && !xrtGetError());
        xlCollectRetiringModulesUnlocked();
        if(!module->ownershipPlan){xl__ownershipDriverRunning=1;xlApiUnlock();assert_untouched();return;}
        xlApiUnlock();assert_untouched();assert(!xrtDeadlineExpired(deadline));xrtThreadYield();
    }
}
static void inspect_aggregate_unrooted(xrtownershipref executor, bool feeder)
{
    /* Normal host admission has already run, and the final external Future
     * root has just been released. Freeze/claims come from the actual whole
     * module ownership plan, not hand-written flags or fabricated counts.
     * Serialize the host's retirement driver while this independent probe
     * owns its plan. It is aborted/restored before normal collection resumes. */
    XL6_OwnershipPlan* plan = NULL; xrtownershipscope freeze = {0}; xmemdebugsnapshot before;
    xrtownershipref producer = {0}; const xrtownershipadapterv1* adapter = NULL;
    const xrtownershippreparationv1* preparation = NULL; xdeadline deadline = xrtDeadlineAfter(5000000);
    xlApiLock();assert(!module->ownershipPlan && xl__ownershipDriverRunning && !xrtGetError());xlApiUnlock();
    xrtMemDebugSnapshot(&before);
    for (;;) {
        quiet_executor(executor, &freeze); assert(xrtOwnershipScopeEnd(&freeze));
        xlApiLock();
        probe_busy = false;
        XL6_OwnershipPlanResult result = xl6OwnershipPlanTryCreateV6((xrtownershipref){module, &xlModuleOwnershipOps},
            probe_resolve, NULL, false, module, &plan);
        xlApiUnlock();
        if (result == XL6_OWNERSHIP_PLAN_PREPARATIONS) break;
        if (result == XL6_OWNERSHIP_PLAN_BUSY && probe_busy) ++proof_snapshot_deferrals;
        if (result != XL6_OWNERSHIP_PLAN_BUSY) fprintf(stderr, "Aggregate plan refusal: mode=%u width=%u feeder=%u transaction=%u status=%d error=%s\n",
            aggregate_mode, cycle_width, feeder, transactions, (int)result, xrtGetError() ? xrtErrorMessage(xrtGetError()) : "none");
        assert(result == XL6_OWNERSHIP_PLAN_BUSY && !plan && !xrtGetError() && !xrtDeadlineExpired(deadline)); xrtThreadYield();
    }
    quiet_executor(executor, &freeze);
    WaitQueueSearch queue = {0};
    assert(executor.Ops->Trace(executor.Data, find_wait_queue, &queue) && queue.edges && queue.preparation);
    assert(!queue.preparation->Ready(queue.queue.Data));
    for (unsigned i = 0; i < cycle_width; ++i) {
        WaitProducerSearch search = {plan, xrtFutureOwnership(cycle_outputs[i]), {0}, 0};
        assert(search.output.Ops->Trace(search.output.Data, find_wait_producer, &search) && !search.found);
        ++proof_unprepared_queries;
    }
    assert(xrtOwnershipScopeEnd(&freeze));
    /* The descriptor correctly refuses an unprepared queue. Perform the
     * actual independently admitted queue Prepare, as the normal plan driver
     * does, rather than forging shutdownPrepared or bypassing this contract.
     * Accepted activations and their callback/finally duties remain pending. */
    assert(queue.preparation->Prepare(queue.queue.Data, plan) == XRT_OWNERSHIP_PREPARE_READY);
    for (;;) {
        quiet_executor(executor, &freeze); assert(xrtOwnershipScopeEnd(&freeze));
        probe_busy = false;
        XL6_OwnershipPlanResult result = xl6OwnershipPlanTryRefresh(plan);
        if (result == XL6_OWNERSHIP_PLAN_PREPARATIONS) break;
        if (result == XL6_OWNERSHIP_PLAN_BUSY && probe_busy) ++proof_snapshot_deferrals;
        assert(result == XL6_OWNERSHIP_PLAN_BUSY && !xrtDeadlineExpired(deadline)); xrtThreadYield();
    }
    quiet_executor(executor, &freeze);
    for (unsigned i = 0; i < cycle_width; ++i) {
        WaitProducerSearch search = {plan, xrtFutureOwnership(cycle_outputs[i]), {0}, 0};
        assert(search.output.Ops->Trace(search.output.Data, find_wait_producer, &search) && search.found == 1);
        assert(resolve_probe_capability(search.producer, &adapter, &preparation) && adapter && preparation);
        XL6_FutureWaitV1 single = {0}; assert(xl6ContinuationFutureWaitV1(search.producer, plan, &single));
        AggregateWaitSearch aggregate = {plan, xrtFutureOwnership(single.source), 0};
        assert(aggregate.output.Ops->Trace(aggregate.output.Data, check_aggregate_wait, &aggregate) && aggregate.found == 1);
        ++proof_native_views;
        unsigned wrong_token = 0;
        assert(xl6FutureWaitCycleMemberV2(search.producer, &wrong_token) == XL6_FUTURE_WAIT_UNPROVEN);
        XL6_FutureWaitProof result = xl6FutureWaitCycleMemberV2(search.producer, plan);
        if (feeder && !i) { assert(result == XL6_FUTURE_WAIT_UNPROVEN); ++proof_feeders; }
        else { assert(result == XL6_FUTURE_WAIT_CYCLE_MEMBER); ++proof_members; }
        if (!i) producer = search.producer;
    }
    assert_untouched();
    if (cycle_width == 3 && !feeder) {
        /* Three real activation/callback/aggregate chains exceed local node
         * storage. Each budget probes exactly the public borrowed query.
         * Diagnostics and all original semantic duties must remain intact. */
        for (unsigned budget = 0; budget < 9; ++budget) {
            xrtSetErrorKind(XERR_PROTOCOL); const xerror* primary = xrtGetError(); xmemdebugsnapshot checkpoint;
            assert(primary); xrtMemDebugSnapshot(&checkpoint); assert(xrtMemDebugFailAfter(budget));
            XL6_FutureWaitProof result = xl6FutureWaitCycleMemberV2(producer, plan);
            bool failed = xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
            assert(xrtGetError() == primary);
            if (failed) { assert(result == XL6_FUTURE_WAIT_RESOURCE_FAILURE); ++proof_resources; }
            else assert(result == XL6_FUTURE_WAIT_CYCLE_MEMBER);
            proof_balanced(&checkpoint); xrtClearError(); assert_untouched(); ++proof_budgets;
        }
    }
    assert(xrtOwnershipScopeEnd(&freeze));
    if (cycle_width == 3 && !feeder) {
        /* Test the real continuation producer's Prepare caller as well as the
         * pure query. Failure to allocate proof scratch must return BUSY,
         * never FAILED, and must not run cancellation/finally callbacks. */
        quiet_executor(executor, &freeze);
        assert(resolve_probe_capability(producer, &adapter, &preparation) && preparation);
        assert(xrtOwnershipScopeEnd(&freeze));
        xrtSetErrorKind(XERR_PROTOCOL); const xerror* primary = xrtGetError(); assert(primary && xrtMemDebugFailAfter(0));
        xrtownershipprepareresult result = preparation->Prepare(producer.Data, plan);
        bool failed = xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
        assert(failed && result == XRT_OWNERSHIP_PREPARE_BUSY && xrtGetError() == primary);
        xrtClearError(); assert_untouched(); ++proof_deferrals;
    }
    for (;;) {
        XL6_OwnershipPlanResult result = xl6OwnershipPlanTryDestroy(plan);
        if (result == XL6_OWNERSHIP_PLAN_READY) break;
        assert(result == XL6_OWNERSHIP_PLAN_BUSY && !xrtDeadlineExpired(deadline)); xrtThreadYield();
    }
    xlApiLock(); assert(xl__ownershipDriverRunning); xl__ownershipDriverRunning = 0; xlApiUnlock();
    assert_untouched(); proof_balanced(&before); ++proof_plans;
}
static void aggregate_transaction(unsigned width, bool cleanup, bool root, bool feeder, bool acyclic, int budget)
{
    xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    xpromise* ready = xrtPromiseCreate(&aggregate_ready_future, NULL);
    assert(ready && xrtPromiseResolve(ready, &number));
    mixed_transaction(width, cleanup, root, feeder, acyclic, budget);
    xrtPromiseDestroy(ready); xrtFutureDestroy(aggregate_ready_future); aggregate_ready_future = NULL;
    proof_balanced(&before);
}
static void late_aggregate_transaction(unsigned outcome)
{
    /* Keep an actual external source pending across module Close. Its normal
     * completion, failure or cancellation then runs accepted generated code
     * (including successful ALL boxing) after root code admission is closed. */
    xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    cycle_width = 2; expected_cleanups = 2; loop_start = 0; terminal_future = NULL; create_cycle_module(0);
    xfuture *boot = NULL, *cleanup = NULL;
    xpromise *bootPromise = xrtPromiseCreate(&boot, NULL), *cleanupPromise = xrtPromiseCreate(&cleanup, NULL);
    xpromise *terminal = xrtPromiseCreate(&terminal_future, NULL), *ready = xrtPromiseCreate(&aggregate_ready_future, NULL);
    assert(bootPromise && cleanupPromise && terminal && ready);
    assert(xrtPromiseResolve(cleanupPromise, &number) && xrtPromiseResolve(ready, &number));
    xlApiLock(); assert(xlModuleBeginCallUnlocked(module)); xlApiUnlock();
    for (unsigned i = 0; i < 2; ++i) { cycle_outputs[i] = start_cycle(i, boot, cleanup, false); assert(cycle_outputs[i] && !xrtGetError()); }
    xlApiLock(); xlModuleEndCallUnlocked(module); xlApiUnlock();
    assert(xrtPromiseResolve(bootPromise, &number)); xdeadline deadline = xrtDeadlineAfter(5000000);
    while (atomic_load(&peer_count) != 2) { assert(!xrtDeadlineExpired(deadline)); xrtThreadYield(); }
    xrtownershipscope freeze = {0}; quiet_executor(executor_view(), &freeze); assert(xrtOwnershipScopeEnd(&freeze));
    xlApiLock(); assert(prepare_mixed_module() && module->retirementPrepared); xlApiUnlock();
    xlDestroyModule(module); assert(module->destroyRequested && module->retirementPrepared);
    for (unsigned pass = 0; pass < 4; ++pass) collect();
    assert_untouched();
    if (!outcome) assert(xrtPromiseResolve(terminal, &number));
    else if (outcome == 1) {
        xerror* error = xrtErrorCreate(XERR_PROTOCOL, "aggregate.late", 71, "late aggregate source failure");
        assert(error && xrtPromiseReject(terminal, error)); xrtErrorFree(error);
    } else assert(xrtPromiseCancel(terminal));
    for (unsigned i = 0; i < 2; ++i) {
        assert(xrtFutureWaitFor(cycle_outputs[i], 5000000) == XWAIT_OK);
        assert(xrtFutureState(cycle_outputs[i]) == (!outcome ? XFUTURE_RESOLVED : outcome == 1 ? XFUTURE_FAILED : XFUTURE_CANCELLED));
        if (!outcome) assert(*(int64_t*)xrtFutureValue(cycle_outputs[i]) == number);
        if (outcome == 1) assert(xrtFutureError(cycle_outputs[i]) && strstr(xrtErrorMessage(xrtFutureError(cycle_outputs[i])), "late aggregate source failure"));
        assert(atomic_load(&catches[i]) == (outcome == 2 ? 1u : 0u) && atomic_load(&finals[i]) == 1 && atomic_load(&afters[i]) == (outcome ? 0u : 1u));
        for (unsigned j = 0; j < 2; ++j) { assert(atomic_load(&callback_finally[i][j]) == 1); ++callback_count; }
        xrtFutureDestroy(cycle_outputs[i]);
    }
    xrtPromiseDestroy(bootPromise); xrtFutureDestroy(boot); xrtPromiseDestroy(cleanupPromise); xrtFutureDestroy(cleanup);
    xrtPromiseDestroy(terminal); xrtFutureDestroy(terminal_future); terminal_future = NULL;
    xrtPromiseDestroy(ready); xrtFutureDestroy(aggregate_ready_future); aggregate_ready_future = NULL;
    balance_cycle(&before); ++late_completions;
}
int main(void)
{
    assert(xrtMemDebugEnable(true)); expected_cleanups = 0;
#ifndef MODULE_EXECUTOR_DLL
    provider_init();
#endif
    for (aggregate_mode = 0; aggregate_mode < 6; ++aggregate_mode) {
        for (unsigned width = 1; width <= 3; ++width) for (unsigned cleanup = 0; cleanup < 2; ++cleanup)
            for (unsigned root = 0; root < 2; ++root) aggregate_transaction(width, cleanup != 0, root != 0, false, false, -1);
        for (unsigned cleanup = 0; cleanup < 2; ++cleanup) for (unsigned root = 0; root < 2; ++root)
            aggregate_transaction(3, cleanup != 0, root != 0, true, false, -1);
        for (unsigned width = 1; width <= 2; ++width) for (unsigned root = 0; root < 2; ++root)
            aggregate_transaction(width, false, root != 0, false, true, -1);
    }
    aggregate_mode = 0;
    for (int budget = 0; budget < 32; ++budget) aggregate_transaction(2, false, false, false, false, budget);
    for (aggregate_mode = 0; aggregate_mode < 6; ++aggregate_mode) for (unsigned outcome = 0; outcome < 3; ++outcome) late_aggregate_transaction(outcome);
    assert(transactions == 170 && rooted_checks == 128 && cleanup_roots == 48 && callback_count == 704 && oom_failures > 0 && preflight_total == 170 && late_completions == 18);
    assert(proof_plans == 80 && proof_members == 160 && proof_feeders == 12 && proof_budgets == 108 && proof_resources == 12 && proof_deferrals == 12 && proof_unprepared_queries == 172 && proof_native_views == 172);
    printf("Aggregate dependency modules: 170 real unloads, six literal/array ALL/RACE/ANY modes, 72 rings, 24 feeders, 24 acyclic controls, 128 external-root checks, 48 cleanup-await roots, 704 callback finally executions, 32 snapshot budgets, %u actual OOM; original async catch/finally, complete ALL results, ANY index, MEM_FREE and exact memory balance\n", oom_failures);
    printf("Aggregate wait proof: 80 actual independently admitted module plans, 160 members, 12 feeders refused, 108 diagnostic-preserving proof budgets, 12 resource failures, 12 real producer BUSY deferrals; zero premature cancellation or semantic cleanup\n");
    printf("Aggregate snapshot admission: %u exact native-transition quiescence deferrals; every successful plan independently readmits the whole graph\n", proof_snapshot_deferrals);
    printf("Aggregate descriptor boundary: 172 unprepared-queue refusals and 172 wrong-claim refusals; actual queue preparation required\n");
    printf("Aggregate native input facts: 172 exact complete ordered views, duplicate and completed slots, ALL/ANY rules and Race-only loser cancellation; 18 success/failure/cancellation completions after module Close\n");
    return 0;
}
