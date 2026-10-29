#include "release/include/xl_clock.h"
#include "lib/tcc/libtcc.h"
#define main unused_executor_lifetime_main
#define XL_ENABLE_EXE_COMPILER 1
#ifdef MODULE_EXECUTOR_DLL
static void load_artifact(void);
static void* lookup(TCCState*, const char*);
static void delete_state(TCCState*);
#define JOINT_CUSTOM_PROVIDER
#include "xlir_async_module_lifetime_host.c"
static void* executor_entry;
static void load_artifact(void)
{
    MEMORY_BASIC_INFORMATION memory;
    artifact = LoadLibraryA(JOINT_DLL_PATH); assert(artifact); compiled_state = (TCCState*)artifact;
    executor_entry = (void*)GetProcAddress(artifact, "xl_script_main_checked"); assert(executor_entry);
    assert(VirtualQuery(executor_entry, &memory, sizeof(memory)) == sizeof(memory) && memory.State == MEM_COMMIT);
}
static void* lookup(TCCState* state, const char* name)
{ assert(state == compiled_state); return (void*)GetProcAddress(artifact, name); }
static void delete_state(TCCState* state)
{
    MEMORY_BASIC_INFORMATION memory;
    assert(state == compiled_state && !module->codeLeases && !module->activeCalls);
    assert(atomic_load(&cleanups) == expected_cleanups);
    assert(FreeLibrary(artifact) && GetModuleHandleA(JOINT_DLL_NAME) == NULL);
    assert(VirtualQuery(executor_entry, &memory, sizeof(memory)) == sizeof(memory) && memory.State == MEM_FREE);
    compiled_state = NULL; artifact = NULL; executor_entry = NULL; atomic_fetch_add(&deleted, 1);
}
#else
#define JOINT_TCC_ASYNC
#ifndef JOINT_TCC_SOURCE_FILE
#define JOINT_TCC_SOURCE_FILE "test/cases/xlir_module_executor_artifact.c"
#endif
#include "xlir_one_shot_tcc_host.c"
#endif
#undef main

typedef struct ModuleExecutorHooks {
    xfuture* (*peer)(int64_t); void (*caught)(int64_t); void (*finally)(int64_t);
    void (*after)(int64_t); void (*dropped)(void);
} ModuleExecutorHooks;
typedef xfuture* (*ModuleStart)(int64_t, xfuture*, xfuture*, bool);
typedef xrtownershipref (*ModuleView)(void);
static ModuleStart start_cycle;
static ModuleView executor_view;
static xfuture *cycle_outputs[3], *terminal_future;
static unsigned cycle_width, loop_start, protocol_mode, protocol_calls;
static unsigned transactions, rooted_checks, cleanup_roots, oom_failures, protocol_refusals, readiness_retries, strict_refusals;
static atomic_uint peer_count, catches[3], finals[3], afters[3], global_drops;
static XL6_ModuleRetirementV4 altered_contract;
static XL6_ModuleRetirementV3 legacy_contract;
static XL6_ModuleRetirementV1 different_graph;
static const XL6_ModuleRetirementV4* real_contract;
static int64_t number = 19;

