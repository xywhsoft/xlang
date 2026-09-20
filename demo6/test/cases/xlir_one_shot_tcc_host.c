#include "lib/tcc/libtcc.h"
#include "src/runtime/xl6_closure.h"
#include "src/runtime/xl6_slot.h"
#include "src/runtime/xl6_generator.h"
#include "src/runtime/xl6_value_iterator.h"
#include "src/runtime/xl6_frame_storage.h"
#include "src/runtime/xl6_task.h"
#include "src/runtime/xl6_future_callback.h"
#include "src/runtime/xl6_future_aggregate.h"
static void load_artifact(void);
static void* lookup(TCCState*, const char*);
static void delete_state(TCCState*);
static void provider_init(void);
#define JOINT_CUSTOM_PROVIDER
#define JOINT_STATE_DESCRIPTION "actual TCC compile/relocate/delete + MEM_FREE"
#define JOINT_PROVIDER_INIT() provider_init()
#ifndef JOINT_TCC_SOURCE_FILE
#ifdef JOINT_TCC_ASYNC
#define JOINT_TCC_SOURCE_FILE "test/cases/xlir_async_module_lifetime_artifact.c"
#else
#define JOINT_TCC_SOURCE_FILE "test/cases/xlir_one_shot_lifetime_artifact.c"
#endif
#endif
#ifdef JOINT_TCC_ASYNC
#include "xlir_async_module_lifetime_host.c"
#else
#include "xlir_one_shot_lifetime_host.c"
#endif

