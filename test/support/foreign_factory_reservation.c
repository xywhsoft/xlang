/* Runtime protocol gates; generated-language factory tests are separate. */
#include "foreign_view_native_fixture.h"
#include "src/runtime/xl6_foreign_view_abi.h"
#include "src/api/xl_foreign_error.h"

static unsigned watchCalls,producerCalls;
static bool observe(void* raw,void* storage,void(*notify)(void*),void* user)
{++watchCalls;return watch(raw,storage,notify,user);}
static XL6_ForeignViewDescriptorV1 policy(void)
{
    XL6_ForeignViewDescriptorV1 result=rootDescriptor;
    result.flags=XL6_FOREIGN_VIEW_RELIABLE_OWNED_WATCH;result.Watch=observe;
    return result;
}
static bool reserve(XL6_ForeignResultProbeV1* probe,const XL6_ForeignViewDescriptorV1* descriptor)
{return xl6ForeignResultProbeReserveRootV1(probe,descriptor,NULL,&codeOwner,sizeof(Watch),_Alignof(Watch),0);}
static void success(void)
{
    XL6_ForeignResultProbeV1 probe={0};XL6_ForeignViewDescriptorV1 descriptor=policy();
    unsigned calls=watchCalls;assert(reserve(&probe,&descriptor) && codeReferences==2);
    Native* raw=native_create();++producerCalls;
    assert(xrtMemDebugFailAfter(0));
    assert(xl6ForeignResultProbeObserveV1(&probe,raw) && watchCalls==calls+1);
    XL6_ForeignView* root=xl6ForeignResultProbeMaterializeRootV1(&probe);assert(root);
    assert(xl6ForeignResultProbeEndV1(&probe) && !xrtMemDebugFailTriggered());xrtMemDebugFailClear();
    assert(xl6ForeignViewReleaseV1(root) && codeReferences==1 && !xl6ForeignViewPendingV1(0));
}
static void oom(void)
{
    XL6_ForeignViewDescriptorV1 descriptor=policy();unsigned prefixes=0;
    for(int64 index=0;index<32;++index){
        XL6_ForeignResultProbeV1 probe={0};unsigned before=producerCalls;
        xmemdebugsnapshot a,b;xrtMemDebugSnapshot(&a);assert(xrtMemDebugFailAfter(index));
        bool prepared=reserve(&probe,&descriptor);xrtMemDebugFailClear();xrtClearError();
        assert(producerCalls==before);
        if(prepared){assert(xl6ForeignResultProbeEndV1(&probe));break;}
        assert(!probe.self && codeReferences==1);++prefixes;
        xrtMemDebugSnapshot(&b);assert(a.LiveCount==b.LiveCount && a.LiveBytes==b.LiveBytes);
    }
    assert(prefixes>=2);printf("reserved factory OOM prefixes: %u\n",prefixes);
}
static void partial(void)
{
    XL6_ForeignViewDescriptorV1 descriptor=policy();XL6_ForeignResultProbeV1 probe={0};
    assert(reserve(&probe,&descriptor));Native* raw=native_create();raw->failures=1;raw->failWithoutDiagnostic=true;
    xl6_error_scope errors={0};assert(xl6ErrorScopeBegin(&errors));xl6ErrorScopeCaptureFailure(&errors);
    assert(xl6ForeignResultProbeObserveV1(&probe,raw));
    assert(!xl6ForeignResultProbeEndV1(&probe));
    xl6_error_result result=xl6ErrorScopeEnd(&errors);assert(!result.ok && !result.error);
    assert(!raw->closing && raw->watch && codeReferences==2 && xl6ForeignViewPendingV1(0)==1);
    assert(xl6ForeignViewDrainV1() && codeReferences==1);
}
static bool ineligible(void* raw,void* storage,void(*notify)(void*),void* user)
{
    bool attached=observe(raw,storage,notify,user);
    if(attached){xl6_error_result result={false,NULL};assert(xl6ErrorScopeTryPropagateFailureV1(&result));}
    return attached;
}
static void eligibility(void)
{
    XL6_ForeignViewDescriptorV1 descriptor=policy();descriptor.Watch=ineligible;
    XL6_ForeignResultProbeV1 probe={0};assert(reserve(&probe,&descriptor));
    Native* raw=native_create();raw->failures=1;raw->failWithoutDiagnostic=true;
    assert(xrtMemDebugFailAfter(0));
    assert(!xl6ForeignResultProbeObserveV1(&probe,raw) && !xrtGetError() && raw->watch);
    assert(!xrtMemDebugFailTriggered());xrtMemDebugFailClear();
    assert(!xl6ForeignResultProbeEndV1(&probe) && !xrtGetError());
    assert(codeReferences==2 && xl6ForeignViewPendingV1(0)==1 && !raw->closing);
    assert(xl6ForeignViewDrainV1() && codeReferences==1);
}
static bool brokenWatch(void* raw,void* storage,void(*notify)(void*),void* user)
{(void)raw;(void)storage;(void)notify;(void)user;return false;}
static void watch_contract(void)
{
    XL6_ForeignViewDescriptorV1 descriptor=policy();descriptor.flags=0;
    XL6_ForeignResultProbeV1 probe={0};unsigned before=producerCalls;
    assert(!reserve(&probe,&descriptor) && !probe.self && producerCalls==before);xrtClearError();
    /* Deliberate trusted-C contract violation, not a supported native path.
     * The failing observer is never mistaken for consumed native ownership. */
    descriptor=policy();descriptor.Watch=brokenWatch;assert(reserve(&probe,&descriptor));
    Native* raw=native_create();assert(!xl6ForeignResultProbeObserveV1(&probe,raw));
    assert(probe.reservedRoot && !raw->closing);xrtClearError();
    assert(xl6ForeignResultProbeEndV1(&probe) && codeReferences==1);
}