static xfuture* cycle_peer(int64_t id)
{
    assert(id >= 0 && (unsigned)id < cycle_width && !atomic_load(&deleted));
    xfuture* result = xrtFutureRef((unsigned)id == cycle_width - 1
        ? (terminal_future ? terminal_future : cycle_outputs[loop_start]) : cycle_outputs[id + 1]);
    assert(result); atomic_fetch_add(&peer_count, 1); return result;
}
static void cycle_caught(int64_t id)
{ assert(id >= 0 && (unsigned)id < cycle_width && !atomic_load(&deleted) && !atomic_load(&global_drops)); assert(!atomic_fetch_add(&catches[id], 1)); }
static void cycle_finally(int64_t id)
{
    assert(id >= 0 && (unsigned)id < cycle_width && !atomic_load(&deleted) && !atomic_load(&global_drops));
    assert(!atomic_fetch_add(&finals[id], 1)); atomic_fetch_add(&cleanups, 1);
}
static void cycle_after(int64_t id)
{ assert(id >= 0 && (unsigned)id < cycle_width && !atomic_load(&deleted)); assert(!atomic_fetch_add(&afters[id], 1)); }
static void module_dropped(void)
{ assert(!atomic_load(&deleted) && atomic_load(&cleanups) == expected_cleanups); assert(!atomic_fetch_add(&global_drops, 1)); }
static XL6_ModulePrepareResult forbidden_preflight(void) { abort(); return XL6_MODULE_PREPARE_FAILED; }
static const XL6_ModuleRetirementV4* altered_get(void) { return protocol_mode == 15 ? NULL : &altered_contract; }
static const XL6_ModuleRetirementV3* legacy_get(void) { return protocol_mode == 5 ? NULL : &legacy_contract; }
static const XL6_ModuleRetirementV2* conflicting_get(void)
{
    static XL6_ModuleRetirementV2 other;
    other = (XL6_ModuleRetirementV2){sizeof(other), 0x584C364D525432ULL, real_contract->graph, forbidden_preflight};
    return &other;
}
static DWORD WINAPI preflight_boundary(void* unused)
{
    xrtownershipscope freeze = {0}; XL6_DeadlineV1 deadline = xl6DeadlineAfterMillisecondsV1(5000); (void)unused;
    xlApiLock(); assert(module->preparationRefs == 1 && module->retirementRunning && xl__ownershipDriverRunning); xlApiUnlock();
    while (!xrtOwnershipFreezeTryBegin(&freeze)) { assert(!xl6DeadlineExpiredV1(deadline)); xrtThreadYield(); }
    assert(xrtOwnershipScopeEnd(&freeze)); return 0;
}
static XL6_ModulePrepareResult checked_preflight(void)
{
    assert(!atomic_load(&deleted) && !atomic_load(&global_drops) && module->preparationRefs == 1);
    HANDLE worker = CreateThread(NULL, 0, preflight_boundary, NULL, 0, NULL); assert(worker);
    assert(WaitForSingleObject(worker, 6000) == WAIT_OBJECT_0 && CloseHandle(worker));
    collect(); assert(!atomic_load(&deleted)); ++protocol_calls;
    if (protocol_calls == 1) {
        if (protocol_mode == 8) return XL6_MODULE_PREPARE_BUSY;
        if (protocol_mode == 9) return XL6_MODULE_PREPARE_FAILED;
        if (protocol_mode == 10) { xrtSetErrorKind(XERR_STATE); return XL6_MODULE_PREPARE_FAILED; }
    }
    return real_contract->preflight();
}
static void* protocol_lookup(TCCState* state, const char* name)
{
    if (!strcmp(name, "xl_script_module_retirement_v4")) return protocol_mode <= 7 ? NULL : (void*)altered_get;
    if (!strcmp(name, "xl_script_module_retirement_v3") && protocol_mode <= 6) return (void*)legacy_get;
    if (!strcmp(name, "xl_script_module_retirement_v2") && protocol_mode == 6) return (void*)conflicting_get;
    return lookup(state, name);
}
static void create_cycle_module(unsigned version)
{
    typedef void (*Bind)(const ModuleExecutorHooks*); Bind bind; XL6_ModuleRetirementGetV4 get;
    ModuleExecutorHooks hooks = {cycle_peer, cycle_caught, cycle_finally, cycle_after, module_dropped};
    memset(&context, 0, sizeof(context)); protocol_mode = version; protocol_calls = 0;
    atomic_store(&deleted, 0); atomic_store(&cleanups, 0); atomic_store(&global_drops, 0); atomic_store(&peer_count, 0);
    for (unsigned i = 0; i < 3; ++i) { cycle_outputs[i] = NULL; atomic_store(&catches[i], 0); atomic_store(&finals[i], 0); atomic_store(&afters[i], 0); }
    load_artifact();
    void* symbol = lookup(compiled_state, "module_executor_bind"); memcpy(&bind, &symbol, sizeof(bind));
    symbol = lookup(compiled_state, "module_executor_start"); memcpy(&start_cycle, &symbol, sizeof(start_cycle));
    symbol = lookup(compiled_state, "module_executor_view"); memcpy(&executor_view, &symbol, sizeof(executor_view));
    symbol = lookup(compiled_state, "xl_script_module_retirement_v4"); memcpy(&get, &symbol, sizeof(get));
    assert(bind && start_cycle && executor_view && get && !lookup(compiled_state, "xl_script_module_retirement_v2"));
    real_contract = get(); assert(real_contract && real_contract->abiTag == 0x584C364D525434ULL);
    altered_contract = *real_contract; different_graph = *real_contract->graph;
    /* Keep the original legacy-V3 guarded teardown controls, rather than
     * retagging their permissive fallback expectations as a V4 contract. */
    legacy_contract = (XL6_ModuleRetirementV3){sizeof(legacy_contract),0x584C364D525433ULL,real_contract->graph,real_contract->preflight};
    if (version == 1) --legacy_contract.size;
    if (version == 2) legacy_contract.abiTag = 0x584C364D525432ULL;
    if (version == 3) legacy_contract.graph = &different_graph;
    if (version == 4) legacy_contract.preflight = NULL;
    if (version >= 8 && version <= 10) altered_contract.preflight = checked_preflight;
    if (version == 11) --altered_contract.size;
    if (version == 12) altered_contract.abiTag = 0x584C364D525433ULL;
    if (version == 13) altered_contract.graph = &different_graph;
    if (version == 14) altered_contract.preflight = NULL;
    if (version == 16) altered_contract.step = NULL;
    bind(&hooks); xl__tcc.tcc_get_symbol = version ? protocol_lookup : lookup; xl__tcc.tcc_delete = delete_state;
    xlApiLock(); module = xlCreateModuleFromState(&context, compiled_state); xlApiUnlock();
    if(version>=11){
        assert(!module && context.lastError && strstr(context.lastError,"invalid module retirement V4 contract"));
        xl__tcc.tcc_get_symbol=lookup;return;
    }
    assert(module);
    assert(!module->retirementPreparation);
    if (version >= 1 && version <= 6) assert(!module->retirementContract && !module->retirementStep);
    else assert(module->retirementContract && (!!module->retirementStep == (version != 7)));
    xl__tcc.tcc_get_symbol = lookup;
    assert(xlActivateModule(&context, module, NULL) && !xrtGetError());
#ifdef MODULE_EXECUTOR_CREATED
    MODULE_EXECUTOR_CREATED();
#endif
}
static void balance_cycle(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot now; finish(); assert(atomic_load(&deleted) == 1 && atomic_load(&global_drops) == 1);
    xrtClearError(); xrtFree(context.lastError); context.lastError = NULL; xrtMemDebugSnapshot(&now);
    assert(now.LiveCount == before->LiveCount && now.LiveBytes == before->LiveBytes &&
        now.AllocCount - before->AllocCount == now.FreeCount - before->FreeCount &&
        now.InvalidFreeCount == before->InvalidFreeCount && now.DoubleFreeCount == before->DoubleFreeCount &&
        now.UseAfterFreeCount == before->UseAfterFreeCount); ++transactions;
}
static void quiet_executor(xrtownershipref executor, xrtownershipscope* freeze)
{
    XL6_DeadlineV1 deadline = xl6DeadlineAfterMillisecondsV1(5000);
    for (;;) {
        if (xrtOwnershipFreezeTryBegin(freeze)) {
            size_t count; if (executor.Ops->Count(executor.Data, &count)) return;
            assert(xrtOwnershipScopeEnd(freeze));
        }
        assert(!xl6DeadlineExpiredV1(deadline)); xrtThreadYield();
    }
}
typedef struct TraceCount { xrtownershipref executor; unsigned edges; } TraceCount;
static bool trace_executor(xrtownershipref ref, void* data)
{
    TraceCount* trace = data;
    if (ref.Data == trace->executor.Data && ref.Ops == trace->executor.Ops) ++trace->edges;
    return true;
}
static void cycle_transaction(unsigned width, bool cleanupWait, bool nativeRoot, bool feeder, bool acyclic, int faultBudget)
{
    xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    cycle_width = width; expected_cleanups = width; loop_start = feeder ? 1 : 0; terminal_future = NULL;
    create_cycle_module(0);
    xfuture *boot = NULL, *cleanup = NULL; xpromise *bootPromise = xrtPromiseCreate(&boot, NULL), *cleanupPromise = xrtPromiseCreate(&cleanup, NULL), *terminal = NULL;
    XL6_CodeLease* guard = NULL; assert(bootPromise && cleanupPromise);
    if (!cleanupWait) assert(xrtPromiseResolve(cleanupPromise, &number));
    if (acyclic) { terminal = xrtPromiseCreate(&terminal_future, NULL); assert(terminal && xrtPromiseResolve(terminal, &number)); }
    xlApiLock(); assert(xlModuleBeginCallUnlocked(module)); xlApiUnlock();
    for (unsigned i = 0; i < width; ++i) { cycle_outputs[i] = start_cycle(i, boot, cleanup, cleanupWait); assert(cycle_outputs[i] && !xrtGetError()); }
    xlApiLock(); xlModuleEndCallUnlocked(module); xlApiUnlock();
    assert(xrtPromiseResolve(bootPromise, &number)); XL6_DeadlineV1 deadline = xl6DeadlineAfterMillisecondsV1(5000);
    while (atomic_load(&peer_count) != width) { assert(!xl6DeadlineExpiredV1(deadline)); xrtThreadYield(); }
    xrtownershipref executor = executor_view(); xrtownershipscope freeze = {0}; quiet_executor(executor, &freeze);
    TraceCount trace = {executor, 0}; assert(module->ownershipTrace(trace_executor, &trace) && trace.edges == 1);
    assert(xrtOwnershipScopeEnd(&freeze));
    /* READY before pending work drains is exclusively the new V3 contract.
     * No executor Close/Destroy or worker substitution occurs in this host. */
    xlApiLock();
#ifdef MODULE_EXECUTOR_PREPARE
    assert(MODULE_EXECUTOR_PREPARE() && module->retirementPrepared);
#else
    assert(xlModulePrepareRetirementUnlocked(module) && module->retirementPrepared);
#endif
    xlApiUnlock();
    if (!acyclic) for (unsigned i = 0; i < width; ++i) assert(!xrtFutureDone(cycle_outputs[i]) && !atomic_load(&catches[i]));
    xrtPromiseDestroy(bootPromise); xrtFutureDestroy(boot);
    if (nativeRoot) assert(xl6CodeLeaseAcquire(&module->codeOwnerGraph.base, &guard) == XL6_CODE_LEASE_ACQUIRED);
    if (acyclic) {
        for (unsigned i = 0; i < width; ++i) assert(xrtFutureWaitFor(cycle_outputs[i], 5000000) == XWAIT_OK);
        xlDestroyModule(module); if (guard) xl6CodeLeaseRelease(guard); finish();
        for (unsigned i = 0; i < width; ++i) {
            assert(xrtFutureState(cycle_outputs[i]) == XFUTURE_RESOLVED && *(int64_t*)xrtFutureValue(cycle_outputs[i]) == number);
            assert(!atomic_load(&catches[i]) && atomic_load(&finals[i]) == 1 && atomic_load(&afters[i]) == 1);
            xrtFutureDestroy(cycle_outputs[i]);
        }
    } else {
        if (faultBudget >= 0) assert(xrtMemDebugFailAfter((uint64)faultBudget));
        xlDestroyModule(module);
        if (faultBudget >= 0) { if (xrtMemDebugFailTriggered()) ++oom_failures; xrtMemDebugFailClear(); xrtClearError(); }
        assert(!atomic_load(&deleted) && !module->ownershipCleared && !module->retirementFailed);
        for (unsigned i = nativeRoot ? 0 : 1; i < width; ++i) xrtFutureDestroy(cycle_outputs[i]);
        for (unsigned pass = 0; pass < 4; ++pass) collect();
        assert(!atomic_load(&deleted) && !atomic_load(&global_drops));
        for (unsigned i = 0; i < width; ++i) assert(!atomic_load(&catches[i]) && !atomic_load(&finals[i]));
        ++rooted_checks;
#ifdef MODULE_EXECUTOR_BEFORE_UNROOTED
        if (!nativeRoot) MODULE_EXECUTOR_BEFORE_UNROOTED();
#endif
        if (guard) xl6CodeLeaseRelease(guard); else xrtFutureDestroy(cycle_outputs[0]);
#ifdef MODULE_EXECUTOR_UNROOTED
        if (!nativeRoot) MODULE_EXECUTOR_UNROOTED(executor, feeder);
#endif
        if (cleanupWait) {
            deadline = xl6DeadlineAfterMillisecondsV1(5000);
            for (;;) {
                collect(); unsigned entered = 0; for (unsigned i = 0; i < width; ++i) entered += atomic_load(&catches[i]);
                if (entered) { assert(entered == 1); break; }
                assert(!xl6DeadlineExpiredV1(deadline)); xrtThreadYield();
            }
            assert(!atomic_load(&deleted)); quiet_executor(executor, &freeze);
            for (unsigned i = 0; i < width; ++i) {
                assert(!xrtFutureDone(cycle_outputs[i]) && !atomic_load(&finals[i]));
                if (feeder && i == 0) { xcancel* cancel = xrtFutureCancelToken(cycle_outputs[i]); assert(cancel && !xrtCancelRequested(cancel)); xrtCancelDestroy(cancel); }
                assert(xrtFutureRef(cycle_outputs[i]) == cycle_outputs[i]);
            }
            assert(xrtOwnershipScopeEnd(&freeze));
            for (unsigned pass = 0; pass < 4; ++pass) collect();
            assert(!atomic_load(&deleted) && !module->ownershipCleared && !atomic_load(&global_drops)); ++cleanup_roots;
            assert(xrtPromiseResolve(cleanupPromise, &number));
        }
        finish();
        for (unsigned i = 0; i < width; ++i) {
            assert(atomic_load(&catches[i]) == 1 && atomic_load(&finals[i]) == 1 && !atomic_load(&afters[i]));
            if (cleanupWait) { assert(xrtFutureState(cycle_outputs[i]) == XFUTURE_CANCELLED); xrtFutureDestroy(cycle_outputs[i]); }
        }
    }
    xrtFutureDestroy(terminal_future); terminal_future = NULL; xrtPromiseDestroy(terminal);
    xrtPromiseDestroy(cleanupPromise); xrtFutureDestroy(cleanup); balance_cycle(&before);
}
static void protocol_transaction(unsigned version)
{
    xmemdebugsnapshot before; XL6_CodeLease* guard = NULL; XL6_CodeScope scope = {0};
    xrtMemDebugSnapshot(&before); expected_cleanups = 0; create_cycle_module(version);
    if (version <= 7) {
        XL_ScriptUnitCheckedProc unit; void* symbol = lookup(compiled_state, "xl_script_module_unit_checked"); memcpy(&unit, &symbol, sizeof(unit));
        assert(xl6CodeLeaseAcquire(&module->codeOwnerGraph.base, &guard) == XL6_CODE_LEASE_ACQUIRED);
        xlDestroyModule(module); collect(); assert(!atomic_load(&deleted) && !module->ownershipPlan && !protocol_calls);
        xl6CodeScopeEnter(&scope, guard); assert(unit && unit()); xl6CodeScopeLeave(&scope);
        xl6CodeLeaseRelease(guard); ++protocol_refusals;
    } else {
        xlDestroyModule(module);
        /* Worker parking can defer freeze admission before the first actual
         * preflight callback. Wait only for that callback; its first BUSY or
         * FAILED result and the second-call retry assertions stay exact. */
        XL6_DeadlineV1 deadline=xl6DeadlineAfterMillisecondsV1(5000);
        while(!protocol_calls){
            assert(!atomic_load(&deleted)&&!module->retirementPrepared&&!module->retirementFailed&&
                !atomic_load(&global_drops)&&!xl6DeadlineExpiredV1(deadline));
            collect();xrtThreadYield();
        }
        assert(!atomic_load(&deleted) && protocol_calls == 1 && !module->retirementPrepared);
        assert(!!module->retirementFailed == (version != 8)); xrtClearError();
        finish(); assert(protocol_calls == 2); ++readiness_retries;
    }
    balance_cycle(&before);
}
static void strict_protocol_transaction(unsigned version)
{
    xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);expected_cleanups=0;create_cycle_module(version);
    assert(!module && !atomic_load(&deleted) && !atomic_load(&global_drops) && !atomic_load(&cleanups));
    /* A rejected factory leaves the real, unactivated image caller-owned.
     * No fake module shell or runtime destructor is needed to dispose of it. */
    MEMORY_BASIC_INFORMATION memory;
