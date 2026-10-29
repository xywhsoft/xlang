#define XL_STATIC_EMBED
#ifndef XL_ENABLE_EXE_COMPILER
#define XL_ENABLE_EXE_COMPILER 0
#endif
#include "src/api/xl_api.c"
#include "src/api/xl_api_path.c"
#include "src/common/xl6_console.c"
#include "src/runtime/xl6_continuation_executor.h"
#include "src/runtime/xl6_continuation_future.h"
#include "src/runtime/xl6_future_payload.h"
#include <assert.h>
#include <stdatomic.h>
#ifndef JOINT_DLL_PATH
#define JOINT_DLL_PATH "build/test/xlir_async_module_lifetime.generated.dll"
#define JOINT_DLL_NAME "xlir_async_module_lifetime.generated.dll"
#endif
#ifndef JOINT_STATE_DESCRIPTION
#define JOINT_STATE_DESCRIPTION "real DLL unloads"
#endif
#ifndef JOINT_PROVIDER_INIT
#define JOINT_PROVIDER_INIT() ((void)0)
#endif
typedef struct JointHooks {
    int fail_init;
    xfuture* inputs[2];
    int64_t (*probe)(int64_t);
    void (*finally)(void);
    int fail_main;
    void (*entry_probe)(void);
    void (*global_drop)(void);
} JointHooks;
static XL_Context context;
static XL_Module* module;
static HMODULE artifact;
static TCCState* compiled_state;
static atomic_uint cleanups, probes, deleted;
static unsigned mode;
static unsigned expected_cleanups = 1;
#ifndef JOINT_CUSTOM_PROVIDER
static void load_artifact(void)
{
    artifact = LoadLibraryA(JOINT_DLL_PATH); assert(artifact);
    compiled_state = (TCCState*)artifact;
}
static void* lookup(TCCState* state, const char* name)
{ assert(state == compiled_state); return (void*)GetProcAddress(artifact, name); }
static void delete_state(TCCState* state)
{
    assert(state == compiled_state && module->codeLeases == 0 && module->activeCalls == 0);
    assert(atomic_load(&cleanups) == expected_cleanups);
    assert(FreeLibrary(artifact));
    assert(GetModuleHandleA(JOINT_DLL_NAME) == NULL);
    atomic_fetch_add(&deleted, 1);
}
#endif
static int64_t probe(int64_t value)
{
    assert(!atomic_load(&deleted) && value == 42);
    atomic_fetch_add(&probes, 1);
    if (mode == 4) xlDestroyModule(module); /* retire from a real generated CALL */
    return value;
}
static void finally(void)
{ assert(!atomic_load(&deleted)); atomic_fetch_add(&cleanups, 1); }
static void collect(void)
{ xlApiLock(); xlCollectRetiringModulesUnlocked(); xlApiUnlock(); }
static void finish(void)
{
    xdeadline deadline = xrtDeadlineAfter(5000000);
    while (!atomic_load(&deleted)) {
        collect(); assert(!xrtDeadlineExpired(deadline)); xrtThreadYield();
    }
    assert(xl__retiringModules == NULL);
}
int main(int argc, char** argv)
{
    unsigned round;
    assert(argc == 2); mode = (unsigned)strtoul(argv[1], NULL, 10); assert(mode < 6);
    assert(xrtMemDebugEnable(true));
    JOINT_PROVIDER_INIT();
    for (round = 0; round < 25; ++round) {
        JointHooks hooks = {0}; xpromise *first, *second; xfuture* output = NULL;
        xmemdebugsnapshot before, after; xfutureresult result = {0}; int64_t number = 21;
        typedef xfuture* (*Start)(xfuture*, xfuture*); Start start;
        typedef void (*Bind)(const JointHooks*); Bind bind;
        typedef XL6_FuturePayloadDrop (*DropAddress)(void); DropAddress drop_address;
        atomic_store(&cleanups, 0); atomic_store(&probes, 0); atomic_store(&deleted, 0);
        memset(&context, 0, sizeof(context));
        xrtClearError(); xrtMemDebugSnapshot(&before);
        load_artifact();
        { void* symbol = lookup(compiled_state, "joint_bind_hooks");
          _Static_assert(sizeof(symbol) == sizeof(bind), "Windows function pointer ABI"); memcpy(&bind, &symbol, sizeof(bind));
          symbol = lookup(compiled_state, "joint_start_test"); memcpy(&start, &symbol, sizeof(start));
          symbol = lookup(compiled_state, "joint_payload_drop_address"); memcpy(&drop_address, &symbol, sizeof(drop_address)); }
        assert(bind && start);
        {
            HMODULE resident = NULL; XL6_FuturePayloadDrop drop;
            assert(drop_address); drop = drop_address();
            assert(drop == xl6PayloadDropGet(XL6_FUTURE_PAYLOAD_VALUE));
            assert(GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)(uintptr_t)drop, &resident));
            assert(resident == GetModuleHandleA(NULL) && resident != artifact);
        }
        first = xrtPromiseCreate(&hooks.inputs[0], NULL); second = xrtPromiseCreate(&hooks.inputs[1], NULL);
        assert(first && second); hooks.fail_init = mode == 5; hooks.probe = probe; hooks.finally = finally; bind(&hooks);
        xl__tcc.tcc_get_symbol = lookup; xl__tcc.tcc_delete = delete_state;
        xlApiLock(); module = xlCreateModuleFromState(&context, compiled_state); xlApiUnlock(); assert(module);
        if (mode == 5) { assert(!xlActivateModule(&context, module, NULL)); xrtClearError(); }
        else {
            assert(xlActivateModule(&context, module, NULL));
            xlApiLock(); assert(xlModuleBeginCallUnlocked(module)); xlApiUnlock();
            output = start(hooks.inputs[0], hooks.inputs[1]); assert(output && xrtGetError() == NULL);
            xlApiLock(); xlModuleEndCallUnlocked(module); xlApiUnlock();
        }
        xlApiLock(); assert(module->codeLeases == 1 && !atomic_load(&deleted)); xlApiUnlock();
        if (mode != 5) assert(!xlDeactivateModule(&context, module));
        xrtClearError();
        if (mode != 4) xlDestroyModule(module); /* consumed once, never retried by handle */
        collect(); assert(!atomic_load(&deleted));
        assert(xrtPromiseResolve(first, &number));
        if (mode == 1) {
            xerror* error = xrtErrorCreate(XERR_STATE, "joint", 71, "input failed");
            assert(error && xrtPromiseReject(second, error)); xrtErrorFree(error);
        } else if (mode == 2) assert(xrtPromiseCancel(second));
        else assert(xrtPromiseResolve(second, &number));
        if (output) {
            assert(xrtFutureWaitFor(output, 5000000) == XWAIT_OK && xrtFutureResult(output, &result));
            assert(result.State == (mode == 1 ? XFUTURE_FAILED : mode == 2 ? XFUTURE_CANCELLED : XFUTURE_RESOLVED));
            if (result.State == XFUTURE_RESOLVED) assert(result.Value && *(int64_t*)result.Value == 42);
            if (mode == 3) {
                /* A trivial payload needs no producer-module code. Prove the
                 * module is really unmapped BEFORE reading and dropping it. */
                finish();
                assert(*(int64_t*)xrtFutureValue(output) == 42);
            }
            xrtFutureDestroy(output);
        }
        finish();
        assert(atomic_load(&probes) == (mode == 1 || mode == 2 ? 0u : 1u));
        xrtFutureDestroy(hooks.inputs[0]); xrtFutureDestroy(hooks.inputs[1]);
        xrtPromiseDestroy(first); xrtPromiseDestroy(second);
        xrtFree(context.lastError); xrtClearError(); xrtMemDebugSnapshot(&after);
        assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
        assert(after.AllocCount-before.AllocCount == after.FreeCount-before.FreeCount);
    }
    printf("generated async/source API mode=%u: 25 %s, pending entry/retirement/cleanup and balanced memory\n", mode, JOINT_STATE_DESCRIPTION);
    return 0;
}