typedef struct Record {void* payload;bool ready,adopted;unsigned* partial;unsigned* normal;} Record;
static bool recordPartial(void* raw)
{
    Record* record=raw;if(!record->ready)return false;
    if(record->partial)++*record->partial;
    xrtFree(record->payload);xrtFree(record);return true;
}
static bool recordNormal(void* raw)
{
    Record* record=raw;assert(record->adopted && record->ready);
    ++*record->normal;xrtFree(record->payload);xrtFree(record);return true;
}
static bool forbiddenBegin(void* raw,void* storage){(void)raw;(void)storage;assert(!"unadopted native begin");return false;}
static bool forbiddenEnd(void* storage){(void)storage;assert(!"unadopted native end");return false;}
static XL6_ForeignViewDescriptorV1 recordPolicy(bool nativeScope)
{
    XL6_ForeignViewDescriptorV1 result={sizeof(result),XL6_FOREIGN_VIEW_V1_TAG,"factory.record",
        0,1,NULL,NULL,recordNormal,0,1,NULL,NULL,NULL,NULL,0,recordPartial,NULL};
    if(nativeScope){result.callBytes=8;result.CallBegin=forbiddenBegin;result.CallEnd=forbiddenEnd;}
    return result;
}
static void records(void)
{
    unsigned partialCalls=0,normalCalls=0;XL6_ForeignViewDescriptorV1 descriptor=recordPolicy(true);
    XL6_ForeignResultProbeV1 probe={0};
    assert(xl6ForeignResultProbeReserveRootV1(&probe,&descriptor,NULL,&codeOwner,0,1,XL6_FOREIGN_RESULT_RECORD_UNADOPTED));
    Record* record=xrtCalloc(1,sizeof(*record));assert(record);
    assert(xl6ForeignResultProbeOwnRecordV1(&probe,record));
    /* The original zero allocation is owned before the producer writes any
     * result. Neither a failed constructor nor a NULL out can discard it. */
    assert(!xl6ForeignResultProbeObserveV1(&probe,NULL));xrtClearError();
    assert(!xl6ForeignResultProbeEndV1(&probe) && !xrtGetError() && codeReferences==2);
    record->ready=true;record->partial=&partialCalls;
    assert(xl6ForeignViewDrainV1() && partialCalls==1 && !normalCalls && codeReferences==1);

    descriptor=recordPolicy(false);probe=(XL6_ForeignResultProbeV1){0};
    assert(xl6ForeignResultProbeReserveRootV1(&probe,&descriptor,NULL,&codeOwner,0,1,XL6_FOREIGN_RESULT_RECORD_UNADOPTED));
    record=xrtCalloc(1,sizeof(*record));assert(record && xl6ForeignResultProbeOwnRecordV1(&probe,record));
    record->payload=xrtMalloc(1);assert(record->payload);record->ready=record->adopted=true;record->normal=&normalCalls;
    assert(xl6ForeignResultProbeObserveV1(&probe,record));
    xl6_error_scope errors={0};assert(xl6ErrorScopeBegin(&errors));xl6ErrorScopeCaptureFailure(&errors);
    assert(xl6ForeignResultProbeStorageAdoptedV1(&probe)); /* prior failed != unadopted */
    assert(xl6ForeignResultProbeEndV1(&probe));xl6_error_result result=xl6ErrorScopeEnd(&errors);
    assert(!result.ok && !result.error && normalCalls==1 && partialCalls==1 && codeReferences==1);
}