#ifdef MODULE_EXECUTOR_DLL
    void* address=executor_entry;
    assert(FreeLibrary(artifact) && GetModuleHandleA(JOINT_DLL_NAME)==NULL);artifact=NULL;executor_entry=NULL;
#else
    void* address=relocated_entry;tcc_delete(compiled_state);relocated_entry=NULL;
#endif
    assert(VirtualQuery(address,&memory,sizeof(memory))==sizeof(memory) && memory.State==MEM_FREE);
    compiled_state=NULL;atomic_fetch_add(&deleted,1);xrtFree(context.lastError);context.lastError=NULL;xrtClearError();
    xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes && after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount);
    assert(before.InvalidFreeCount==after.InvalidFreeCount && before.DoubleFreeCount==after.DoubleFreeCount && before.UseAfterFreeCount==after.UseAfterFreeCount);++strict_refusals;
}
#ifndef MODULE_EXECUTOR_MIXED
int main(void)
{
    assert(xrtMemDebugEnable(true)); expected_cleanups = 0;
#ifndef MODULE_EXECUTOR_DLL
    provider_init();
#endif
    for (unsigned round = 0; round < 5; ++round) {
        for (unsigned width = 1; width <= 3; ++width) for (unsigned cleanup = 0; cleanup < 2; ++cleanup)
            for (unsigned root = 0; root < 2; ++root) cycle_transaction(width, cleanup != 0, root != 0, false, false, -1);
        for (unsigned cleanup = 0; cleanup < 2; ++cleanup) for (unsigned root = 0; root < 2; ++root)
            cycle_transaction(3, cleanup != 0, root != 0, true, false, -1);
        for (unsigned width = 1; width <= 2; ++width) for (unsigned root = 0; root < 2; ++root)
            cycle_transaction(width, false, root != 0, false, true, -1);
        for (unsigned version = 1; version <= 10; ++version) protocol_transaction(version);
        for (unsigned version = 11; version <= 16; ++version) strict_protocol_transaction(version);
    }
    for (int budget = 0; budget < 32; ++budget) cycle_transaction(2, false, false, false, false, budget);
    assert(transactions == 182 && rooted_checks == 112 && cleanup_roots == 40 && protocol_refusals == 35 && readiness_retries == 15 && strict_refusals == 30 && oom_failures > 0);
    printf("Generated module executor: 182 real unloads, 60 rings, 20 feeders, 20 acyclic controls, 112 external-root checks, 40 cleanup-await roots, 30 malformed-contract refusals, 5 legacy guarded teardowns, 15 readiness retries, 32 snapshot budgets, %u actual OOM; original module executor, catch/finally/globals, MEM_FREE and exact memory balance\n", oom_failures);
    puts("Strict V4 admission: 30 malformed contracts rejected before activation; real images retired with MEM_FREE and full allocation balance");
    return 0;
}
#endif
