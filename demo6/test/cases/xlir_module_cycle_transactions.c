/* Additional acceptance, never a replacement for the original 50 lifetimes.
 * Real generated globals, real host phases and actual TCC executable pages. */
#define JOINT_TCC_ASYNC
#define XL_ENABLE_EXE_COMPILER 1
#define JOINT_TCC_SOURCE_FILE "test/cases/xlir_module_ownership_artifact.c"
#define main unused_cycle_lifetime_main
#include "xlir_one_shot_tcc_host.c"
#undef main

static unsigned transactions, failures, finish_failures, forbidden_traces;
static const XL6_ModuleRetirementV1* original_contract;
static XL6_ModuleRetirementV1 test_contract;
static bool forbidden_trace(XL6_ModuleOwnershipVisit visit, void* data)
{ (void)visit; (void)data; ++forbidden_traces; abort(); }
static const XL6_ModuleRetirementV1* unknown_contract(void) { return &test_contract; }
static void* legacy_lookup(TCCState* state, const char* name)
{
    if (!strcmp(name, "xl_script_module_retirement_v2")) return NULL;
    return !strcmp(name, "xl_script_module_retirement_v1") ? (void*)unknown_contract : lookup(state, name);
}
static int retry_finish(void)
{
    if (!finish_failures++) { xrtSetErrorKind(XERR_STATE); return 0; }
    return original_contract->finish();
}
static void capture_noop(xvalue* value)
{
    int64_t result = 0;
    assert(!atomic_load(&deleted) && xrtValueGetInt(value, &result) && result == 42);
}
static void create_module(bool legacy)
{
    void* symbol; void (*bind)(void (*)(xvalue*));
    memset(&context, 0, sizeof(context)); atomic_store(&deleted, 0); atomic_store(&cleanups, 0);
    load_artifact();
    symbol = lookup(compiled_state, "module_graph_bind_capture"); memcpy(&bind, &symbol, sizeof(bind));
    assert(bind); bind(capture_noop);
    test_contract = (XL6_ModuleRetirementV1){sizeof(test_contract)-1, 0x584C364D525431ULL, NULL, NULL};
    xl__tcc.tcc_get_symbol = legacy ? legacy_lookup : lookup; xl__tcc.tcc_delete = delete_state;
    xlApiLock(); module = xlCreateModuleFromState(&context, compiled_state); xlApiUnlock();
    assert(module && !!module->retirementContract == !legacy && xlActivateModule(&context, module, NULL));
    assert(module->codeLeases == 1 && module->snapshotRefs == 0);
    xl__tcc.tcc_get_symbol = lookup;
}
static void check_original_graph(void)
{
    xrtownershipscope freeze = {0}; xrtownershipresult shape = {0};
    xrtownershipref root = {module, &xlModuleOwnershipOps};
    xlApiLock(); assert(xrtOwnershipFreezeTryBegin(&freeze));
    assert(xrtOwnershipInspect(&root, 1, NULL, 0, &shape));
    assert(shape.NodeCount == 8 && shape.EdgeCount == 9 && !shape.ReachableAnchorCount);
    assert(xrtOwnershipScopeEnd(&freeze)); xlApiUnlock();
}
static void balance(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;
    finish(); assert(atomic_load(&deleted) == 1); xrtClearError(); xrtFree(context.lastError);
    xrtMemDebugSnapshot(&after);
    assert(before->LiveCount == after.LiveCount && before->LiveBytes == after.LiveBytes &&
        after.AllocCount-before->AllocCount == after.FreeCount-before->FreeCount);
    ++transactions;
}
typedef struct ActiveScope { XL6_CodeLease* lease; HANDLE entered, leave; } ActiveScope;
static DWORD WINAPI active_worker(void* data)
{
    ActiveScope* activity = data; XL6_CodeScope scope = {0};
    xl6CodeScopeEnter(&scope, activity->lease);
    assert(SetEvent(activity->entered));
    assert(WaitForSingleObject(activity->leave, 3000) == WAIT_OBJECT_0);
    assert(!atomic_load(&deleted)); xl6CodeScopeLeave(&scope); return 0;
}
static void transaction(unsigned mode)
{
    xmemdebugsnapshot before; XL6_CodeLease* guard = NULL; xrtownershipscope mutation = {0};
    xrtMemDebugSnapshot(&before); create_module(mode == 2);
    if (mode == 0) {
        assert(xrtOwnershipMutationBegin(&mutation)); xlDestroyModule(module);
        assert(!atomic_load(&deleted) && !module->ownershipPlan && module->codeLeases == 1);
        assert(xrtOwnershipScopeEnd(&mutation)); collect();
    } else if (mode == 1) {
        ActiveScope activity = {0}; HANDLE worker; XL6_ModuleOwnershipTrace trace = module->ownershipTrace;
        assert(xl6CodeLeaseAcquire(&module->codeOwnerGraph.base, &guard) == XL6_CODE_LEASE_ACQUIRED);
        activity.lease = guard; activity.entered = CreateEventA(NULL, TRUE, FALSE, NULL);
        activity.leave = CreateEventA(NULL, TRUE, FALSE, NULL); assert(activity.entered && activity.leave);
        worker = CreateThread(NULL, 0, active_worker, &activity, 0, NULL);
        assert(worker && WaitForSingleObject(activity.entered, 3000) == WAIT_OBJECT_0);
        /* A read-only graph would find an external scope AFTER tracing. The
         * production admission must refuse BEFORE any generated global read. */
        module->ownershipTrace = forbidden_trace; xlDestroyModule(module); collect();
        assert(!atomic_load(&deleted) && !module->ownershipPlan && !forbidden_traces);
        module->ownershipTrace = trace;
        assert(SetEvent(activity.leave) && WaitForSingleObject(worker, 3000) == WAIT_OBJECT_0);
        assert(CloseHandle(worker) && CloseHandle(activity.entered) && CloseHandle(activity.leave));
        xl6CodeLeaseRelease(guard);
    } else if (mode == 2) {
        XL6_CodeScope scope = {0}; XL_ScriptUnitCheckedProc unit; void* symbol;
        assert(xl6CodeLeaseAcquire(&module->codeOwnerGraph.base, &guard) == XL6_CODE_LEASE_ACQUIRED);
        xlDestroyModule(module); collect();
        assert(!atomic_load(&deleted) && !module->ownershipPlan && !module->retirementContract);
        /* Unknown contract stays refused. Explicit legacy teardown has its
         * own real entry lease; never install a guessed adapter to clean up. */
        symbol = lookup(compiled_state, "xl_script_module_unit_checked"); memcpy(&unit, &symbol, sizeof(unit));
        xl6CodeScopeEnter(&scope, guard); assert(unit && unit()); xl6CodeScopeLeave(&scope);
        xl6CodeLeaseRelease(guard);
    } else if (mode == 3) {
        original_contract = module->retirementContract; test_contract = *original_contract;
        test_contract.finish = retry_finish; finish_failures = 0; module->retirementContract = &test_contract;
        xlDestroyModule(module);
        assert(!atomic_load(&deleted) && module->ownershipPlan && module->ownershipCleared &&
            module->snapshotRefs == 1 && module->ownershipPhase == 3 && module->retirementError);
        collect(); assert(finish_failures == 2);
    } else {
        XL6_OwnershipPlanResult result;
        assert(xl6CodeLeaseAcquire(&module->codeOwnerGraph.base, &guard) == XL6_CODE_LEASE_ACQUIRED);
        xlApiLock(); assert(xlModulePrepareRetirementUnlocked(module)); xl__ownershipDriverRunning = 1;
        result = xl6OwnershipPlanTryCreate((xrtownershipref){module, &xlModuleOwnershipOps},
            xlModuleResolveOwnership, NULL, &module->ownershipPlan);
        xl__ownershipDriverRunning = 0; module->ownershipPhase = 5; xlApiUnlock();
        assert(result == XL6_OWNERSHIP_PLAN_ROOTED && module->snapshotRefs == 1);
        assert(xrtOwnershipMutationBegin(&mutation)); xlDestroyModule(module);
        assert(module->ownershipPlan && module->ownershipPhase == 5 && !atomic_load(&deleted));
        assert(xrtOwnershipScopeEnd(&mutation)); collect();
        assert(!module->ownershipPlan && !module->snapshotRefs && !atomic_load(&deleted));
        xl6CodeLeaseRelease(guard);
    }
    balance(&before);
}
static void allocation_fault(unsigned point, bool commit)
{
    xmemdebugsnapshot before; xrtownershipscope mutation = {0}; bool failed;
    xrtMemDebugSnapshot(&before); create_module(false);
    if (commit) {
        XL6_OwnershipPlanResult result;
        assert(xrtOwnershipMutationBegin(&mutation)); xlDestroyModule(module);
        assert(xrtOwnershipScopeEnd(&mutation));
        xlApiLock(); assert(xlModulePrepareRetirementUnlocked(module)); xl__ownershipDriverRunning = 1;
        result = xl6OwnershipPlanTryCreate((xrtownershipref){module, &xlModuleOwnershipOps},
            xlModuleResolveOwnership, NULL, &module->ownershipPlan);
        xl__ownershipDriverRunning = 0; module->ownershipPhase = 2; xlApiUnlock();
        assert(result == XL6_OWNERSHIP_PLAN_READY);
    }
    assert(xrtMemDebugFailAfter(point));
    if (commit) collect(); else xlDestroyModule(module);
    failed = xrtMemDebugFailTriggered(); xrtMemDebugFailClear(); xrtClearError();
    if (failed) {
        ++failures;
        assert(!atomic_load(&deleted) && !module->ownershipPlan && !module->ownershipCleared &&
            !module->ownershipClaim && !module->snapshotRefs && module->codeLeases == 1);
        check_original_graph();
    }
    if (!atomic_load(&deleted)) collect();
    balance(&before);
}
static void preparation_inspection_fault(bool preparing)
{
    xmemdebugsnapshot before;xrtownershipscope mutation={0};
    xrtMemDebugSnapshot(&before);create_module(false);
    /* Defer automatic retirement, then keep the actual V2 plan. The next
     * allocation belongs to snapshot reconstruction, not to a finalizer. */
    assert(xrtOwnershipMutationBegin(&mutation));xlDestroyModule(module);
    assert(xrtOwnershipScopeEnd(&mutation));
    xlApiLock();assert(xlModulePrepareRetirementUnlocked(module));xl__ownershipDriverRunning=1;
    XL6_OwnershipPlanResult result=xl6OwnershipPlanTryCreateV2(
        (xrtownershipref){module,&xlModuleOwnershipOps},xlModuleResolveOwnershipV2,NULL,&module->ownershipPlan);
    xl__ownershipDriverRunning=0;assert(result==XL6_OWNERSHIP_PLAN_READY);
    module->ownershipPhase=preparing?6:1;
    assert(xrtMemDebugFailAfter(0));xlModuleAdvanceOwnershipUnlocked(module);
    bool failed=xrtMemDebugFailTriggered();xrtMemDebugFailClear();xlApiUnlock();xrtClearError();
    assert(failed&&!atomic_load(&deleted)&&!module->ownershipPlan&&!module->ownershipCleared);
    assert(!module->retirementFailed&&!module->retirementError);
    check_original_graph();collect();balance(&before);
}
int main(void)
{
    assert(xrtMemDebugEnable(true)); expected_cleanups = 0; provider_init();
    for (unsigned round = 0; round < 25; ++round)
        for (unsigned mode = 0; mode < 5; ++mode) transaction(mode);
    for (unsigned point = 0; point < 48; ++point) {
        allocation_fault(point, false); allocation_fault(point, true);
    }
    assert(transactions == 221 && failures > 0 && !forbidden_traces);
    printf("Module cycle transactions: 221 actual TCC lifetimes, 25 native active-scope pre-trace refusals, 25 legacy refusals, 25 finish retries, 25 busy aborts, 96 creation/commit allocation probes, %u real failures; MEM_FREE and zero live delta\n", failures);
    for(unsigned round=0;round<25;++round){preparation_inspection_fault(false);preparation_inspection_fault(true);}
    assert(transactions==271);
    puts("V2 preparation inspection: 50 real TCC retirement snapshot allocation failures, no semantic failure latch, exact graph preservation, successful retry, MEM_FREE and zero live delta");
    return 0;
}
