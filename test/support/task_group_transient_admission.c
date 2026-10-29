/* Real parent-cancel, synchronous bind and completion-notify windows. The
 * barriers use owned/phased callbacks, never an inherited mutation scope. */
#define XRT_MODULE_ALL
#define XRUNTIME_MODULE_ALL
#include "lib/xruntime/xruntime_features.h"
#if defined(XRUNTIME_IMPLEMENTATION) && !defined(XRT_IMPLEMENTATION)
#define XRT_IMPLEMENTATION
#endif
#include "lib/xrt/xrt.h"
#include "lib/xruntime/xruntime.h"
#include "src/api/xl_task_group_bridge.h"
#include "src/runtime/xl6_ownership_catalog.h"
#include "src/runtime/xl6_ownership_domains.h"
#include <assert.h>
#include <stdio.h>

typedef struct Barrier { volatile int32 refs; xatomic32 entered,release; } Barrier;
static bool barrierCount(const void* data,size_t* refs)
{ const Barrier* value=data;if(!value || !refs || value->refs<=0)return false;*refs=(size_t)value->refs;return true; }
static bool barrierTrace(const void* data,xrtownershipvisitor visit,ptr context)
{ (void)data;(void)visit;(void)context;return true; }
static const xrtownershipops barrierOps={barrierCount,barrierTrace};
static void pauseCallback(ptr data)
{
    Barrier* value=data;xrtAtomic32Store(&value->entered,1,XMEMORY_RELEASE);
    while(!xrtAtomic32Load(&value->release,XMEMORY_ACQUIRE))xrtThreadYield();
}
static void releaseCallback(const void* data)
{
    Barrier* value=(Barrier*)data;xrtownershipscope scope={0};assert(xrtOwnershipMutationBegin(&scope));
    assert(xrtRefRelease(&value->refs)==0);assert(xrtOwnershipScopeEnd(&scope));
}
static const xcancelwatchownershipv1 barrierPolicy={sizeof(barrierPolicy),pauseCallback,releaseCallback,&barrierOps};
typedef struct Work { unsigned mode;xl6_task_group* wrapper;xtaskgroup* native;xfuture* parent;xpromise* producer;xerror* error;bool ok; } Work;
static int32 workRun(ptr data)
{
    Work* work=data;
    if(work->mode==0)work->ok=xrtFutureCancel(work->parent);
    else if(work->mode==1)work->ok=xl6TaskGroupBindParent(work->wrapper,work->parent);
    else if(work->mode==2)work->ok=xrtTaskGroupCancel(work->native);
    else if(work->mode==3)work->ok=xrtPromiseReject(work->producer,work->error);
    else work->ok=xrtPromiseCancel(work->producer);
    assert(!xrtGetError());return 0;
}
typedef struct Edges { xrtownershipref native,watch,item; } Edges;
static bool wrapperEdge(xrtownershipref ref,ptr data)
{ Edges* edges=data;if(!edges->native.Data)edges->native=ref;else edges->watch=ref;return true; }
static bool nativeEdge(xrtownershipref ref,ptr data)
{ Edges* edges=data;if(ref.Ops==xrtTaskGroupWatchPolicyV1Get()->Ops)edges->item=ref;return true; }
static void freeze(xrtownershipscope* scope)
{
    double deadline=xrtTimer()+5;
    while(!xrtOwnershipFreezeTryBegin(scope)){assert(xrtTimer()<deadline);xrtThreadYield();}
}
static XL6_OwnershipAdmissionV1 query(XL6_OwnershipCatalog* catalog,xrtownershipref ref)
{
    const xrtownershipadapterv1* adapter=NULL;const xrtownershippreparationv1* preparation=NULL;
    const xrtownershipfinishv1* completion=NULL;
    XL6_OwnershipAdmissionV1 result=xl6OwnershipCatalogProbeV1(ref,catalog,&adapter,&preparation,&completion);
    assert(result!=XL6_OWNERSHIP_ADMISSION_BUSY || (!adapter && !preparation && !completion));return result;
}
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(!xrtGetError() && before->LiveCount==after.LiveCount && before->LiveBytes==after.LiveBytes);
    assert(after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount);
    assert(before->InvalidFreeCount==after.InvalidFreeCount && before->DoubleFreeCount==after.DoubleFreeCount &&
        before->UseAfterFreeCount==after.UseAfterFreeCount);
}
static void cycle(XL6_OwnershipCatalog* catalog,unsigned mode)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);Barrier barrier={0};barrier.refs=1;
    Work work={0};work.mode=mode;xfuture *source=NULL,*parent=NULL,*peer=NULL;
    xpromise* producer=xrtPromiseCreate(&source,NULL);assert(producer && source);
    work.producer=producer;
    xpromise* parentProducer=xrtPromiseCreate(&parent,NULL);assert(parentProducer && parent);work.parent=parent;
    Edges edges={0};xrtownershipscope scope={0};
    if(mode>=2){
        xtaskgroupconfig config={0};config.CancelOn=XRT_TASK_GROUP_CANCEL_ON_FAILED;
        work.native=xrtTaskGroupCreate(&config);assert(work.native && xrtTaskGroupAdd(work.native,source));
        edges.native=xrtTaskGroupOwnership(work.native);
    }else{
        work.wrapper=xl6TaskGroupCreate();assert(work.wrapper && xl6TaskGroupAdd(work.wrapper,source));
        if(mode==0)assert(xl6TaskGroupBindParent(work.wrapper,parent));
        freeze(&scope);xrtownershipref wrapper=xl6TaskGroupOwnershipV1(work.wrapper);
        assert(wrapper.Ops->Trace(wrapper.Data,wrapperEdge,&edges));assert(xrtOwnershipScopeEnd(&scope));
    }
    freeze(&scope);assert(edges.native.Ops->Trace(edges.native.Data,nativeEdge,&edges) && edges.item.Data);
    assert(xrtOwnershipScopeEnd(&scope));
    xcancel *sourceToken=xrtFutureCancelToken(source),*parentToken=xrtFutureCancelToken(parent);
    assert(sourceToken && parentToken);xcancelwatch* watch=NULL;xcancel* peerToken=NULL;xpromise* peerProducer=NULL;
    if(mode==3){
        peerProducer=xrtPromiseCreate(&peer,NULL);assert(peerProducer && peer && xrtTaskGroupAdd(work.native,peer));
        peerToken=xrtFutureCancelToken(peer);assert(peerToken);
        watch=xrtCancelWatchOwnedV1(peerToken,&barrier,&barrierPolicy);assert(watch);
        work.producer=producer;work.error=xrtErrorCreate(XERR_STATE,"test.task.group",1,"source failed");assert(work.error);
    }else{
        watch=xrtCancelWatchOwnedV1(sourceToken,&barrier,&barrierPolicy);assert(watch);
        if(mode==1)assert(xrtFutureCancel(parent));
    }
    xthread* thread=xrtThreadCreate(workRun,&work,0);assert(thread);double deadline=xrtTimer()+5;
    while(!xrtAtomic32Load(&barrier.entered,XMEMORY_ACQUIRE)){assert(xrtTimer()<deadline);xrtThreadYield();}
    xrtownershipref refs[8];size_t count=0;
    if(mode<2)refs[count++]=xl6TaskGroupOwnershipV1(work.wrapper);
    if(mode!=4){refs[count++]=edges.native;refs[count++]=edges.item;}
    if(mode!=3){
        refs[count++]=xrtFutureOwnership(source);refs[count++]=xrtCancelOwnership(sourceToken);
        refs[count++]=xrtCancelWatchOwnership(watch);
        if(mode==0){refs[count++]=xrtFutureOwnership(parent);refs[count++]=edges.watch;}
    }else{
        refs[count++]=xrtFutureOwnership(peer);refs[count++]=xrtCancelOwnership(peerToken);
        refs[count++]=xrtCancelWatchOwnership(watch);
    }
    XL6_OwnershipAdmissionV1 results[8];freeze(&scope);size_t physical;
    if(mode!=4)assert(!edges.native.Ops->Count(edges.native.Data,&physical));
    else{
        xrtownershipref future=xrtFutureOwnership(source);
        assert(future.Ops->Count(future.Data,&physical) && !future.Ops->Trace(future.Data,nativeEdge,&edges));
    }
    assert(xrtMemDebugFailAfter(0));
    for(size_t i=0;i<count;++i)results[i]=query(catalog,refs[i]);
    assert(!xrtMemDebugFailTriggered() && !xrtGetError());xrtMemDebugFailClear();assert(xrtOwnershipScopeEnd(&scope));
    XL6_OwnershipPlan* plan=NULL;
    assert(xl6OwnershipPlanTryCreateV6(edges.native,xl6OwnershipCatalogProbeV1,NULL,true,catalog,&plan)==XL6_OWNERSHIP_PLAN_BUSY && !plan && !xrtGetError());
    xrtAtomic32Store(&barrier.release,1,XMEMORY_RELEASE);
    assert(xrtThreadWaitFor(thread,5000)==XWAIT_OK && work.ok);xrtThreadDestroy(thread);
    assert(xrtCancelUnwatchChecked(watch));
    assert(!barrier.refs);
    freeze(&scope);
    assert(query(catalog,edges.native)==XL6_OWNERSHIP_ADMISSION_ADMITTED);
    assert(query(catalog,xrtFutureOwnership(source))==XL6_OWNERSHIP_ADMISSION_ADMITTED);
    assert(query(catalog,xrtFutureOwnership(parent))==XL6_OWNERSHIP_ADMISSION_ADMITTED);
    assert(xrtOwnershipScopeEnd(&scope));
    if(mode<3)assert(xrtPromiseCancel(producer));
    else if(mode==3)assert(xrtPromiseCancel(peerProducer));
    if(work.wrapper)xl6TaskGroupDestroy(work.wrapper);else xrtTaskGroupDestroy(work.native);
    xrtErrorFree(work.error);xrtCancelDestroy(peerToken);xrtPromiseDestroy(peerProducer);xrtFutureDestroy(peer);
    xrtCancelDestroy(parentToken);xrtCancelDestroy(sourceToken);
    xrtPromiseDestroy(parentProducer);xrtFutureDestroy(parent);xrtPromiseDestroy(producer);xrtFutureDestroy(source);
    balanced(&before);
    for(size_t i=0;i<count;++i){
        if(results[i]!=XL6_OWNERSHIP_ADMISSION_BUSY)fprintf(stderr,"mode=%u node=%zu admission=%d expected BUSY\n",mode,i,(int)results[i]);
        assert(results[i]==XL6_OWNERSHIP_ADMISSION_BUSY);
    }
}
int main(void)
{
    assert(xrtMemDebugEnable(true));xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    const XL6_OwnershipDomainV1* domains[]={xl6OwnershipDomainResourceTaskGroupV1Get(),xl6OwnershipDomainNativeFutureV1Get()};
    XL6_OwnershipCatalog* catalog=xl6OwnershipCatalogCreateV1(domains,2);assert(catalog);
    for(unsigned round=0;round<16;++round)for(unsigned mode=0;mode<5;++mode)cycle(catalog,mode);
    xl6OwnershipCatalogDestroyV1(catalog);balanced(&before);
    puts("TaskGroup transient admission: 80 real cancel/bind/notify windows, exact BUSY, zero-allocation frozen probes, unchanged physical refusal, balanced ledger");return 0;
}
