static void bind_mixed_callbacks(void);
static int prepare_mixed_module(void);
#define MODULE_EXECUTOR_MIXED
#define MODULE_EXECUTOR_CREATED bind_mixed_callbacks
#define MODULE_EXECUTOR_PREPARE prepare_mixed_module
#ifndef JOINT_TCC_SOURCE_FILE
#define JOINT_TCC_SOURCE_FILE "test/cases/xlir_module_mixed_dependency_artifact.c"
#endif
#include "xlir_module_executor_host.c"
static atomic_uint callback_finally[3][2];
static unsigned callback_count;
static unsigned preflight_calls, preflight_total, admission_deferrals;
static XL6_ModuleRetirementV3 observed_preflight;
static XL6_ModulePrepareResult mixed_preflight(void)
{
    assert(!preflight_calls++ && module->preparationRefs == 1 && module->retirementRunning);
    XL6_ModulePrepareResult result = real_contract->preflight();
    assert(result == XL6_MODULE_PREPARE_READY); ++preflight_total; return result;
}
static int prepare_mixed_module(void)
{
    xdeadline deadline = xrtDeadlineAfter(5000000);
    while (!xlModulePrepareRetirementUnlocked(module)) {
        /* The host's nonblocking freeze probe can race a native task tail or
         * parked worker transition after quiet_executor released its freeze.
         * Retry ONLY pre-callback admission: an invoked BUSY/FAILED callback,
         * an active retirement transaction or any diagnostic is a failure. */
        assert(!preflight_calls && !module->retirementFailed && !xrtGetError() &&
            !module->ownershipPlan && !module->preparationRefs && !module->retirementRunning && !xl__ownershipDriverRunning);
        ++admission_deferrals; xlApiUnlock();
        assert(!xrtDeadlineExpired(deadline)); xrtThreadYield(); xlApiLock();
    }
    assert(preflight_calls == 1); return 1;
}
static void mixed_callback(int64_t id, int64_t stage)
{
    assert(id >= 0 && (unsigned)id < cycle_width && stage >= 0 && stage < 2);
    assert(!atomic_load(&deleted) && !atomic_load(&global_drops));
    assert(!atomic_fetch_add(&callback_finally[id][stage], 1));
}
static void bind_mixed_callbacks(void)
{
    typedef void (*Bind)(void (*)(int64_t, int64_t)); Bind bind;
    for (unsigned i = 0; i < 3; ++i) for (unsigned j = 0; j < 2; ++j) atomic_store(&callback_finally[i][j], 0);
    void* symbol = lookup(compiled_state, "mixed_dependency_bind"); memcpy(&bind, &symbol, sizeof(bind));
    assert(bind); bind(mixed_callback);
    preflight_calls = 0; observed_preflight = *real_contract; observed_preflight.preflight = mixed_preflight;
    module->retirementPreflight = &observed_preflight;
#ifdef MODULE_MIXED_CREATED
    MODULE_MIXED_CREATED();
#endif
}
static void mixed_transaction(unsigned width, bool cleanup, bool root, bool feeder, bool acyclic, int budget)
{
    cycle_transaction(width, cleanup, root, feeder, acyclic, budget);
    for (unsigned i = 0; i < width; ++i) for (unsigned j = 0; j < 2; ++j) {
        assert(atomic_load(&callback_finally[i][j]) == 1); ++callback_count;
    }
}
#ifndef MODULE_MIXED_CUSTOM_MAIN
int main(void)
{
    assert(xrtMemDebugEnable(true)); expected_cleanups = 0;
#ifndef MODULE_EXECUTOR_DLL
    provider_init();
#endif
    for (unsigned round = 0; round < 5; ++round) {
        for (unsigned width = 1; width <= 3; ++width) for (unsigned cleanup = 0; cleanup < 2; ++cleanup)
            for (unsigned root = 0; root < 2; ++root) mixed_transaction(width, cleanup != 0, root != 0, false, false, -1);
        for (unsigned cleanup = 0; cleanup < 2; ++cleanup) for (unsigned root = 0; root < 2; ++root)
            mixed_transaction(3, cleanup != 0, root != 0, true, false, -1);
        for (unsigned width = 1; width <= 2; ++width) for (unsigned root = 0; root < 2; ++root)
            mixed_transaction(width, false, root != 0, false, true, -1);
    }
    for (int budget = 0; budget < 32; ++budget) mixed_transaction(2, false, false, false, false, budget);
    assert(transactions == 132 && rooted_checks == 112 && cleanup_roots == 40 && callback_count == 548 && oom_failures > 0 && preflight_total == 132);
    printf("Mixed producer modules: 132 real unloads, 60 rings, 20 feeders, 20 acyclic controls, 112 external-root checks, 40 cleanup-await roots, 548 callback finally executions, 32 snapshot budgets, %u actual OOM; real task pool, four callback frames per await, original async catch/finally, globals, MEM_FREE and exact memory balance\n", oom_failures);
    printf("Mixed preflight: 132 generated READY callbacks, %u pre-callback admission deferrals; no BUSY or FAILED callback retry\n", admission_deferrals);
    return 0;
}
#endif
