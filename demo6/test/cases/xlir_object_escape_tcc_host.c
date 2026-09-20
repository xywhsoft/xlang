/* Preserve the original guarded 10 x 25 population. An additive build with
 * JOINT_ESCAPE_AUTOMATIC executes the SAME population without a host lease.
 * Observe external reachability, not aggregate module lease count: genuine
 * internal code owners remain counted and may form collectible cycles. */
#define JOINT_TCC_ASYNC
#define JOINT_TCC_SOURCE_FILE "test/cases/xlir_object_escape_artifact.c"
#define main unused_async_main
#include "xlir_one_shot_tcc_host.c"
#undef main

typedef struct EscapeHooks {
    void (*capture)(xvalue*);
    void (*dropped)(int64_t);
} EscapeHooks;
static xvalue* escaped;
static unsigned selected, external_drops, internal_drops;
static void capture_escape(xvalue* value)
{
    assert(value && !escaped && module->activeCalls == 1);
    /* Mode 3 drops the outer container before the retirement test: retaining
     * only the envelope must never be mistaken for retaining its extracted child. */
    if (selected == 8) {
        escaped = xrtValueClone(value); /* independent COW shell, shared backing */
        assert(escaped != value);
    } else escaped = xrtValueRetain(selected == 3 || selected == 9 ? xrtValueArrayGet(value, 0) : value);
    assert(escaped && xrtGetError() == NULL);
}
static void drop_escape(int64_t id)
{
    assert(!atomic_load(&deleted));
    if (id == 42) ++external_drops;
    else { assert(id >= 7 && id <= 9); ++internal_drops; }
}
static void call_escape(void)
{
    xrtcallframe frame = {0}; xrtcallresult result = XRT_CALL_RESULT_INIT;
    int64_t value = 0;
    assert(xrtValueIsCallable(escaped) && xrtValueInvoke(escaped, &frame, &result));
    assert(xrtCallResultCount(&result) == 1 && xrtValueGetInt(xrtCallResultGet(&result, 0), &value) && value == (selected == 6 ? 7 : 42));
    xrtCallResultClear(&result); assert(xrtGetError() == NULL);
}
static bool escape_admit(xrtownershipref ref,void* unused)
{ (void)unused; return xlModuleResolveOwnership(ref,NULL)!=NULL; }
static bool escape_external_owner(void)
{
    xrtownershipscope freeze={0}; xrtownershipsnapshot* graph=NULL; xrtownershipnode observed;
    xrtownershipref root={module,&xlModuleOwnershipOps}; bool reachable;
    xlApiLock(); assert(xrtOwnershipFreezeTryBegin(&freeze));
    /* Exactly ONE actual caller handle is an internal slot for this
     * observation of what will remain after xlDestroyModule consumes it. */
    assert(!module->destroyRequested && !module->snapshotRefs);
    assert(xrtOwnershipSnapshotCreate(&root,1,&root,1,escape_admit,NULL,&graph));
    assert(xrtOwnershipSnapshotNode(graph,0,&observed));
    assert(observed.Reference.Data==module && observed.StrongCount==module->codeLeases+1);
    reachable=observed.Reachable;
    xrtOwnershipSnapshotDestroy(graph); assert(xrtOwnershipScopeEnd(&freeze)); xlApiUnlock();
    return reachable;
}
#ifndef XLIR_ESCAPE_NO_MAIN
int main(int argc, char** argv)
{
    unsigned round, missing = 0;
    assert(argc == 2); selected = (unsigned)strtoul(argv[1], NULL, 10); assert(selected < 10);
    expected_cleanups = 0;
    assert(xrtMemDebugEnable(true)); provider_init();
    for (round = 0; round < 25; ++round) {
        typedef void (*Bind)(const EscapeHooks*); Bind bind;
        typedef int64_t (*Run)(int64_t); Run run;
        EscapeHooks hooks = {capture_escape, drop_escape};
        xmemdebugsnapshot before, after;
        bool rooted; int guarded = 0;
        memset(&context, 0, sizeof(context)); escaped = NULL;
        external_drops = internal_drops = 0;
        atomic_store(&deleted, 0); atomic_store(&cleanups, 0);
        xrtClearError(); xrtMemDebugSnapshot(&before); load_artifact();
        { void* symbol = lookup(compiled_state, "escape_bind"); memcpy(&bind, &symbol, sizeof(bind));
          symbol = lookup(compiled_state, "escape_run"); memcpy(&run, &symbol, sizeof(run)); }
        assert(bind && run); bind(&hooks);
        xl__tcc.tcc_get_symbol = lookup; xl__tcc.tcc_delete = delete_state;
        xlApiLock(); module = xlCreateModuleFromState(&context, compiled_state); xlApiUnlock(); assert(module);
        { int activated = xlActivateModule(&context, module, NULL);
          if (!activated) fprintf(stderr, "object module activation failed: %s; XRT: %s\n",
              xlGetLastError(&context), xrtGetError() != NULL ? xrtErrorMessage(xrtGetError()) : "none");
          assert(activated); }
        xlApiLock(); assert(xlModuleBeginCallUnlocked(module)); xlApiUnlock();
        assert(run(selected) == 42 && xrtGetError() == NULL);
        xlApiLock(); xlModuleEndCallUnlocked(module); xlApiUnlock();
        rooted = escape_external_owner();
        assert(!external_drops && !internal_drops && (escaped != NULL) == (selected != 4));
        if (selected < 4 || selected >= 6) {
            if (!rooted) ++missing;
#ifndef JOINT_ESCAPE_AUTOMATIC
            /* Hold globals as well as code; no call/destructor runs after an
             * actual unmap, even while the automatic-ownership gate is red. */
            assert(xlModuleAcquireCode(module)); guarded = 1;
#else
            assert(rooted); /* Never exercise unowned executable memory. */
#endif
            if (selected == 0 || selected == 6) call_escape();
        } else {
            /* Internal-only values must permit retirement; trivial external
             * values must not unnecessarily pin module code. */
            assert(!rooted);
        }
        xlDestroyModule(module); /* consume once */
        if (selected < 4 || selected >= 6) {
            collect(); assert(!atomic_load(&deleted) && !internal_drops);
            if (selected == 0 || selected == 6) call_escape();
            xrtValueRelease(escaped); escaped = NULL;
            /* Shared globals must remain live after the host drops its alias;
             * they are not separate external-only objects. */
            if (guarded) {
                assert(external_drops == (selected < 4 ? 1u : 0u) && !internal_drops && xrtGetError() == NULL);
                xlModuleReleaseCode(module); /* may free module; no further handle access */
            }
        }
        finish(); assert(internal_drops == 3);
        assert(external_drops == (selected < 4 ? 1u : 0u));
        if (selected == 5) {
            int64_t value = 0; assert(xrtValueGetInt(escaped, &value) && value == 42);
            xrtValueRelease(escaped); escaped = NULL;
        }
        assert(!escaped && xrtGetError() == NULL);
        xrtFree(context.lastError); xrtMemDebugSnapshot(&after);
        assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
        assert(after.AllocCount-before.AllocCount == after.FreeCount-before.FreeCount);
    }
    printf("object escape mode=%u: transactions=25 automatic_owner_missing=%u; external reachability, real TCC page deletion and balanced memory\n", selected, missing);
    /* Known architectural failures stay red, not converted into passing XFAILs. */
    return missing ? 2 : 0;
}
#endif