static bool reportingFailed;
static bool recordReporting(void* raw)
{
    if(reportingFailed){reportingFailed=false;xl6_error_result failed={false,NULL};assert(xl6ErrorScopeTryPropagateFailureV1(&failed));}
    return recordPartial(raw);
}
static void separate_status(void)
{
    unsigned cleaned=0;XL6_ForeignViewDescriptorV1 descriptor=recordPolicy(false);
    descriptor.Destroy=recordReporting;descriptor.flags=XL6_FOREIGN_VIEW_DESTROY_REPORTS_FAILURE;
    Record* record=xrtCalloc(1,sizeof(*record));assert(record);record->partial=&cleaned;
    XL6_ForeignView* root=xl6ForeignViewCreateRootV1(record,&descriptor,&codeOwner);assert(root);
    assert(xrtMemDebugFailAfter(0));
    assert(xl6ForeignViewCloseV1(root) && !xrtGetError() && !xrtMemDebugFailTriggered());
    xrtMemDebugFailClear();assert(xl6ForeignViewReleaseV1(root) && codeReferences==2 && !cleaned);
    record->ready=true;assert(xl6ForeignViewDrainV1() && codeReferences==1 && cleaned==1);
    for(unsigned consumed=0;consumed<2;++consumed){
        record=xrtCalloc(1,sizeof(*record));assert(record);record->ready=consumed!=0;record->partial=&cleaned;
        root=xl6ForeignViewCreateRootV1(record,&descriptor,&codeOwner);assert(root);
        reportingFailed=true;assert(!xl6ForeignViewCloseV1(root) && !xrtGetError());
        if(!consumed)record->ready=true;
        assert(xl6ForeignViewReleaseV1(root) && codeReferences==1);
    }
    descriptor.flags=0;record=xrtCalloc(1,sizeof(*record));assert(record);record->partial=&cleaned;
    root=xl6ForeignViewCreateRootV1(record,&descriptor,&codeOwner);assert(root);
    assert(!xl6ForeignViewCloseV1(root) && !xrtGetError()); /* Legacy false still fails. */
    record->ready=true;assert(xl6ForeignViewReleaseV1(root) && codeReferences==1);
    descriptor.flags=XL6_FOREIGN_VIEW_DESTROY_REPORTS_FAILURE;XL6_ForeignResultProbeV1 probe={0};
    assert(xl6ForeignResultProbeReserveRootV1(&probe,&descriptor,NULL,&codeOwner,0,1,XL6_FOREIGN_RESULT_RECORD_UNADOPTED));
    record=xrtCalloc(1,sizeof(*record));assert(record);record->partial=&cleaned;
    assert(xl6ForeignResultProbeOwnRecordV1(&probe,record));
    assert(!xl6ForeignResultProbeEndV1(&probe) && !xrtGetError()); /* No opt-in for partial records. */
    record->ready=true;assert(xl6ForeignViewDrainV1() && codeReferences==1 && cleaned==5);
}
static unsigned requested,deferDestroy,rejectRequests,ownDestroyAttempts;
static bool failRequest;
static bool failedDestroy;
static bool requestClose(void* raw)
{
    Native* native=raw;assert(!native->closing);++requested;
    if(rejectRequests){--rejectRequests;return false;}
    if(failRequest){failRequest=false;xl6_error_result failed={false,NULL};assert(xl6ErrorScopeTryPropagateFailureV1(&failed));}
    return true;
}
static bool contextBusy(const void* raw){const Native* native=raw;return native->depth || native->failures;}
static bool destroyOwnScope(void* raw)
{
    Native* native=raw;assert(!native->depth);++ownDestroyAttempts;
    if(native->failures)return false;
    if(deferDestroy){--deferDestroy;return false;}
    if(failedDestroy){failedDestroy=false;xl6_error_result failure={false,NULL};assert(xl6ErrorScopeTryPropagateFailureV1(&failure));return false;}
    NativeScope scope={0};assert(scope_begin(native,&scope));assert(destroy_native(native));
    bool ok=scope_end(&scope);
    if(!ok){xl6_error_result failure={false,xrtTakeError()};assert(xl6ErrorScopeTryPropagateFailureV1(&failure));}
    return true;
}
static void request(void)
{
    unsigned beforeRequested=requested,beforeDestroyed=nativeDestroyed;
    Native* raw=native_create();Native* child=native_child(raw);
    XL6_ForeignViewDescriptorV1 descriptor=policy();descriptor.Destroy=destroyOwnScope;descriptor.Busy=contextBusy;
    descriptor.RequestClose=requestClose;descriptor.flags|=XL6_FOREIGN_VIEW_DEFER_ACTIVE_DESTROY|XL6_FOREIGN_VIEW_DESTROY_OWNS_NATIVE_SCOPE;
    XL6_ForeignView* root=xl6ForeignViewCreateRootV1(raw,&descriptor,&codeOwner);assert(root);
    NativeScope native={0};XL6_ForeignViewCallV1 call={0};assert(xl6ForeignViewCallBeginV1(root,&call,&native,sizeof(native)));
    XL6_ForeignView* view=xl6ForeignViewBorrowChildV1(child,root,&childDescriptor,&codeOwner);assert(view);
    raw->failures=1; /* independent cache/publisher activity */
    assert(xrtMemDebugFailAfter(0));
    assert(xl6ForeignViewCloseV1(root) && requested==beforeRequested+1 && !raw->closing && raw->watch);
    assert(xl6ForeignViewCloseV1(view) && !child->closing);
    assert(xl6ForeignViewCallEndV1(&call) && !raw->closing && !xrtGetError());
    assert(!xrtMemDebugFailTriggered());xrtMemDebugFailClear();
    assert(xl6ForeignViewReleaseV1(view) && xl6ForeignViewReleaseV1(root));
    assert(nativeDestroyed==beforeDestroyed && requested==beforeRequested+1 && codeReferences==3);
    assert(xl6ForeignViewPendingV1(0)==2);raw->failures=0;
    assert(xl6ForeignViewDrainV1() && nativeDestroyed==beforeDestroyed+1 && codeReferences==1);
}
static void request_deferred(void)
{
    XL6_ForeignViewDescriptorV1 descriptor=policy();descriptor.Destroy=destroyOwnScope;descriptor.Busy=contextBusy;
    descriptor.RequestClose=requestClose;descriptor.flags|=XL6_FOREIGN_VIEW_DEFER_ACTIVE_DESTROY|XL6_FOREIGN_VIEW_DESTROY_OWNS_NATIVE_SCOPE;
    unsigned beforeRequested=requested;Native* raw=native_create();
    XL6_ForeignView* root=xl6ForeignViewCreateRootV1(raw,&descriptor,&codeOwner);assert(root);
    deferDestroy=2;
    assert(xl6ForeignViewCloseV1(root) && !raw->closing && !xrtGetError());
    assert(xl6ForeignViewReleaseV1(root) && !raw->closing && !xrtGetError() && codeReferences==2);
    assert(xl6ForeignViewDrainV1() && codeReferences==1 && requested==beforeRequested+1);
    raw=native_create();root=xl6ForeignViewCreateRootV1(raw,&descriptor,&codeOwner);assert(root);
    failedDestroy=true;assert(!xl6ForeignViewCloseV1(root) && !xrtGetError() && !raw->closing && codeReferences==2);
    assert(xl6ForeignViewReleaseV1(root) && codeReferences==1);
}
static void request_rejected(void)
{
    XL6_ForeignViewDescriptorV1 descriptor=policy();descriptor.Destroy=destroyOwnScope;descriptor.Busy=contextBusy;
    descriptor.RequestClose=requestClose;descriptor.flags|=XL6_FOREIGN_VIEW_DEFER_ACTIVE_DESTROY|XL6_FOREIGN_VIEW_DESTROY_OWNS_NATIVE_SCOPE;
    Native* raw=native_create();XL6_ForeignView* root=xl6ForeignViewCreateRootV1(raw,&descriptor,&codeOwner);assert(root);
    rejectRequests=1;assert(!xl6ForeignViewCloseV1(root) && !xrtGetError() && !xl6ForeignViewPendingV1(0));
    NativeScope native={0};XL6_ForeignViewCallV1 call={0};
    assert(xl6ForeignViewCallBeginV1(root,&call,&native,sizeof(native)) && xl6ForeignViewCallEndV1(&call));
    assert(xl6ForeignViewReleaseV1(root) && codeReferences==1);
    raw=native_create();root=xl6ForeignViewCreateRootV1(raw,&descriptor,&codeOwner);assert(root);
    failRequest=true;assert(!xl6ForeignViewCloseV1(root) && !xrtGetError() && xl6ForeignViewPendingV1(0));
    assert(!xl6ForeignViewCallBeginV1(root,&call,&native,sizeof(native)));xrtClearError();
    assert(xl6ForeignViewReleaseV1(root) && codeReferences==1);
}
static void request_resurrection(void)
{
    XL6_ForeignViewDescriptorV1 descriptor=policy();descriptor.Destroy=destroyOwnScope;descriptor.Busy=contextBusy;
    descriptor.RequestClose=requestClose;descriptor.flags|=XL6_FOREIGN_VIEW_DEFER_ACTIVE_DESTROY|XL6_FOREIGN_VIEW_DESTROY_OWNS_NATIVE_SCOPE;
    unsigned beforeRequested=requested,beforeDestroyed=nativeDestroyed;
    Native* raw=native_create();XL6_ForeignView* root=xl6ForeignViewCreateRootV1(raw,&descriptor,&codeOwner);assert(root);
    XL6_ForeignView* pin=xl6ForeignViewNativePinV1(root);assert(pin);
    rejectRequests=1;
    assert(!xl6ForeignViewReleaseV1(root) && !xrtGetError() && requested==beforeRequested+1);
    assert(xl6ForeignViewPendingV1(0)==1 && !raw->closing && codeReferences==2);
    XL6_ForeignView* revived=xl6ForeignViewRetainV1(pin);assert(revived);
    assert(xl6ForeignViewDrainV1() && !xl6ForeignViewPendingV1(0) && requested==beforeRequested+1);
    NativeScope native={0};XL6_ForeignViewCallV1 call={0};
    assert(xl6ForeignViewCallBeginV1(revived,&call,&native,sizeof(native)) && xl6ForeignViewCallEndV1(&call));
    assert(xl6ForeignViewReleaseV1(revived) && nativeDestroyed==beforeDestroyed+1 && codeReferences==2);
    xl6ForeignViewNativeUnpinV1(pin);assert(codeReferences==1);

    raw=native_create();raw->failures=1;root=xl6ForeignViewCreateRootV1(raw,&descriptor,&codeOwner);assert(root);
    pin=xl6ForeignViewNativePinV1(root);assert(pin);
    assert(xl6ForeignViewCloseV1(root) && xl6ForeignViewReleaseV1(root));
    assert(!xl6ForeignViewRetainV1(pin));xrtClearError();
    assert(xl6ForeignViewDrainV1() && xl6ForeignViewPendingV1(0)==1);
    raw->failures=0;assert(xl6ForeignViewDrainV1() && nativeDestroyed==beforeDestroyed+2 && codeReferences==2);
    xl6ForeignViewNativeUnpinV1(pin);assert(codeReferences==1);
}
static int32 releaseOnWorker(ptr raw)
{assert(xl6ForeignViewReleaseV1(raw));return 0;}
static void request_worker(void)
{
    Native* raw=native_create();raw->failures=1;unsigned beforeRequested=requested;
    XL6_ForeignViewDescriptorV1 descriptor=policy();descriptor.Destroy=destroyOwnScope;descriptor.Busy=contextBusy;
    descriptor.RequestClose=requestClose;descriptor.flags|=XL6_FOREIGN_VIEW_DEFER_ACTIVE_DESTROY|XL6_FOREIGN_VIEW_DESTROY_OWNS_NATIVE_SCOPE;
    XL6_ForeignView* root=xl6ForeignViewCreateRootV1(raw,&descriptor,&codeOwner);assert(root);
    xthread* thread=xrtThreadCreate(releaseOnWorker,root,0);assert(thread);assert(xrtThreadWait(thread)==XWAIT_OK);xrtThreadDestroy(thread);
    assert(requested==beforeRequested && !raw->closing && codeReferences==2 && xl6ForeignViewPendingV1(0)==1);
    assert(xl6ForeignViewDrainV1() && requested==beforeRequested+1 && !raw->closing);
    raw->failures=0;assert(xl6ForeignViewDrainV1() && codeReferences==1);
}
static void idle_progress(void)
{
    XL6_ForeignViewDescriptorV1 descriptor=policy();descriptor.Destroy=destroyOwnScope;descriptor.Busy=contextBusy;
    descriptor.RequestClose=requestClose;descriptor.flags|=XL6_FOREIGN_VIEW_DEFER_ACTIVE_DESTROY|XL6_FOREIGN_VIEW_DESTROY_OWNS_NATIVE_SCOPE;
    Native* raw=native_create();raw->failures=1;
    XL6_ForeignView* root=xl6ForeignViewCreateRootV1(raw,&descriptor,&codeOwner);assert(root);
    unsigned attempts=ownDestroyAttempts;
    assert(xl6ForeignViewCloseV1(root) && !xrtGetError() && ownDestroyAttempts==attempts+1 && !raw->closing);
    const XL6_OwnershipDomainV1* domains[]={xl6OwnershipDomainForeignViewV1Get()};
    XL6_OwnershipCatalog* catalog=xl6OwnershipCatalogCreateV1(domains,1);assert(catalog);
    const xrtownershipadapterv1* adapter=NULL;const xrtownershippreparationv1* prepare=NULL;
    const XL6_OwnershipFinishV1* finish=NULL;
    assert(xl6OwnershipCatalogProbeV1(xl6ForeignViewOwnershipV1(root),catalog,&adapter,&prepare,&finish)==XL6_OWNERSHIP_ADMISSION_BUSY);
    assert(!adapter && !prepare && !finish && ownDestroyAttempts==attempts+1);
    xl6OwnershipCatalogDestroyV1(catalog);
    assert(xl6ForeignViewReleaseV1(root) && !xrtGetError() && ownDestroyAttempts==attempts+2 && codeReferences==2);
    raw->failures=0;assert(xl6ForeignViewDrainV1() && codeReferences==1);
}
static bool factoryDiscover(XL6_ModuleOwnershipVisit visit,void* context,void* catalog)
{(void)catalog;return xl6ForeignViewDiscoverV1(NULL,visit,context);}
static void deferred_finalize(void)
{
    for(unsigned failure=0;failure<2;++failure){
        unsigned before=nativeDestroyed;
        Native* raw=native_create();XL6_ForeignViewDescriptorV1 descriptor=policy();
        descriptor.Destroy=destroyOwnScope;descriptor.Busy=contextBusy;descriptor.RequestClose=requestClose;
        descriptor.flags|=XL6_FOREIGN_VIEW_DEFER_ACTIVE_DESTROY|XL6_FOREIGN_VIEW_DESTROY_OWNS_NATIVE_SCOPE;
        XL6_ForeignView* root=xl6ForeignViewCreateRootV1(raw,&descriptor,NULL);assert(root);
        raw->capture=xl6ForeignViewRetainV1(root);assert(raw->capture && xl6ForeignViewReleaseV1(root));
        const XL6_OwnershipDomainV1* domains[]={xl6OwnershipDomainForeignViewV1Get()};
        XL6_OwnershipCatalog* catalog=xl6OwnershipCatalogCreateV1(domains,1);assert(catalog);
        XL6_OwnershipPlan* plan=NULL;
        assert(xl6OwnershipPlanTryCreateV6(xl6ForeignViewOwnershipV1(root),xl6OwnershipCatalogProbeV1,factoryDiscover,true,catalog,&plan)>0 && plan);
        assert(xl6OwnershipPlanPrepare(plan)==XL6_OWNERSHIP_STEP_READY);
        deferDestroy=failure ? 0:1;failedDestroy=failure!=0;
        assert(xl6OwnershipPlanFinalizeV2(plan)==XL6_OWNERSHIP_STEP_FAILED && !xrtGetError());
        assert(!raw->closing && raw->watch && nativeDestroyed==before);
        assert(xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_ERROR);xrtClearError();
        assert(xl6OwnershipPlanTryDestroy(plan)>0);xl6OwnershipCatalogDestroyV1(catalog);
        assert(xl6ForeignViewDrainV1() && nativeDestroyed==before+1 && !xl6ForeignViewPendingV1(0));
    }
}
static int acquireEscaped(void* data)
{
    int result=codeAcquire(data);xl6_error_result failed={false,NULL};assert(xl6ErrorScopeTryPropagateFailureV1(&failed));return result;
}
static void acquire_failure(void)
{
    XL6_CodeOwner owner=codeOwner;owner.acquire=acquireEscaped;
    XL6_ForeignViewDescriptorV1 descriptor=policy();XL6_ForeignResultProbeV1 probe={0};
    assert(!xl6ForeignResultProbeReserveRootV1(&probe,&descriptor,NULL,&owner,sizeof(Watch),_Alignof(Watch),0));
    assert(!probe.self && codeReferences==1 && !xrtGetError());
}
int main(int argc,char** argv)
{
    assert(xrtMemDebugEnable(true));
    XL6_ForeignResultProbeV1 warm={0};XL6_ForeignViewDescriptorV1 descriptor=policy();
    assert(reserve(&warm,&descriptor) && xl6ForeignResultProbeEndV1(&warm));
    xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);
    const struct {const char* name;void(*run)(void);} cases[]={
        {"success",success},{"oom",oom},{"partial",partial},{"eligibility",eligibility},
        {"watch",watch_contract},{"records",records},{"status",separate_status},{"finalize",deferred_finalize},{"request",request},{"defer",request_deferred},{"reject",request_rejected},{"resurrection",request_resurrection},{"worker",request_worker},{"idle",idle_progress},{"acquire",acquire_failure}};
    bool found=argc==1;
    for(size_t i=0;i<sizeof(cases)/sizeof(cases[0]);++i)
        if(argc==1 || !strcmp(argv[1],cases[i].name)){found=true;cases[i].run();}
    assert(found && argc<=2);xrtClearError();xrtMemDebugSnapshot(&after);
    assert(codeReferences==1 && !xl6ForeignViewPendingV1(0));
    assert(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes);
    puts("factory reservation: preallocated roots, partial/BUSY owners, record phase and requested close: PASS");return 0;
}
