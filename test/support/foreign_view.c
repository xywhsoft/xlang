#include "src/runtime/xl6_foreign_view.h"
#include "src/runtime/xl6_ownership_plan.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct Watch {
    void (*invalidate)(void*);
    void* user;
    struct Watch** slot;
} Watch;
typedef struct Native Native;
struct Native {
    Watch* watch;
    Native* root;
    Native* children;
    Native* next;
    XL6_ForeignView* capture;
    unsigned depth, failures;
    bool closing, failWithoutDiagnostic, failEnd;
};
typedef struct NativeScope { struct NativeScope* self; Native* root; } NativeScope;
static unsigned nativeDestroyed, childDestroyed;
static unsigned codeReferences=1;
static int codeAcquire(void* data) { (void)data; ++codeReferences; return 1; }
static void codeRelease(void* data) { (void)data; assert(codeReferences>1); --codeReferences; }
static const XL6_CodeOwner codeOwner={sizeof(codeOwner),NULL,codeAcquire,codeRelease};

static Native* native_create(void)
{ Native* n=xrtCalloc(1,sizeof(*n)); assert(n); n->root=n; return n; }
static Native* native_child(Native* root)
{
    Native* n=xrtCalloc(1,sizeof(*n)); assert(n);
    n->root=root; n->next=root->children; root->children=n; return n;
}
static bool watch(void* raw,void* storage,void (*invalidate)(void*),void* user)
{
    Native* n=raw; Watch* w=storage;
    if (n->closing || n->watch || w->slot || w->invalidate || w->user) return false;
    w->slot=&n->watch; w->invalidate=invalidate; w->user=user; n->watch=w; return true;
}
static void unwatch(void* storage)
{
    Watch* w=storage;
    if (w->slot) *w->slot=NULL;
    *w=(Watch){0};
}
static void invalidate_native(Native* n)
{
    if (n->closing) return;
    n->closing=true;
    if (n->watch) {
        Watch* w=n->watch; void (*fn)(void*)=w->invalidate; void* user=w->user;
        unwatch(w); fn(user);
    }
}
static void flush_native(Native* root)
{
    if (root->depth) return;
    Native** slot=&root->children;
    while (*slot) {
        Native* n=*slot;
        if (root->closing || n->closing) { *slot=n->next; ++childDestroyed; xrtFree(n); }
        else slot=&n->next;
    }
    if (root->closing) {
        XL6_ForeignView* capture=root->capture;
        root->capture=NULL; ++nativeDestroyed; xrtFree(root);
        xl6ForeignViewReleaseV1(capture);
    }
}
static bool scope_begin(void* raw,void* storage)
{
    Native* root=raw; NativeScope* s=storage;
    if (root->closing || s->self || s->root) return false;
    s->self=s; s->root=root; ++root->depth; return true;
}
static bool scope_end(void* storage)
{
    NativeScope* s=storage;
    assert(s->self==s && s->root && s->root->depth);
    Native* root=s->root; bool fail=root->failEnd; root->failEnd=false;
    *s=(NativeScope){0}; --root->depth; flush_native(root);
    if (fail) { xrtSetErrorKind(XERR_RANGE); return false; }
    return true;
}
static bool destroy_native(void* raw)
{
    Native* n=raw; Native* root=n->root;
    if (n->failures) {
        --n->failures;
        if (!n->failWithoutDiagnostic) xrtSetErrorKind(XERR_VALUE);
        return false;
    }
    invalidate_native(n);
    if (n==root) for (Native* child=root->children; child; child=child->next) invalidate_native(child);
    flush_native(root); return true;
}
static bool trace_native(const void* raw,xrtownershipvisitor visit,void* context)
{
    const Native* n=raw;
    return !n->capture || visit(xl6ForeignViewOwnershipV1(n->capture),context);
}
static const XL6_ForeignViewDescriptorV1 rootDescriptor={
    sizeof(rootDescriptor),XL6_FOREIGN_VIEW_V1_TAG,"native.test.root",
    sizeof(Watch),_Alignof(Watch),watch,unwatch,destroy_native,
    sizeof(NativeScope),_Alignof(NativeScope),scope_begin,scope_end,trace_native};
