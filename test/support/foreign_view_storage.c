#include "foreign_view_native_fixture.h"
#include "src/runtime/xl6_cell_native_graph.h"
#include "src/runtime/xl6_native_graph_future_payload.h"
#include "src/runtime/xl6_foreign_view_capture.h"
#include "src/runtime/xl6_capture_v3.h"
#include "src/runtime/xl6_closure_capture_v4.h"
#include "src/runtime/xl6_ownership_domains.h"
#include "src/api/xl_foreign_error.h"

static const XL6_NativeGraphCaptureProfileV1* view_profile(void)
{ return xl6NativeGraphCaptureProfileV1Get(xl6ForeignViewCaptureProfileV1Get()); }
static unsigned visits;
static bool count_ref(xrtownershipref ref,void* context)
{ (void)context;assert(ref.Data && ref.Ops);++visits;return true; }
static void check_empty_errors(void) { assert(!xrtGetError()); }
static XL6_ForeignView* new_view(Native** raw)
{
    *raw=native_create();native_child(*raw);
    XL6_ForeignView* view=xl6ForeignViewCreateRootV1(*raw,&rootDescriptor,NULL);assert(view);return view;
}
static void cells(void)
{
    const XL6_CellRuntimeV1* api=xl6CellRuntimeV1Get();
    Native* raw;XL6_ForeignView* view=new_view(&raw);
    XL6_Cell* cell=xl6CellCreateNativeGraphV3(&view_profile()->prefix);assert(cell);
    xrtownershipref ref=api->Ownership(cell);visits=0;
    assert(ref.Ops->Trace(ref.Data,count_ref,NULL) && !visits); /* zero storage */
    XL6_ForeignView* owner=NULL;assert(view_profile()->CopyFresh(&owner,&view));
    api->Initialize(cell,&owner,sizeof(owner));owner=NULL;
    visits=0;assert(ref.Ops->Trace(ref.Data,count_ref,NULL) && visits==1);
    XL6_ForeignView* loaded=NULL;api->Load(cell,&loaded,sizeof(loaded));assert(loaded==view);
    XL6_ForeignView* empty=NULL;XL6_ForeignView* previous=NULL;
    xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);
    api->Exchange(cell,&empty,&previous,sizeof(previous));assert(previous==view);
    api->Exchange(cell,&previous,&empty,sizeof(empty));previous=NULL;assert(!empty);
    api->Retain(cell);api->Release(cell);api->Load(cell,&loaded,sizeof(loaded));assert(loaded==view);
    xrtMemDebugSnapshot(&after);
    assert(before.AllocCount==after.AllocCount && before.FreeCount==after.FreeCount && before.ReallocCount==after.ReallocCount);
    assert(xl6ForeignViewReleaseV1(view));api->Release(cell);check_empty_errors();
}
static bool discover(XL6_ModuleOwnershipVisit visit,void* context,void* catalog)
{ (void)catalog;return xl6ForeignViewDiscoverV1(NULL,visit,context); }
static void collect(XL6_ForeignView* view)
{
    const XL6_OwnershipDomainV1* domains[]={xl6OwnershipDomainForeignViewV1Get(),
        xl6OwnershipDomainCellV1Get(),xl6OwnershipDomainClosureV1Get(),
        xl6OwnershipDomainNativeFutureV1Get(),xl6OwnershipDomainNativeGraphFuturePayloadV1Get()};
    XL6_OwnershipCatalog* catalog=xl6OwnershipCatalogCreateV1(domains,sizeof(domains)/sizeof(domains[0]));assert(catalog);
    XL6_OwnershipPlan* plan=NULL;
    assert(xl6OwnershipPlanTryCreateV6(xl6ForeignViewOwnershipV1(view),xl6OwnershipCatalogProbeV1,discover,true,catalog,&plan)>0 && plan);
    XL6_OwnershipStepResult step=XL6_OWNERSHIP_STEP_BUSY;
    for(unsigned i=0;i<8 && step==XL6_OWNERSHIP_STEP_BUSY;++i)step=xl6OwnershipPlanPrepare(plan);
    assert(step==XL6_OWNERSHIP_STEP_READY);step=XL6_OWNERSHIP_STEP_BUSY;
    for(unsigned i=0;i<8 && step==XL6_OWNERSHIP_STEP_BUSY;++i)step=xl6OwnershipPlanFinalizeV2(plan);
    assert(step==XL6_OWNERSHIP_STEP_READY);
    assert(xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_READY);
    assert(xl6OwnershipPlanFinishV2(plan)==XL6_OWNERSHIP_STEP_READY);
    assert(xl6OwnershipPlanTryDestroy(plan)>0);xl6OwnershipCatalogDestroyV1(catalog);
    assert(xl6ForeignViewDrainV1() && xl6ForeignViewPendingV1(0)==0);
}
static void cell_cycle(void)
{
    Native* raw;XL6_ForeignView* view=new_view(&raw);
    XL6_Cell* cell=xl6CellCreateNativeGraphV3(&view_profile()->prefix);assert(cell);
    /* Capture the cell using the existing cell kind, not a new boxed carrier. */
    XL6_Cell** environment=xrtCalloc(1,sizeof(*environment));assert(environment);
    *environment=xl6CellRuntimeV1Get()->Retain(cell);
    static const XL6_CaptureSlotV2 slot={ {XL6_CAPTURE_CELL,0,sizeof(XL6_Cell*)},NULL };
    static const XL6_CaptureLayoutV2 layout={sizeof(layout),sizeof(XL6_Cell*),1,&slot};
    XL6_Closure* closure=xl6ClosureRuntimeV4Get()->Create(NULL,(void(*)(void))check_empty_errors,NULL,
        environment,sizeof(*environment),&layout,NULL,NULL,0);assert(closure);
    raw->callback=closure;
    xl6CellRuntimeV1Get()->Initialize(cell,&view,sizeof(view)); /* consumes view */
    xl6CellRuntimeV1Get()->Release(cell);
    collect(view);check_empty_errors();
}
static void futures(void)
{
    const XL6_NativeFuturePayloadRuntimeV1* api=xl6NativeGraphFuturePayloadRuntimeV1Get();
    Native* raw;XL6_ForeignView* view=new_view(&raw);
    xfuture* future=NULL;xpromise* promise=xrtPromiseCreate(&future,NULL);assert(promise && future);
    void* data=api->Allocate(&view_profile()->prefix);assert(data);
    assert(view_profile()->CopyFresh(data,&view));assert(api->ResolveOwned(promise,data));
    assert(xrtFutureValue(future)==data);
    XL6_ForeignView* copy=NULL;
    xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);
    assert(api->Copy(data,&view_profile()->prefix,&copy) && copy==view);
    xrtMemDebugSnapshot(&after);
    assert(before.AllocCount==after.AllocCount && before.FreeCount==after.FreeCount && before.ReallocCount==after.ReallocCount);
    XL6_NativeGraphCaptureProfileV1 other=*view_profile();++other.prefix.identity;
    XL6_ForeignView* rejected=NULL;
    assert(!api->Copy(data,&other.prefix,&rejected) && !rejected);xrtClearError();
    assert(!xl6NativeFuturePayloadRuntimeV1Get()->Copy(data,&view_profile()->prefix,&rejected) && !rejected);xrtClearError();
    assert(!api->Copy(data,&view_profile()->prefix,data));xrtClearError();
    void* unconsumed=api->Allocate(&view_profile()->prefix);assert(unconsumed);
    assert(view_profile()->CopyFresh(unconsumed,&view));
    assert(!api->ResolveOwned(promise,unconsumed));xrtClearError();api->Drop(unconsumed,NULL);
    xrtPromiseDestroy(promise);xrtFutureDestroy(future);assert(xl6ForeignViewDrainV1());
    assert(xl6ForeignViewReleaseV1(view));assert(xl6ForeignViewReleaseV1(copy));check_empty_errors();
}
static void future_cycle(void)
{
    const XL6_NativeFuturePayloadRuntimeV1* api=xl6NativeGraphFuturePayloadRuntimeV1Get();
    Native* raw;XL6_ForeignView* view=new_view(&raw);
    xpromise* promise=xrtPromiseCreate(&raw->future,NULL);assert(promise && raw->future);
    void* data=api->Allocate(&view_profile()->prefix);assert(data);
    *(XL6_ForeignView**)data=view; /* move the only language owner */
    assert(api->ResolveOwned(promise,data));xrtPromiseDestroy(promise);
    collect(view);check_empty_errors();
}
typedef struct Marker { int id;bool fail,diagnostic; } Marker;
static unsigned drops;static bool copyError;
static bool marker_copy(void* output,const void* input)
{ *(Marker*)output=*(const Marker*)input;if(copyError)xrtSetErrorKind(XERR_RANGE);return true; }
static bool marker_trace(const void* value,XL6_ModuleOwnershipVisit visit,void* context)
{ (void)value;(void)context;return visit!=NULL; }
static bool marker_drop(void* raw,unsigned char* live)
{
    Marker value=*(Marker*)raw;*(Marker*)raw=(Marker){0};if(live)*live=0;
    if(value.id)++drops;
    if(value.diagnostic)xrtSetErrorKind(XERR_RANGE);
    return !value.fail;
}
static const XL6_NativeCaptureProfileV1* marker_profile(void)
{
    static const XL6_NativeGraphCaptureProfileV1 p={ {sizeof(p),UINT64_C(0x53544F5241474531),NULL,NULL},
        XL6_NATIVE_GRAPH_CAPTURE_V1_TAG,sizeof(Marker),_Alignof(Marker),marker_copy,marker_trace,marker_drop};
    return &p.prefix;
}
static void checked_cleanup(void)
{
    for(unsigned diagnostic=0;diagnostic<2;++diagnostic){
        XL6_Cell* cell=xl6CellCreateNativeGraphV3(marker_profile());assert(cell);
        Marker value={1,true,diagnostic!=0};xl6CellRuntimeV1Get()->Initialize(cell,&value,sizeof(value));
        xl6_error_scope scope={0};assert(xl6ErrorScopeBegin(&scope));drops=0;
        xl6CellRuntimeV1Get()->Release(cell);xl6_error_result result=xl6ErrorScopeEnd(&scope);
        assert(!result.ok && (!!result.error)==(diagnostic!=0) && drops==1);xrtErrorFree(result.error);
        const XL6_NativeFuturePayloadRuntimeV1* api=xl6NativeGraphFuturePayloadRuntimeV1Get();
        Marker* payload=api->Allocate(marker_profile());assert(payload);*payload=value;
        assert(xl6ErrorScopeBegin(&scope));drops=0;api->Drop(payload,NULL);result=xl6ErrorScopeEnd(&scope);
        assert(!result.ok && (!!result.error)==(diagnostic!=0) && drops==1 && !xl6ForeignCallbackActiveV1());xrtErrorFree(result.error);
    }
    /* An escaping failure with no diagnostic remains primary across a
     * destructive cleanup that has its own allocated error. */
    xl6_error_scope incoming={0};assert(xl6ErrorScopeBegin(&incoming));xl6ErrorScopeCaptureFailure(&incoming);
    Marker failure={1,true,true};
    XL6_Cell* failedCell=xl6CellCreateNativeGraphV3(marker_profile());assert(failedCell);
    xl6CellRuntimeV1Get()->Initialize(failedCell,&failure,sizeof(failure));
    xl6CellRuntimeV1Get()->Release(failedCell);
    xl6_error_result original=xl6ErrorScopeEnd(&incoming);assert(!original.ok && !original.error);
    const XL6_NativeFuturePayloadRuntimeV1* api=xl6NativeGraphFuturePayloadRuntimeV1Get();
    Marker* payload=api->Allocate(marker_profile());assert(payload);*payload=(Marker){1,false,false};
    Marker output={0};drops=0;copyError=true;
    assert(!api->Copy(payload,marker_profile(),&output) && !output.id && drops==1);copyError=false;xrtClearError();
    api->Drop(payload,NULL);assert(drops==2);check_empty_errors();
    assert(xl6ErrorScopeBegin(&incoming));xl6ErrorScopeCaptureFailure(&incoming);
    payload=api->Allocate(marker_profile());assert(payload);*payload=failure;
    api->Drop(payload,NULL);original=xl6ErrorScopeEnd(&incoming);assert(!original.ok && !original.error);
}
static void allocation_rollback(void)
{
    xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);
    unsigned failures=0;
    for(int64_t point=0;point<4;++point){
        assert(xrtMemDebugFailAfter(point));XL6_Cell* cell=xl6CellCreateNativeGraphV3(marker_profile());
        assert(xrtMemDebugFailAfter(-1));if(!cell)++failures;xrtClearError();xl6CellRuntimeV1Get()->Release(cell);
        xrtMemDebugSnapshot(&after);assert(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes);
        if(cell)break;
    }
    assert(failures==2);failures=0;
    for(int64_t point=0;point<3;++point){
        const XL6_NativeFuturePayloadRuntimeV1* api=xl6NativeGraphFuturePayloadRuntimeV1Get();
        assert(xrtMemDebugFailAfter(point));void* data=api->Allocate(marker_profile());
        assert(xrtMemDebugFailAfter(-1));if(!data)++failures;xrtClearError();api->Drop(data,NULL);
        xrtMemDebugSnapshot(&after);assert(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes);
        if(data)break;
    }
    assert(failures==1);
}
int main(int argc,char** argv)
{
    assert(xrtMemDebugEnable(true));xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);
    const struct {const char* name;void(*run)(void);} cases[]={
        {"cells",cells},{"cell-cycle",cell_cycle},{"futures",futures},{"future-cycle",future_cycle},
        {"cleanup",checked_cleanup},{"allocation",allocation_rollback}};
    bool found=argc==1;
    for(size_t i=0;i<sizeof(cases)/sizeof(cases[0]);++i)
        if(argc==1 || !strcmp(argv[1],cases[i].name)){found=true;cases[i].run();}
    assert(found && argc<=2);assert(xl6ForeignViewDrainV1());xrtClearError();xrtMemDebugSnapshot(&after);
    assert(codeReferences==1 && before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes);
    assert(before.InvalidFreeCount==after.InvalidFreeCount && before.DoubleFreeCount==after.DoubleFreeCount);
    puts("native graph storage: cell/closure and Future cycles, transfers, checked cleanup and allocation rollback passed");return 0;
}