static void* relocated_entry;
typedef void (*joint_memdebug_snapshot_fn)(xmemdebugsnapshot*);
typedef size_t (*joint_memdebug_visit_live_fn)(xmemdebugallocationvisitor, ptr);
static joint_memdebug_snapshot_fn provider_snapshot;
static joint_memdebug_visit_live_fn provider_visit_live;
static void compiler_error(void* unused, const char* message)
{ (void)unused; fprintf(stderr, "TCC: %s\n", message); }
static void add_runtime_symbol(TCCState* state, const char* name, const void* address)
{ assert(address && tcc_add_symbol(state, name, address) == 0); }
static void load_artifact(void)
{
    HMODULE runtime = GetModuleHandleA("xl.dll");
    typedef int (*Register)(TCCState*, int); Register register_environment;
    MEMORY_BASIC_INFORMATION memory;
    assert(runtime);
    { FARPROC symbol = GetProcAddress(runtime, "xlTccRegisterXrtEnvironment"); memcpy(&register_environment, &symbol, sizeof(register_environment)); }
    assert(register_environment);
    compiled_state = tcc_new(); assert(compiled_state);
    tcc_set_error_func(compiled_state, NULL, compiler_error);
    assert(tcc_set_output_type(compiled_state, TCC_OUTPUT_MEMORY) == 0);
#ifndef JOINT_SOURCE_XRT
    assert(register_environment(compiled_state, TCC_OUTPUT_MEMORY));
#endif
#ifdef JOINT_TCC_INCLUDE_DIRECTORY
    assert(tcc_add_include_path(compiled_state, JOINT_TCC_INCLUDE_DIRECTORY) == 0);
#else
    assert(tcc_add_include_path(compiled_state, "build/test") == 0);
#endif
    assert(tcc_add_include_path(compiled_state, "test/cases") == 0);
    assert(tcc_add_include_path(compiled_state, ".") == 0);
    /* Release environment supplies the unchanged XRT/error ABI. Source-owned
     * dispatch and payload services override it with resident host addresses. */
#define BIND(name) add_runtime_symbol(compiled_state, #name, (const void*)name)
#ifdef JOINT_SOURCE_XRT
    /* The released DLL supplies only the TCC provider in this lane. Every
     * generated XRT call uses the host's current source implementation. */
#define XL_XRT_SYMBOL(name) BIND(name);
#include "src/api/xl_xrt_symbols.inc"
#undef XL_XRT_SYMBOL
    BIND(xl6ErrorScopeBegin);
    BIND(xl6ErrorScopeBeginCleanup);
    BIND(xl6ErrorScopeEnd);
    BIND(xl6ForeignCallBegin);
    BIND(xl6ForeignCallEnd);
    BIND(xl6ErrorScopeCaptureFailure);
    BIND(xl6ErrorScopePropagateFailure);
    BIND(xl6ForeignCallbackFail);
#endif
    BIND(xl6ContinuationExecutorCreateGuaranteed);
    BIND(xl6ContinuationExecutorReserveOwned);
    BIND(xl6ContinuationExecutorClose);
    BIND(xl6ContinuationExecutorDestroy);
    BIND(xl6ContinuationExecutorOwnership);
    BIND(xl6ContinuationExecutorIsIdle);
    BIND(xl6ContinuationReserveSuccessor);
    BIND(xl6ContinuationAbort);
    BIND(xl6ContinuationCommit);
    BIND(xl6ContinuationWatchRegister);
    BIND(xl6ContinuationTraceBind);
    BIND(xl6ContinuationOwnership);
    BIND(xl6PayloadDropGet);
    BIND(xl6PayloadTraceGet);
    BIND(xl6PayloadResolveOwned);
    BIND(xl6PayloadObserveOutcome);
    BIND(xl6FutureAggregateRuntimeV1Get);
    BIND(xl6ClosureRuntimeV1Get);
    BIND(xl6ClosureRuntimeV2Get);
    BIND(xl6ClassRuntimeV1Get);
    BIND(xl6ClassRuntimeV2Get);
    BIND(xl6SlotRuntimeV1Get);
    BIND(xl6GeneratorRuntimeV1Get);
    BIND(xl6GeneratorRuntimeV2Get);
    BIND(xl6GeneratorOwnershipAdapterV1);
    BIND(xl6ContinuationFrameRuntimeV2Get);
    BIND(xl6ContinuationFrameRuntimeV3Get);
    BIND(xl6ContinuationFrameRuntimeV4Get);
    BIND(xl6ContinuationProducerPolicyV1Get);
    BIND(xl6ContinuationWatchPolicyV1Get);
    BIND(xl6ContinuationFrameOwnershipAdapterV1);
    BIND(xl6ValueIteratorRuntimeV1Get);
    BIND(xl6FrameStorageRuntimeV1Get);
    BIND(xl6TaskRuntimeV1Get);
    BIND(xl6TaskRuntimeV2Get);
    BIND(xl6TaskProducerPolicyV1Get);
    BIND(xl6TaskNativeDataPolicyV1Get);
    BIND(xl6TaskWatchPolicyV1Get);
    BIND(xl6TaskOwnershipAdapterV1);
    BIND(xl6TaskPoolOwnership);
    BIND(xl6TaskPoolOwnershipAdapterV1);
    BIND(xl6TaskSubmitPayloadV1);
    BIND(xl6TaskPoolTryQuiesceV1);
    BIND(xl6FutureCallbackRuntimeV1Get);
    BIND(xl6FutureCallbackRuntimeV2Get);
    BIND(xl6FutureCallbackProducerPolicyV1Get);
    BIND(xl6FutureCallbackWatchPolicyV1Get);
    BIND(xl6FutureCallbackOwnershipAdapterV1);
    BIND(xl6CaptureRuntimeV1Get);
    BIND(xl6ActivationCaptureRuntimeV1Get);
#undef BIND
#ifdef JOINT_SOURCE_XRT
    /* TCC keeps the first definition of an injected symbol. Register the
     * source implementation BEFORE the historical provider's defaults. */
    assert(register_environment(compiled_state, TCC_OUTPUT_MEMORY));
    assert(tcc_compile_string(compiled_state,
        "#include <__xl_runtime/xruntime_decl.h>\n"
        "const void* joint_runtime_identity(void) { return xrtCallableOwnership(NULL).Ops; }\n") == 0);
#endif
    assert(tcc_add_file(compiled_state, JOINT_TCC_SOURCE_FILE) == 0);
    assert(tcc_relocate(compiled_state) == 0);
#ifdef JOINT_SOURCE_XRT
    /* Compare physical runtime identity, not PE import thunk addresses. */
    { const void* (*identity)(void); void* symbol = tcc_get_symbol(compiled_state, "joint_runtime_identity");
      memcpy(&identity, &symbol, sizeof(identity));
      assert(identity && identity() == xrtCallableOwnership(NULL).Ops); }
#endif
    relocated_entry = tcc_get_symbol(compiled_state, "xl_script_main_checked"); assert(relocated_entry);
    assert(VirtualQuery(relocated_entry, &memory, sizeof(memory)) == sizeof(memory));
    assert(memory.State == MEM_COMMIT);
    assert(memory.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY));
}
static void* lookup(TCCState* state, const char* name)
{ assert(state == compiled_state); return tcc_get_symbol(state, name); }
static void delete_state(TCCState* state)
{
    MEMORY_BASIC_INFORMATION memory;
    assert(state == compiled_state && module->codeLeases == 0 && module->activeCalls == 0);
    assert(atomic_load(&cleanups) == expected_cleanups);
    tcc_delete(state); /* Real allocator/page release, not a stand-in hook. */
    assert(VirtualQuery(relocated_entry, &memory, sizeof(memory)) == sizeof(memory));
    assert(memory.State == MEM_FREE);
    compiled_state = NULL; relocated_entry = NULL;
    atomic_fetch_add(&deleted, 1);
}
static bool identify_environment_allocation(const xmemdebugallocation* allocation, void* data)
{
    unsigned* found = (unsigned*)data;
    if (allocation->Size == sizeof("<static libtcc>") &&
        memcmp(allocation->Address, "<static libtcc>", sizeof("<static libtcc>")) == 0) ++*found;
    return true;
}
static void provider_init(void)
{
    HMODULE runtime = GetModuleHandleA("xl.dll");
    unsigned round;
    assert(runtime != NULL);
    if (provider_snapshot == NULL || provider_visit_live == NULL) {
        FARPROC snapshot_symbol = GetProcAddress(runtime, "xrtMemDebugSnapshot");
        FARPROC visit_symbol = GetProcAddress(runtime, "xrtMemDebugVisitLive");
        assert(snapshot_symbol != NULL && visit_symbol != NULL);
        memcpy(&provider_snapshot, &snapshot_symbol, sizeof(provider_snapshot));
        memcpy(&provider_visit_live, &visit_symbol, sizeof(provider_visit_live));
    }
    for (round = 0; round < 2; ++round) {
        xmemdebugsnapshot before, after, provider_before, provider_after;
        MEMORY_BASIC_INFORMATION memory;
        xrtMemDebugSnapshot(&before);
        provider_snapshot(&provider_before);
        load_artifact(); tcc_delete(compiled_state);
        assert(VirtualQuery(relocated_entry, &memory, sizeof(memory)) == sizeof(memory) && memory.State == MEM_FREE);
        compiled_state = NULL; relocated_entry = NULL;
        xrtMemDebugSnapshot(&after);
        provider_snapshot(&provider_after);
        printf("TCC environment setup %u: live %llu -> %llu, bytes %llu -> %llu\n", round,
            (unsigned long long)before.LiveCount, (unsigned long long)after.LiveCount,
            (unsigned long long)before.LiveBytes, (unsigned long long)after.LiveBytes);
        printf("TCC provider setup %u: live %llu -> %llu, bytes %llu -> %llu\n", round,
            (unsigned long long)provider_before.LiveCount, (unsigned long long)provider_after.LiveCount,
            (unsigned long long)provider_before.LiveBytes, (unsigned long long)provider_after.LiveBytes);
        /* First-use environment caches are not module-owned. The next full
         * compile/delete must be balanced, as must every executed transaction. */
        if (round != 0) {
            unsigned found = 0;
            assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
            assert(after.AllocCount-before.AllocCount == after.FreeCount-before.FreeCount);
            assert(provider_before.LiveCount == provider_after.LiveCount &&
                provider_before.LiveBytes == provider_after.LiveBytes);
            assert(provider_after.AllocCount-provider_before.AllocCount ==
                provider_after.FreeCount-provider_before.FreeCount);
            (void)provider_visit_live(identify_environment_allocation, &found);
            /* The provider DLL owns one process-lifetime environment cache.
             * Query that allocator explicitly: current-source test hosts may
             * also contain an independent XRT implementation. */
            assert(found == 1 && provider_after.LiveCount == 1 &&
                provider_after.LiveBytes == sizeof("<static libtcc>"));
        }
    }
}
