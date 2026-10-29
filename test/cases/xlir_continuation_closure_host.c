/* Actual compiler-generated await -> closure construction after host Close.
 * No explicit extra code pin, replacement reservation service or fake unmap. */
#define JOINT_TCC_ASYNC
#define XL_ENABLE_EXE_COMPILER 1
#define JOINT_TCC_SOURCE_FILE "test/cases/xlir_continuation_closure_artifact.c"
#define main unused_async_lifetime_main
#include "xlir_one_shot_tcc_host.c"
#undef main
static xvalue* captured_output;
static void capture_output(xvalue* value)
{
    assert(!captured_output && value && xrtValueIsFuture(value));
    captured_output = xrtValueRetain(value); assert(captured_output);
}
static bool owner_edge(xrtownershipref edge, void* context)
{
    unsigned* visited = context;
    assert(edge.Data == module && edge.Ops == &xlModuleOwnershipOps); ++*visited; return true;
}
int main(void)
{
    const XL6_ClosureRuntimeV1* api = xl6ClosureRuntimeV1Get();
    assert(xrtMemDebugEnable(true)); expected_cleanups = 0; provider_init();
    for (unsigned round = 0; round < 50; ++round) for (unsigned extracted = 0; extracted < 2; ++extracted) {
        typedef struct Hooks { xfuture* source; void (*capture)(xvalue*); } Hooks;
        typedef void (*Bind)(const Hooks*); Bind bind; void (*start)(void); void* symbol;
        Hooks hooks = {0}; xpromise* producer; xfuture* output;
        xfutureresult result = {0}; XL6_Closure* closure; XL6_ClosureScope scope = {0};
        xmemdebugsnapshot before, after; int64_t* payload; int64_t answer = 0; unsigned edges = 0;
        atomic_store(&deleted, 0); atomic_store(&cleanups, 0);
        memset(&context, 0, sizeof(context)); captured_output = NULL;
        xrtClearError(); xrtMemDebugSnapshot(&before); load_artifact();
        symbol = lookup(compiled_state, "continuation_closure_bind"); memcpy(&bind, &symbol, sizeof(bind));
        symbol = lookup(compiled_state, "continuation_closure_start"); memcpy(&start, &symbol, sizeof(start));
        assert(bind && start);
        producer = xrtPromiseCreate(&hooks.source, NULL); assert(producer);
        hooks.capture = capture_output; bind(&hooks);
        xl__tcc.tcc_get_symbol = lookup; xl__tcc.tcc_delete = delete_state;
        xlApiLock(); module = xlCreateModuleFromState(&context, compiled_state); xlApiUnlock(); assert(module);
        assert(xlActivateModule(&context, module, NULL));
        xlApiLock(); assert(xlModuleBeginCallUnlocked(module)); xlApiUnlock(); start();
        xlApiLock(); xlModuleEndCallUnlocked(module); assert(module->codeLeases == 1); xlApiUnlock();
        assert(captured_output && !xrtGetError()); output = xrtValueGetFuture(captured_output);
        assert(output && xrtFutureState(output) == XFUTURE_PENDING);
        xlDestroyModule(module); collect(); assert(!atomic_load(&deleted));
        payload = xrtMalloc(sizeof(*payload)); assert(payload); *payload = 42;
        assert(xl6PayloadResolveOwned(producer, payload, XL6_FUTURE_PAYLOAD_VALUE));
        assert(xrtFutureWaitFor(output, 5000000) == XWAIT_OK && xrtFutureResult(output, &result));
        if (result.State != XFUTURE_RESOLVED)
            fprintf(stderr, "post-Close closure result state=%d; error=%s\n", (int)result.State,
                result.Error ? xrtErrorMessage(result.Error) : "none");
        assert(result.State == XFUTURE_RESOLVED && result.Value && !atomic_load(&deleted));
        closure = *(XL6_Closure**)result.Value; assert(closure && closure->lease);
        xrtownershipref lease = xl6CodeLeaseOwnership(closure->lease);
        assert(lease.Ops->Trace(lease.Data, owner_edge, &edges) && edges == 1);
        if (extracted) {
            api->Retain(closure); xrtValueRelease(captured_output); captured_output = NULL;
        }
        collect(); assert(!atomic_load(&deleted));
        api->Enter(&scope, closure);
        assert(((bool (*)(void*, int64_t*))closure->invoke)(closure->environment, &answer) && answer == 42);
        api->Leave(&scope);
        if (extracted) api->Release(closure);
        else { xrtValueRelease(captured_output); captured_output = NULL; }
        finish(); assert(atomic_load(&deleted) == 1);
        xrtFutureDestroy(hooks.source); xrtPromiseDestroy(producer); xrtFree(context.lastError);
        xrtClearError(); xrtMemDebugSnapshot(&after);
        assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes &&
            after.AllocCount - before.AllocCount == after.FreeCount - before.FreeCount);
    }
    puts("Generated continuation closures: 100 real TCC close/resume/closure/invoke/unmap lifetimes, 50 Future-held and 50 extracted native references, no explicit owner guard, exact allocator balance");
    return 0;
}
