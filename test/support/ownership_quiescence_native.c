/* Actual resident worker transitions, plus fail-closed probe controls. */
#define XRT_MODULE_ALL
#define XRUNTIME_MODULE_ALL
#include "lib/xruntime/xruntime.h"
#include "src/runtime/xl6_continuation_executor.h"
#include "src/runtime/xl6_ownership_catalog.h"
#include "src/runtime/xl6_ownership_domains.h"
#include <assert.h>
#include <stdio.h>

typedef struct Owner {
    volatile int32 refs;
    struct Owner* self;
    XL6_ContinuationExecutor* executor;
    const void* claim;
    unsigned *dropped,*finalized;
    bool cleared;
} Owner;
typedef struct Work { XL6_ContinuationTicket ticket; xatomic32 started,release,ended; } Work;
static const xrtownershipops ownerOps;
static xrtownershipref view(const Owner* owner) { return (xrtownershipref){owner,&ownerOps}; }
static void end(xrtownershipscope* scope) { assert(xrtOwnershipScopeEnd(scope)); }
static bool count(const void* data,size_t* refs)
{ const Owner* owner=data;if(!owner || !refs || owner->cleared || owner->refs<=0)return false;*refs=(size_t)owner->refs;return true; }
static bool trace(const void* data,xrtownershipvisitor visit,ptr context)
{ const Owner* owner=data;return visit(view(owner->self),context) && visit(xl6ContinuationExecutorOwnership(owner->executor),context); }
static const xrtownershipops ownerOps={count,trace};
static bool hold(const void* data)
{ Owner* owner=(Owner*)data;xrtownershipscope scope={0};assert(xrtOwnershipMutationBegin(&scope));bool held=xrtRefRetain(&owner->refs)>0;end(&scope);return held; }
static void drop(const void* data)
{
    Owner* owner=(Owner*)data;xrtownershipscope scope={0};assert(xrtOwnershipMutationBegin(&scope));
    if(!xrtRefRelease(&owner->refs)){assert(!owner->self && !owner->executor);++*owner->dropped;xrtFree(owner);}end(&scope);
}
static bool claim(const void* data,const void* token)
{ Owner* owner=(Owner*)data;if(!token || owner->cleared || (owner->claim && owner->claim!=token))return false;owner->claim=token;return true; }
static void restore(const void* data,const void* token)
{ Owner* owner=(Owner*)data;assert(owner->claim==token && !owner->cleared);owner->claim=NULL; }
static bool finalize(const void* data,const void* token)
{ const Owner* owner=data;assert(owner->claim==token && !owner->cleared && xl6ContinuationExecutorIsIdle(owner->executor));++*owner->finalized;return true; }
static void clear(const void* data,const void* token)
{ Owner* owner=(Owner*)data;assert(owner->claim==token && !owner->cleared);owner->cleared=true; }
static bool finish(const void* data,const void* token)
{
    Owner* owner=(Owner*)data;assert(owner->claim==token && owner->cleared);
    xrtownershipscope scope={0};assert(xrtOwnershipMutationBegin(&scope));Owner* self=owner->self;
    XL6_ContinuationExecutor* executor=owner->executor;owner->self=NULL;owner->executor=NULL;end(&scope);
    assert(xl6ContinuationExecutorDestroyCompleted(executor));drop(self);return true;
}
static const xrtownershipadapterv1 ownerAdapter={sizeof(ownerAdapter),hold,drop,claim,restore,finalize,clear,finish};
static XL6_OwnershipAdmissionV1 probe(xrtownershipref ref,void* catalog,const xrtownershipadapterv1** adapter,
    const xrtownershippreparationv1** preparation,const xrtownershipfinishv1** completion)
{
    *adapter=NULL;*preparation=NULL;*completion=NULL;
    if(ref.Ops==&ownerOps){*adapter=&ownerAdapter;return XL6_OWNERSHIP_ADMISSION_ADMITTED;}
    return xl6OwnershipCatalogProbeV1(ref,catalog,adapter,preparation,completion);
}
static void run(ptr data)
{
    Work* work=data;xrtAtomic32Store(&work->started,1,XMEMORY_RELEASE);
    while(!xrtAtomic32Load(&work->release,XMEMORY_ACQUIRE))xrtThreadYield();
    assert(xl6ContinuationComplete(&work->ticket));
}
static void released(ptr data)
{ Work* work=data;xrtAtomic32Store(&work->ended,1,XMEMORY_RELEASE); }
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(!xrtGetError() && before->LiveCount==after.LiveCount && before->LiveBytes==after.LiveBytes);
    assert(after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount && before->InvalidFreeCount==after.InvalidFreeCount &&
        before->DoubleFreeCount==after.DoubleFreeCount && before->UseAfterFreeCount==after.UseAfterFreeCount);
}
static void cycle(XL6_OwnershipCatalog* catalog)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);unsigned dropped=0,finalized=0;
    Owner* owner=xrtCalloc(1,sizeof(*owner));assert(owner);owner->refs=1;owner->self=owner;owner->dropped=&dropped;owner->finalized=&finalized;
    owner->executor=xl6ContinuationExecutorCreateGuaranteed(2,1);assert(owner->executor);
    Work work={0};assert(xl6ContinuationExecutorReserve(owner->executor,&work.ticket,run,released,&work)==XL6_CONTINUATION_ADMITTED);
    xl6ContinuationCommit(&work.ticket);xl6ContinuationReady(&work.ticket);xl6ContinuationTransportReleased(&work.ticket);
    double deadline=xrtTimer()+5;while(!xrtAtomic32Load(&work.started,XMEMORY_ACQUIRE)){assert(xrtTimer()<deadline);xrtThreadYield();}
    XL6_OwnershipPlan* plan=NULL;
    assert(xl6OwnershipPlanTryCreateV6(view(owner),probe,NULL,false,catalog,&plan)==XL6_OWNERSHIP_PLAN_BUSY && !plan && !xrtGetError());
    assert(!owner->claim && !finalized && !dropped);
    xrtAtomic32Store(&work.release,1,XMEMORY_RELEASE);
    XL6_OwnershipPlanResult created;
    do{created=xl6OwnershipPlanTryCreateV6(view(owner),probe,NULL,false,catalog,&plan);assert(created!=XL6_OWNERSHIP_PLAN_ERROR && xrtTimer()<deadline);if(created==XL6_OWNERSHIP_PLAN_BUSY)xrtThreadYield();}while(created==XL6_OWNERSHIP_PLAN_BUSY);
    assert(created==XL6_OWNERSHIP_PLAN_PREPARATIONS && plan);
    XL6_OwnershipStepResult prepared;
    do{prepared=xl6OwnershipPlanPrepare(plan);assert(prepared>=XL6_OWNERSHIP_STEP_BUSY && !xrtGetError() && xrtTimer()<deadline);if(prepared==XL6_OWNERSHIP_STEP_BUSY)xrtThreadYield();}while(prepared==XL6_OWNERSHIP_STEP_BUSY);
    assert(xrtAtomic32Load(&work.ended,XMEMORY_ACQUIRE) && xl6OwnershipPlanFinalizeV2(plan)==XL6_OWNERSHIP_STEP_READY && finalized==1);
    assert(xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_READY && xrtMemDebugFailAfter(0));
    assert(xl6OwnershipPlanFinishV2(plan)==XL6_OWNERSHIP_STEP_READY && !xrtMemDebugFailTriggered() && !dropped);
    assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY && dropped==1);xrtMemDebugFailClear();balanced(&before);
}
static unsigned unknownCalls;
static bool unknownCount(const void* data,size_t* refs) { (void)data;(void)refs;++unknownCalls;return false; }
static bool unknownTrace(const void* data,xrtownershipvisitor visit,ptr context) { (void)data;(void)visit;(void)context;++unknownCalls;return false; }
static const xrtownershipops unknownOps={unknownCount,unknownTrace};
static XL6_OwnershipAdmissionV1 invalidBusy(xrtownershipref ref,void* context,const xrtownershipadapterv1** adapter,
    const xrtownershippreparationv1** preparation,const xrtownershipfinishv1** completion)
{
    (void)ref;(void)context;*adapter=&ownerAdapter;*preparation=NULL;*completion=NULL;return XL6_OWNERSHIP_ADMISSION_BUSY;
}
static bool busyA(const void* data) { (void)data;++unknownCalls;return true; }
static bool busyB(const void* data) { (void)data;++unknownCalls;return false; }
static bool conflictingLoad(XL6_OwnershipCatalog* catalog)
{
    static const XL6_OwnershipQuiescenceV1 one={sizeof(one),&unknownOps,busyA},two={sizeof(two),&unknownOps,busyB};
    return xl6OwnershipCatalogAddQuiescenceV1(catalog,&one) && xl6OwnershipCatalogAddQuiescenceV1(catalog,&two);
}
static void refused(XL6_OwnershipCatalog* catalog)
{
    const xrtownershipadapterv1* adapter=NULL;const xrtownershippreparationv1* preparation=NULL;const xrtownershipfinishv1* completion=NULL;
    xrtownershipref unknown={catalog,&unknownOps};xrtownershipscope scope={0};assert(xrtOwnershipFreezeTryBegin(&scope));
    assert(xl6OwnershipCatalogProbeV1(unknown,catalog,&adapter,&preparation,&completion)==XL6_OWNERSHIP_ADMISSION_REFUSED && !adapter && !preparation && !completion && !unknownCalls);end(&scope);
    XL6_OwnershipPlan* plan=NULL;assert(xl6OwnershipPlanTryCreateV6(unknown,probe,NULL,false,catalog,&plan)==XL6_OWNERSHIP_PLAN_ERROR && !plan && !unknownCalls);xrtClearError();
    assert(xl6OwnershipPlanTryCreateV6(unknown,invalidBusy,NULL,true,catalog,&plan)==XL6_OWNERSHIP_PLAN_ERROR && !plan && !unknownCalls);xrtClearError();
    static const XL6_OwnershipDomainV1 conflicting={sizeof(conflicting),conflictingLoad,NULL,NULL};
    const XL6_OwnershipDomainV1* domains[]={&conflicting};
    assert(!xl6OwnershipCatalogCreateV1(domains,1) && !unknownCalls && xrtGetError() && xrtErrorKind(xrtGetError())==XERR_STATE);xrtClearError();
    /* A stable opaque reservation is NOT an audited transient transition. */
    XL6_ContinuationQueue* queue=xl6ContinuationQueueCreate(1);assert(queue);Work work={0};
    assert(xl6ContinuationReserve(queue,&work.ticket,run,released,&work)==XL6_CONTINUATION_ADMITTED);
    assert(xrtOwnershipFreezeTryBegin(&scope));
    assert(xl6OwnershipCatalogProbeV1(xl6ContinuationQueueOwnership(queue),catalog,&adapter,&preparation,&completion)==XL6_OWNERSHIP_ADMISSION_REFUSED);
    assert(!adapter && !preparation && !completion);end(&scope);
    assert(xl6ContinuationAbort(&work.ticket));xl6ContinuationQueueClose(queue);assert(xl6ContinuationQueueDestroy(queue));
}
typedef struct PoolWork { xatomic32 started,release,dropStarted,dropRelease; } PoolWork;
static xtaskoutcome poolRun(xcancel* cancel,ptr data,xtaskvalue* result)
{
    (void)cancel;(void)result;PoolWork* work=data;xrtAtomic32Store(&work->started,1,XMEMORY_RELEASE);
    while(!xrtAtomic32Load(&work->release,XMEMORY_ACQUIRE))xrtThreadYield();
    return XTASK_SUCCESS;
}
static void poolDrop(ptr data,ptr context)
{
    (void)context;PoolWork* work=data;xrtAtomic32Store(&work->dropStarted,1,XMEMORY_RELEASE);
    while(!xrtAtomic32Load(&work->dropRelease,XMEMORY_ACQUIRE))xrtThreadYield();
}
static bool loadNativePool(XL6_OwnershipCatalog* catalog)
{ return xl6OwnershipCatalogAddQuiescenceV1(catalog,xrtTaskPoolOwnershipQuiescenceV1Get()); }
static bool resolveNativePool(xrtownershipref ref,const XL6_OwnershipPoliciesV1* policies,
    const xrtownershipadapterv1** adapter,const xrtownershippreparationv1** preparation,const xrtownershipfinishv1** completion)
{ (void)policies;(void)completion;*adapter=xrtTaskPoolOwnershipAdapterV1(ref,preparation);return *adapter!=NULL; }
static void poolProbe(XL6_OwnershipCatalog* catalog,xtaskpool* pool,XL6_OwnershipAdmissionV1 expected)
{
    const xrtownershipadapterv1* adapter=NULL;const xrtownershippreparationv1* preparation=NULL;const xrtownershipfinishv1* completion=NULL;
    xrtownershipscope freeze={0};double deadline=xrtTimer()+5;
    while(!xrtOwnershipFreezeTryBegin(&freeze)){assert(xrtTimer()<deadline);xrtThreadYield();}
    assert(xrtMemDebugFailAfter(0));
    assert(xl6OwnershipCatalogProbeV1(xrtTaskPoolOwnership(pool),catalog,&adapter,&preparation,&completion)==expected);
    assert(expected==XL6_OWNERSHIP_ADMISSION_BUSY ? !adapter && !preparation && !completion : adapter && preparation && !completion);
    assert(!xrtMemDebugFailTriggered() && !xrtGetError());xrtMemDebugFailClear();end(&freeze);
}
static void poolCycle(XL6_OwnershipCatalog* catalog)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);PoolWork work={0};xtaskpoolconfig config={1,2,0};
    xtaskpool* pool=xrtTaskPoolCreate(&config);assert(pool);xtaskargs args={0};args.Destroy=poolDrop;
    xfuture* output=xrtTaskSubmit(pool,poolRun,&work,&args);assert(output);double deadline=xrtTimer()+5;
    while(!xrtAtomic32Load(&work.started,XMEMORY_ACQUIRE)){assert(xrtTimer()<deadline);xrtThreadYield();}
    poolProbe(catalog,pool,XL6_OWNERSHIP_ADMISSION_BUSY);
    xrtAtomic32Store(&work.release,1,XMEMORY_RELEASE);
    while(!xrtAtomic32Load(&work.dropStarted,XMEMORY_ACQUIRE)){assert(xrtTimer()<deadline);xrtThreadYield();}
    poolProbe(catalog,pool,XL6_OWNERSHIP_ADMISSION_BUSY);
    xrtAtomic32Store(&work.dropRelease,1,XMEMORY_RELEASE);
    assert(xrtTaskPoolClose(pool) && xrtTaskPoolWaitFor(pool,5000)==XWAIT_OK);poolProbe(catalog,pool,XL6_OWNERSHIP_ADMISSION_ADMITTED);
    xrtFutureDestroy(output);assert(xrtTaskPoolDestroy(pool));balanced(&before);
}
int main(void)
{
    assert(xrtMemDebugEnable(true));xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    static const XL6_OwnershipDomainV1 nativePool={sizeof(nativePool),loadNativePool,resolveNativePool,NULL};
    const XL6_OwnershipDomainV1* domains[]={xl6OwnershipDomainExecutorV1Get(),xl6OwnershipDomainQueueV1Get(),&nativePool};
    XL6_OwnershipCatalog* catalog=xl6OwnershipCatalogCreateV1(domains,3);assert(catalog);refused(catalog);
    for(unsigned round=0;round<64;++round){cycle(catalog);poolCycle(catalog);}
    xl6OwnershipCatalogDestroyV1(catalog);balanced(&before);
    puts("Ownership quiescence: 64 real workers, 64 native pools with running and finalizer-tail barriers, BUSY preserved, unknown Count/Trace refused, preparation retried, zero-allocation probes and finish, full ledger balanced");return 0;
}
