#define JOINT_TCC_ASYNC
#define JOINT_TCC_SOURCE_FILE "test/cases/xlir_future_retirement_artifact.c"
#define main unused_async_main
#include "xlir_one_shot_tcc_host.c"
#undef main

typedef struct FutureEscapeHooks { void (*capture)(xvalue*); void (*dropped)(int64_t); } FutureEscapeHooks;
static xvalue* alias;
static unsigned choice, internal_drops, external_drops;
static void capture_future(xvalue* value)
{
    assert(value && !alias && module->activeCalls == 1 && xrtValueIsFuture(value));
    alias = xrtValueClone(value);
    assert(alias && !xrtGetError());
}
static void dropped_future(int64_t id)
{
    assert(!atomic_load(&deleted));
    if(id == 42) ++external_drops;
    else { assert(id >= 7 && id <= 9); ++internal_drops; }
}
static bool admit_future(xrtownershipref ref, void* unused)
{
    (void)unused;
    const XL6_OwnershipAdapterV1* adapter=NULL;const xrtownershippreparationv1* preparation=NULL;
    const XL6_OwnershipFinishV1* completion=NULL;
    if(xlModuleResolveOwnershipV3(ref,module,&adapter,&preparation,&completion))return true;
    const char* family = ref.Ops == xrtFutureOwnership((const xfuture*)1).Ops ? "future" :
        ref.Ops == xrtCancelOwnership((const xcancel*)1).Ops ? "cancel" : "other";
    fprintf(stderr,"Future retirement refused family=%s data=%p ops=%p\n",family,ref.Data,(const void*)ref.Ops);
    if(strcmp(family,"future") == 0) {
        xcancel* token=xrtFutureCancelToken((const xfuture*)ref.Data);
        fprintf(stderr,"Future state=%d cancel-admitted=%d\n",(int)xrtFutureState((const xfuture*)ref.Data),
            xrtCancelOwnershipAdapterV1(xrtCancelOwnership(token)) != NULL);
        xrtCancelDestroy(token);
    }
    return false;
}
int main(int argc, char** argv)
{
    assert(argc == 2); choice = (unsigned)strtoul(argv[1], NULL, 10); assert(choice < 6);
    expected_cleanups = 0; assert(xrtMemDebugEnable(true)); provider_init();
    for(unsigned round = 0; round < 25; ++round) {
        typedef void (*Bind)(const FutureEscapeHooks*); typedef int64_t (*Run)(int64_t);
        Bind bind; Run run; bool (*join)(void); FutureEscapeHooks hooks = {capture_future, dropped_future};
        xmemdebugsnapshot before, after; xrtClearError(); xrtMemDebugSnapshot(&before);
        memset(&context, 0, sizeof(context)); alias = NULL; internal_drops = external_drops = 0;
        atomic_store(&deleted, 0); atomic_store(&cleanups, 0); load_artifact();
        void* symbol = lookup(compiled_state, "future_escape_bind"); memcpy(&bind, &symbol, sizeof(bind));
        symbol = lookup(compiled_state, "future_escape_run"); memcpy(&run, &symbol, sizeof(run)); assert(bind && run); bind(&hooks);
        symbol = lookup(compiled_state, "future_escape_join"); memcpy(&join, &symbol, sizeof(join)); assert(join);
        xl__tcc.tcc_get_symbol = lookup; xl__tcc.tcc_delete = delete_state;
        xlApiLock(); module = xlCreateModuleFromState(&context, compiled_state); xlApiUnlock(); assert(module);
        assert(xlActivateModule(&context, module, NULL));
        xlApiLock(); assert(xlModuleBeginCallUnlocked(module)); xlApiUnlock();
        assert(run(choice) == 42 && !xrtGetError());
        assert(join() && !xrtGetError());
        if(choice == 4) {
            xvalue* extracted = xrtValueRetain(*(xvalue**)xrtFutureValue(xrtValueGetFuture(alias))); assert(extracted);
            xrtValueRelease(alias); alias = extracted;
        }
        xlApiLock(); xlModuleEndCallUnlocked(module); xlApiUnlock();
        xrtownershipscope freeze = {0}; xrtownershipsnapshot* graph = NULL; xrtownershipnode node;
        xrtownershipref root = {module, &xlModuleOwnershipOps};
        xlApiLock(); assert(xrtOwnershipFreezeTryBegin(&freeze));
        assert(xrtOwnershipSnapshotCreate(&root, 1, &root, 1, admit_future, NULL, &graph));
        assert(xrtOwnershipSnapshotNode(graph, 0, &node) && node.Reachable == (choice != 0));
        xrtOwnershipSnapshotDestroy(graph); assert(xrtOwnershipScopeEnd(&freeze)); xlApiUnlock();
        xlDestroyModule(module);
        if(choice) {
            collect(); assert(!atomic_load(&deleted) && !internal_drops && !external_drops);
            if(choice != 4) assert(xrtFutureState(xrtValueGetFuture(alias)) == XFUTURE_RESOLVED);
            xrtValueRelease(alias); alias = NULL;
        }
        finish(); assert(internal_drops == 3 && external_drops == (choice == 5 ? 1u : 0u) && !xrtGetError());
        xrtFree(context.lastError); xrtMemDebugSnapshot(&after);
        assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
        assert(after.AllocCount-before.AllocCount == after.FreeCount-before.FreeCount);
    }
    printf("Future retirement mode=%u: 25 real TCC lifetimes, 75 internal finalizers, independent aliases and zero live delta\n", choice);
    return 0;
}
