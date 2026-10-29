/* Reuse the real source API, resident executor and DLL-unmapping harness.
 * Only the relocated-state provider is substituted, not the execution path. */
#ifndef JOINT_DLL_PATH
#define JOINT_DLL_PATH "build/test/xlir_one_shot_lifetime.generated.dll"
#define JOINT_DLL_NAME "xlir_one_shot_lifetime.generated.dll"
#endif
#define main joint_other_modes_main
#include "xlir_async_module_lifetime_host.c"
#undef main
static atomic_uint global_drops, entry_probes;
static void entry_probe(void)
{
    xlApiLock();
    assert(module->codeOwnerReady && module->activeCalls == 1 && !module->destroyRequested);
    xlApiUnlock(); atomic_fetch_add(&entry_probes, 1);
}
static void global_drop(void)
{
    assert(!atomic_load(&deleted) && atomic_load(&cleanups) == expected_cleanups);
    atomic_fetch_add(&global_drops, 1);
}
int main(int argc, char** argv)
{
    unsigned round, selected;
    assert(argc == 2); selected = (unsigned)strtoul(argv[1], NULL, 10); assert(selected < 7);
    mode = 0; /* The shared async probe must not issue a second Destroy. */
    expected_cleanups = selected == 6 ? 0 : 1;
    assert(xrtMemDebugEnable(true));
    JOINT_PROVIDER_INIT();
    for (round = 0; round < 25; ++round) {
        JointHooks hooks = {0}; xpromise *first, *second; int64_t number = 21; int ok;
        xmemdebugsnapshot before, after;
        typedef void (*Bind)(const JointHooks*); Bind bind;
        xrtClearError(); xrtMemDebugSnapshot(&before);
        memset(&context, 0, sizeof(context));
        atomic_store(&cleanups, 0); atomic_store(&probes, 0); atomic_store(&deleted, 0);
        atomic_store(&global_drops, 0); atomic_store(&entry_probes, 0);
        load_artifact();
        { void* symbol = lookup(compiled_state, "joint_bind_hooks"); memcpy(&bind, &symbol, sizeof(bind)); }
        assert(bind);
        first = xrtPromiseCreate(&hooks.inputs[0], NULL); second = xrtPromiseCreate(&hooks.inputs[1], NULL);
        assert(first && second);
        hooks.fail_init = selected == 1; hooks.fail_main = selected == 6 ? 2 : selected == 2;
        hooks.probe = probe; hooks.finally = finally; hooks.entry_probe = entry_probe; hooks.global_drop = global_drop;
        bind(&hooks);
        if (selected == 3 || selected == 4) {
            assert(xrtPromiseResolve(first, &number));
            if (selected == 4) assert(xrtPromiseCancel(second));
            else assert(xrtPromiseResolve(second, &number));
        }
        xl__tcc.tcc_get_symbol = lookup; xl__tcc.tcc_delete = delete_state;
        xlApiLock();
        module = xlCreateModuleFromState(&context, compiled_state); assert(module);
        ok = xlRunAndRetireModuleUnlocked(&context, module, NULL); /* Consumes handle. */
        xlApiUnlock();
        assert(atomic_load(&entry_probes) == (selected == 6 ? 0u : 2u));
        if (selected == 6) {
            assert(ok && context.lastExitCode == 42 && atomic_load(&deleted) == 1);
        } else if (selected == 3 || selected == 4) {
            /* Ready inputs still dispatch asynchronously. Successful cleanup
             * may be deferred to the checked resident retirement owner. BUSY
             * is not an entry failure; no genuine error may be discarded. */
            assert(ok && context.lastExitCode == 42 && !context.lastError);
            finish();
        } else {
            if (selected == 1 || selected == 2)
                assert(!ok && context.lastError && strstr(context.lastError, "AssertionError"));
            else
                assert(ok && context.lastExitCode == 42 && !context.lastError);
            assert(!atomic_load(&deleted) && !atomic_load(&global_drops));
            /* Handle was consumed. Only the retirement owner may collect it. */
            collect(); assert(!atomic_load(&deleted));
            assert(xrtPromiseResolve(first, &number));
            if (selected == 5) assert(xrtPromiseCancel(second));
            else assert(xrtPromiseResolve(second, &number));
            finish();
        }
        assert(atomic_load(&global_drops) == 1 && atomic_load(&cleanups) == expected_cleanups);
        assert(atomic_load(&probes) == (selected == 4 || selected == 5 || selected == 6 ? 0u : 1u));
        assert(xl__retiringModules == NULL);
        if (selected == 1 || selected == 2) {
            /* Diagnostic text must be host-owned after the module is gone. */
            assert(atomic_load(&deleted) == 1 && strstr(context.lastError, "AssertionError"));
        }
        xrtFutureDestroy(hooks.inputs[0]); xrtFutureDestroy(hooks.inputs[1]);
        xrtPromiseDestroy(first); xrtPromiseDestroy(second);
        xrtFree(context.lastError); xrtClearError(); xrtMemDebugSnapshot(&after);
        assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
        assert(after.AllocCount-before.AllocCount == after.FreeCount-before.FreeCount);
    }
    printf("one-shot mode=%u: 25 %s, checked status, bound entry, globals after async, balanced memory\n", selected, JOINT_STATE_DESCRIPTION);
    return 0;
}
