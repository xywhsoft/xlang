#define JOINT_TCC_ASYNC
#define JOINT_TCC_SOURCE_FILE "test/cases/stdlib_stack_ownership_artifact.c"
#define JOINT_TCC_EXTRA_SYMBOLS(Bind) \
    Bind(xl6StackCreateV1); Bind(xl6StackPushV1); Bind(xl6StackPopOrPeekV1); \
    Bind(xl6StackPeekAtV1); Bind(xl6StackReserveV1); Bind(xl6StackCloneV1); \
    Bind(xl6StackEqualsV1); Bind(xl6StackTrimV1); Bind(xl6StackClearV1); Bind(xl6StackSizeV1);
#define main unused_stack_lifetime_main
#include "xlir_one_shot_tcc_host.c"
#undef main
typedef struct StackGraphHooks {
    void (*capture)(xvalue*);
    void (*dropped)(int64_t,int64_t);
} StackGraphHooks;
static xvalue* escaped;
static unsigned selected, droppedMarkers;
static void captureStack(xvalue* value)
{
    assert(value && !escaped && module->activeCalls == 1);
    escaped = selected == 3 ? xrtValueDeepClone(value) : xrtValueRetain(value);
    assert(escaped && (selected != 3 || escaped != value) && !xrtGetError());
}
static void droppedStack(int64_t id, int64_t count)
{
    assert(!atomic_load(&deleted));
    assert((id == 7 && count == 1) || (id == 42 && count == 2) || (id == 9 && count == 0));
    /* Semantic destructor has intact fields and runs outside any freeze. */
    xrtownershipscope freeze = {0};
    assert(xrtOwnershipFreezeTryBegin(&freeze) && xrtOwnershipScopeEnd(&freeze));
    ++droppedMarkers;
}
static bool admitStack(xrtownershipref ref, void* unused)
{ (void)unused; return xlModuleResolveOwnership(ref,NULL) != NULL; }
static bool externallyRooted(void)
{
    xrtownershipscope freeze = {0}; xrtownershipsnapshot* graph = NULL; xrtownershipnode observed;
    xrtownershipref root = {module,&xlModuleOwnershipOps}; bool rooted;
    xlApiLock(); assert(xrtOwnershipFreezeTryBegin(&freeze));
    assert(!module->destroyRequested && !module->snapshotRefs);
    assert(xrtOwnershipSnapshotCreate(&root,1,&root,1,admitStack,NULL,&graph));
    assert(xrtOwnershipSnapshotNode(graph,0,&observed)); rooted = observed.Reachable;
    xrtOwnershipSnapshotDestroy(graph); assert(xrtOwnershipScopeEnd(&freeze)); xlApiUnlock(); return rooted;
}
int main(int argc, char** argv)
{
    assert(argc == 2); selected = (unsigned)strtoul(argv[1],NULL,10); assert(selected < 4);
    assert(xrtMemDebugEnable(true)); expected_cleanups = 0; provider_init();
    for (unsigned round = 0; round < 25; ++round) {
        xmemdebugsnapshot before, after; xrtMemDebugSnapshot(&before);
        memset(&context,0,sizeof(context)); escaped = NULL; droppedMarkers = 0;
        atomic_store(&deleted,0); atomic_store(&cleanups,0); xrtClearError(); load_artifact();
        typedef void (*Bind)(const StackGraphHooks*); typedef int64_t (*Setup)(int64_t);
        Bind bind; Setup setup; void* symbol = lookup(compiled_state,"stack_graph_bind"); memcpy(&bind,&symbol,sizeof(bind));
        symbol = lookup(compiled_state,"stack_graph_setup"); memcpy(&setup,&symbol,sizeof(setup)); assert(bind && setup);
        StackGraphHooks hooks = {captureStack,droppedStack}; bind(&hooks);
        xl__tcc.tcc_get_symbol = lookup; xl__tcc.tcc_delete = delete_state;
        xlApiLock(); module = xlCreateModuleFromState(&context,compiled_state); xlApiUnlock(); assert(module);
        assert(xlActivateModule(&context,module,NULL));
        xlApiLock(); assert(xlModuleBeginCallUnlocked(module)); xlApiUnlock();
        assert(setup(selected) == 42 && !xrtGetError());
        xlApiLock(); xlModuleEndCallUnlocked(module); xlApiUnlock();
        assert(externallyRooted() == (selected != 0) && !droppedMarkers);
        xlDestroyModule(module);
        if (selected) {
            assert(!atomic_load(&deleted) && !droppedMarkers);
            xrtValueRelease(escaped); escaped = NULL;
        }
        xdeadline deadline = xrtDeadlineAfter(5000000);
        while (!atomic_load(&deleted)) {
            collect();
            if (xrtDeadlineExpired(deadline)) {
                fprintf(stderr,"stack retirement mode=%u round=%u drops=%u phase=%u leases=%zu snapshots=%zu failed=%u plan=%p error=%s\n",
                    selected,round,droppedMarkers,module->ownershipPhase,module->codeLeases,module->snapshotRefs,
                    module->retirementFailed,(void*)module->ownershipPlan,
                    module->retirementError ? xrtErrorMessage(module->retirementError) : "none");
                xrtownershipscope freeze = {0}; xrtownershipsnapshot* snapshot = NULL;
                xrtownershipref root = {module,&xlModuleOwnershipOps};
                assert(xrtOwnershipFreezeTryBegin(&freeze));
                bool ok = xrtOwnershipSnapshotCreate(&root,1,NULL,0,admitStack,NULL,&snapshot);
                fprintf(stderr,"retirement snapshot ok=%d kind=%d\n",ok,xrtErrorKind(xrtGetError()));
                if (ok) for (size_t index=0; index<xrtOwnershipSnapshotNodeCount(snapshot); ++index) {
                    xrtownershipnode node; assert(xrtOwnershipSnapshotNode(snapshot,index,&node));
                    fprintf(stderr,"node %zu data=%p ops=%p strong=%zu internal=%zu reachable=%d\n",index,
                        node.Reference.Data,(void*)node.Reference.Ops,node.StrongCount,node.InternalCount,node.Reachable);
                }
                xrtOwnershipSnapshotDestroy(snapshot); assert(xrtOwnershipScopeEnd(&freeze)); abort();
            }
            xrtThreadYield();
        }
        assert(xl__retiringModules == NULL && droppedMarkers == (selected ? 3u : 1u) && !xrtGetError());
        xrtFree(context.lastError); xrtMemDebugSnapshot(&after);
        assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes &&
            after.AllocCount-before.AllocCount == after.FreeCount-before.FreeCount &&
            before.InvalidFreeCount == after.InvalidFreeCount && before.DoubleFreeCount == after.DoubleFreeCount);
    }
    printf("stack module graph mode=%u: 25 actual TCC lifetimes, intact-field once-only finalizers, real external roots and MEM_FREE; balanced\n",selected);
    return 0;
}