static const XL6_ForeignViewDescriptorV1 childDescriptor={
    sizeof(childDescriptor),XL6_FOREIGN_VIEW_V1_TAG,"native.test.child",
    sizeof(Watch),_Alignof(Watch),watch,unwatch,destroy_native,0,1,NULL,NULL,NULL};

static void views_and_scopes(void)
{
    Native* raw=native_create(); Native* child=native_child(raw);
    XL6_ForeignView* root=xl6ForeignViewCreateRootV1(raw,&rootDescriptor,&codeOwner); assert(root);
    assert(codeReferences==2);
    NativeScope native={0}; XL6_ForeignViewCallV1 call={0};
    assert(xl6ForeignViewCallBeginV1(root,&call,&native,sizeof(native)) && call.raw==raw);
    XL6_ForeignView* a=xl6ForeignViewBorrowChildV1(child,root,&childDescriptor,&codeOwner); assert(a);
    xmemdebugsnapshot before,after; xrtMemDebugSnapshot(&before);
    assert(xrtMemDebugFailAfter(0));
    XL6_ForeignView* b=xl6ForeignViewBorrowChildV1(child,root,&childDescriptor,&codeOwner); assert(a==b);
    assert(xl6ForeignViewRetainV1(a)==a); xl6ForeignViewReleaseV1(a);
    xl6ForeignViewReleaseV1(b);
    assert(xrtMemDebugFailAfter(-1)); xrtMemDebugSnapshot(&after);
    assert(before.AllocCount==after.AllocCount && before.FreeCount==after.FreeCount && before.ReallocCount==after.ReallocCount);
    XL6_ForeignViewCallV1 copied=call;
    assert(!xl6ForeignViewCallEndV1(&copied)); xrtClearError();
    assert(xl6ForeignViewCallEndV1(&call));
    assert(xl6ForeignViewCallEndV1(&call));
    assert(xl6ForeignViewCallBeginV1(a,&call,&native,sizeof(native)));
    unsigned destroyed=childDestroyed;
    assert(xl6ForeignViewCloseV1(a));
    assert(childDestroyed==destroyed && child->closing);
    assert(xl6ForeignViewCallEndV1(&call) && childDestroyed==destroyed+1);
    assert(!xl6ForeignViewCallBeginV1(a,&call,&native,sizeof(native))); xrtClearError();
    xl6ForeignViewReleaseV1(a); xl6ForeignViewReleaseV1(root);
    assert(codeReferences==1 && xl6ForeignViewPendingV1(0)==0);
}
static void native_stack_residency(void)
{
    Native* raw=native_create();
    XL6_ForeignView* root=xl6ForeignViewCreateRootV1(raw,&rootDescriptor,&codeOwner); assert(root);
    raw->capture=xl6ForeignViewRetainV1(root); assert(raw->capture);
    xl6ForeignViewReleaseV1(root);
    XL6_ForeignCallbackScopeV1 callback={0},nested={0};
    assert(xl6ForeignCallbackEnterV1(&callback));
    assert(xl6ForeignCallbackEnterV1(&nested));
    assert(!xl6ForeignCallbackLeaveV1(&callback)); xrtClearError();
    assert(xl6ForeignCallbackLeaveV1(&nested));
    assert(destroy_native(raw));
    assert(codeReferences==2 && xl6ForeignViewPendingV1(0)==1);
    assert(xl6ForeignViewDrainV1() && codeReferences==2);
    assert(xl6ForeignCallbackLeaveV1(&callback));
    assert(codeReferences==2); /* Still no drop on the native callback tail. */
    assert(xl6ForeignViewDrainV1() && codeReferences==1 && xl6ForeignViewPendingV1(0)==0);
}
static int32 drop_on_worker(void* raw)
{
    XL6_ForeignView* view=raw;
    NativeScope native={0}; XL6_ForeignViewCallV1 call={0};
    assert(!xl6ForeignViewCallBeginV1(view,&call,&native,sizeof(native))); xrtClearError();
    assert(!xl6ForeignViewCloseV1(view)); xrtClearError();
    xl6ForeignViewReleaseV1(view);
    assert(xl6ForeignViewPendingV1(0)==1);
    return 0;
}
static void affinity(void)
{
    Native* raw=native_create();
    XL6_ForeignView* root=xl6ForeignViewCreateRootV1(raw,&rootDescriptor,&codeOwner); assert(root);
    unsigned before=nativeDestroyed;
    xthread* thread=xrtThreadCreate(drop_on_worker,root,0); assert(thread);
    assert(xrtThreadWait(thread)==XWAIT_OK); xrtThreadDestroy(thread);
    assert(nativeDestroyed==before && codeReferences==2);
    assert(xl6ForeignViewPendingV1(xrtThreadCurrentId())==1);
    assert(xl6ForeignViewDrainV1() && nativeDestroyed==before+1 && codeReferences==1);
}
static void checked_retry(void)
{
    Native* raw=native_create();
    XL6_ForeignView* root=xl6ForeignViewCreateRootV1(raw,&rootDescriptor,&codeOwner); assert(root);
    raw->failures=1;
    assert(!xl6ForeignViewCloseV1(root) && raw->watch && !raw->closing);
    assert(xrtErrorKind(xrtGetError())==XERR_VALUE); xrtClearError();
    assert(xl6ForeignViewPendingV1(0)==1 && xl6ForeignViewDrainV1());
    xl6ForeignViewReleaseV1(root); assert(codeReferences==1);
    raw=native_create(); root=xl6ForeignViewCreateRootV1(raw,&rootDescriptor,&codeOwner); assert(root);
    raw->failures=1; raw->failWithoutDiagnostic=true; raw->failEnd=true;
    assert(!xl6ForeignViewCloseV1(root));
    assert(!xrtGetError()); /* First NULL-diagnostic failure beats later RangeError. */
    assert(xl6ForeignViewDrainV1()); xl6ForeignViewReleaseV1(root);
    assert(codeReferences==1);
    raw=native_create(); root=xl6ForeignViewCreateRootV1(raw,&rootDescriptor,&codeOwner); assert(root);
    raw->failures=1; raw->failWithoutDiagnostic=true;
    assert(!xl6ForeignViewReleaseV1(root) && !xrtGetError());
    assert(codeReferences==2 && xl6ForeignViewPendingV1(0)==1);
    assert(xl6ForeignViewDrainV1() && codeReferences==1);
}
static bool discover_views(XL6_ModuleOwnershipVisit visit,void* context,void* catalog)
{ (void)catalog; return xl6ForeignViewDiscoverV1(NULL,visit,context); }
static void graph_cycle_case(bool child_capture)
{
    Native* raw=native_create();
    XL6_ForeignView* root=xl6ForeignViewCreateRootV1(raw,&rootDescriptor,NULL); assert(root);
    if (child_capture) {
        Native* child=native_child(raw);
        NativeScope native={0}; XL6_ForeignViewCallV1 call={0};
        assert(xl6ForeignViewCallBeginV1(root,&call,&native,sizeof(native)));
        XL6_ForeignView* view=xl6ForeignViewBorrowChildV1(child,root,&childDescriptor,NULL); assert(view);
        raw->capture=xl6ForeignViewRetainV1(view); assert(raw->capture);
        assert(xl6ForeignViewCallEndV1(&call)); assert(xl6ForeignViewReleaseV1(view));
    } else { raw->capture=xl6ForeignViewRetainV1(root); assert(raw->capture); }
    xl6ForeignViewReleaseV1(root); /* Only the real binding and captured self remain. */
    const XL6_OwnershipDomainV1* domains[]={xl6OwnershipDomainForeignViewV1Get()};
    XL6_OwnershipCatalog* catalog=xl6OwnershipCatalogCreateV1(domains,1); assert(catalog);
    XL6_OwnershipPlan* plan=NULL;
    XL6_OwnershipPlanResult ready=xl6OwnershipPlanTryCreateV6(xl6ForeignViewOwnershipV1(root),
        xl6OwnershipCatalogProbeV1,discover_views,true,catalog,&plan);
    if (ready<=0 || !plan) fprintf(stderr,"view graph admission: result=%d error=%d\n",ready,xrtErrorKind(xrtGetError()));
    assert(ready>0 && plan);
    assert(xl6OwnershipPlanPrepare(plan)==XL6_OWNERSHIP_STEP_READY);
    XL6_OwnershipStepResult finalized=XL6_OWNERSHIP_STEP_BUSY;
    for (unsigned attempt=0;attempt<8 && finalized==XL6_OWNERSHIP_STEP_BUSY;++attempt)
        finalized=xl6OwnershipPlanFinalizeV2(plan);
    assert(finalized==XL6_OWNERSHIP_STEP_READY);
    assert(xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_READY);
    assert(xl6OwnershipPlanFinishV2(plan)==XL6_OWNERSHIP_STEP_READY);
    assert(xl6OwnershipPlanTryDestroy(plan)>0);
    xl6OwnershipCatalogDestroyV1(catalog);
    assert(xl6ForeignViewPendingV1(0)==0);
}
static void graph_cycle(void)
{ graph_cycle_case(false); graph_cycle_case(true); }
static void allocation_rollback(void)
{
    unsigned failures=0;
    for (int64 fail=0;fail<12;++fail) {
        Native* raw=native_create();
        xmemdebugsnapshot before,after; xrtMemDebugSnapshot(&before);
        assert(xrtMemDebugFailAfter(fail));
        XL6_ForeignView* root=xl6ForeignViewCreateRootV1(raw,&rootDescriptor,&codeOwner);
        assert(xrtMemDebugFailAfter(-1)); xrtClearError();
        if (root) { xl6ForeignViewReleaseV1(root); break; }
        ++failures; assert(!raw->watch && codeReferences==1);
        xrtMemDebugSnapshot(&after);
        assert(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes);
        assert(destroy_native(raw));
    }
    assert(failures>=2);
    for (int64 fail=0;fail<16;++fail) {
        Native* raw=native_create(); Native* child=native_child(raw);
        XL6_ForeignView* root=xl6ForeignViewCreateRootV1(raw,&rootDescriptor,&codeOwner); assert(root);
        NativeScope native={0}; XL6_ForeignViewCallV1 call={0};
        assert(xl6ForeignViewCallBeginV1(root,&call,&native,sizeof(native)));
        xmemdebugsnapshot before,after; xrtMemDebugSnapshot(&before);
        assert(xrtMemDebugFailAfter(fail));
        XL6_ForeignView* view=xl6ForeignViewBorrowChildV1(child,root,&childDescriptor,&codeOwner);
        assert(xrtMemDebugFailAfter(-1)); xrtClearError();
        if (view) xl6ForeignViewReleaseV1(view);
        else {
            assert(!child->watch && codeReferences==2);
            xrtMemDebugSnapshot(&after);
            assert(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes);
        }
        assert(xl6ForeignViewCallEndV1(&call)); xl6ForeignViewReleaseV1(root);
        if (view) break;
        ++failures;
    }
    printf("foreign view allocation rollback: %u failure prefixes\n",failures);
}
static void exclusive_root(void)
{
    XL6_ForeignViewDescriptorV1 d=rootDescriptor;
    d.watchBytes=0; d.watchAlignment=1; d.Watch=NULL; d.Unwatch=NULL;
    d.callBytes=0; d.callAlignment=1; d.CallBegin=NULL; d.CallEnd=NULL;
    Native* raw=native_create();
    XL6_ForeignView* root=xl6ForeignViewCreateRootV1(raw,&d,&codeOwner); assert(root);
    XL6_ForeignViewCallV1 call={0};
    assert(xl6ForeignViewCallBeginV1(root,&call,NULL,0));
    unsigned before=nativeDestroyed;
    assert(xl6ForeignViewCloseV1(root) && nativeDestroyed==before);
    assert(xl6ForeignViewCallEndV1(&call) && nativeDestroyed==before+1);
    xl6ForeignViewReleaseV1(root); assert(codeReferences==1);
}
static void metadata_and_admission(void)
{
    for (unsigned fault=0;fault<5;++fault) {
        Native* raw=native_create(); XL6_ForeignViewDescriptorV1 d=rootDescriptor;
        if (fault==0) d.size--;
        if (fault==1) d.abiTag=0;
        if (fault==2) d.watchAlignment=3;
        if (fault==3) d.Unwatch=NULL;
        if (fault==4) { d.CallBegin=NULL; d.CallEnd=NULL; d.callBytes=0; d.callAlignment=1; }
        assert(!xl6ForeignViewCreateRootV1(raw,&d,&codeOwner));
        assert(!raw->watch && codeReferences==1); xrtClearError(); assert(destroy_native(raw));
    }
    Native* raw=native_create(); Native* child=native_child(raw);
    XL6_ForeignView* root=xl6ForeignViewCreateRootV1(raw,&rootDescriptor,&codeOwner); assert(root);
    assert(!xl6ForeignViewBorrowChildV1(child,root,&childDescriptor,&codeOwner)); xrtClearError();
    NativeScope native={0}; XL6_ForeignViewCallV1 call={0};
    assert(!xl6ForeignViewCallBeginV1(root,&call,&native,sizeof(native)-1)); xrtClearError();
    assert(xl6ForeignViewCallBeginV1(root,&call,&native,sizeof(native)));
    XL6_ForeignView* view=xl6ForeignViewBorrowChildV1(child,root,&childDescriptor,&codeOwner); assert(view);
    XL6_ForeignViewDescriptorV1 other=childDescriptor; other.family="wrong.family";
    assert(!xl6ForeignViewBorrowChildV1(child,root,&other,&codeOwner)); xrtClearError();
    const XL6_OwnershipDomainV1* domains[]={xl6OwnershipDomainForeignViewV1Get()};
    XL6_OwnershipCatalog* catalog=xl6OwnershipCatalogCreateV1(domains,1); assert(catalog);
    const xrtownershipadapterv1* adapter=NULL; const xrtownershippreparationv1* prepare=NULL;
    const XL6_OwnershipFinishV1* finish=NULL;
    assert(xl6OwnershipCatalogProbeV1(xl6ForeignViewOwnershipV1(root),catalog,&adapter,&prepare,&finish)==XL6_OWNERSHIP_ADMISSION_BUSY);
    assert(!adapter && !prepare && !finish);
    xl6OwnershipCatalogDestroyV1(catalog);
    assert(xl6ForeignViewCallEndV1(&call)); xl6ForeignViewReleaseV1(view); xl6ForeignViewReleaseV1(root);
    assert(codeReferences==1);
}
int main(int argc,char** argv)
{
    assert(xrtMemDebugEnable(true));
    xmemdebugsnapshot before,after; xrtMemDebugSnapshot(&before);
    const struct { const char* name; void (*run)(void); } cases[]={
        {"views",views_and_scopes},{"residency",native_stack_residency},{"affinity",affinity},
        {"retry",checked_retry},{"graph",graph_cycle},{"allocation",allocation_rollback},
        {"exclusive",exclusive_root},{"admission",metadata_and_admission}};
    bool found=argc==1;
    for (size_t i=0;i<sizeof(cases)/sizeof(cases[0]);++i)
        if (argc==1 || !strcmp(argv[1],cases[i].name)) { found=true; cases[i].run(); }
    assert(found && argc<=2);
    xrtClearError(); xrtMemDebugSnapshot(&after);
    assert(codeReferences==1 && xl6ForeignViewPendingV1(0)==0);
    assert(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes);
    assert(before.InvalidFreeCount==after.InvalidFreeCount && before.DoubleFreeCount==after.DoubleFreeCount);
    puts("foreign view: identity, invalidation, native-stack residency, affinity, retry, graph and rollback passed");
    return 0;
}
