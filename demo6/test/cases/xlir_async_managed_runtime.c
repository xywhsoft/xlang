#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_FUTURE
#include "lib/xrt/xrt.h"
#include "src/runtime/xl6_continuation_future.h"
#include "src/runtime/xl6_ownership_plan.h"
#include "src/runtime/xl6_future_payload.h"
#include "src/api/xl_foreign_error.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include "xlir_async_managed_dll.h"

static const XL6_ContinuationFrameRuntimeV2* api;
static unsigned transactions,watchTransactions,generatedTransactions,probes,refusals,failures,ooms;
static unsigned dllLifetimes;
static void balance(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtClearError();xrtMemDebugSnapshot(&after);
    assert(before->LiveCount==after.LiveCount&&before->LiveBytes==after.LiveBytes&&
        after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount&&
        before->InvalidFreeCount==after.InvalidFreeCount&&before->DoubleFreeCount==after.DoubleFreeCount&&
        before->UseAfterFreeCount==after.UseAfterFreeCount);
}
static DWORD WINAPI freezer(void* unused)
{
    (void)unused;xrtownershipscope scope={0};ULONGLONG stop=GetTickCount64()+5000;
    while(!xrtOwnershipFreezeTryBegin(&scope)){assert(GetTickCount64()<stop);SwitchToThread();}
    assert(xrtOwnershipScopeEnd(&scope));return 0;
}
static void cross_thread(void)
{
    HANDLE thread=CreateThread(NULL,0,freezer,NULL,0,NULL);
    assert(thread&&WaitForSingleObject(thread,10000)==WAIT_OBJECT_0&&CloseHandle(thread));++probes;
}
static size_t count(void* frame)
{
    xrtownershipscope scope={0};size_t refs=177;
    assert(xrtOwnershipFreezeTryBegin(&scope));xrtownershipref root=api->Ownership(frame);
    assert(root.Ops->Count(root.Data,&refs));assert(xrtOwnershipScopeEnd(&scope));return refs;
}
static void uninspectable(void* frame)
{
    xrtownershipscope scope={0};size_t refs=177;
    assert(xrtOwnershipFreezeTryBegin(&scope));xrtownershipref root=api->Ownership(frame);
    assert(!root.Ops->Count(root.Data,&refs)&&refs==177);
    const xrtownershippreparationv1* preparation=NULL;
    assert(!xl6ContinuationFrameOwnershipAdapterV1(root,&preparation)&&!preparation);
    assert(xrtOwnershipScopeEnd(&scope));
}
typedef struct Stats {unsigned mode,runs,finalizers;void* frame;void* resurrected;} Stats;
typedef struct Frame {
    Stats* stats;xvalue* capture;unsigned char initialized;
    XL6_ContinuationTicket ticket;xfuturewatch watch;xfuture* source;xpromise* outer;
} Frame;
static const XL6_ActivationCaptureSlot slots[]={
    {{XL6_CAPTURE_TRIVIAL,offsetof(Frame,stats),sizeof(Stats*)},SIZE_MAX},
    {{XL6_CAPTURE_XVALUE,offsetof(Frame,capture),sizeof(xvalue*)},offsetof(Frame,initialized)}};
static const size_t cleanupOrder[]={1};
static const XL6_ActivationCaptureLayoutV1 captures={sizeof(captures),sizeof(Frame),2,slots,1,cleanupOrder};
static const XL6_ContinuationFrameLayoutV2 layout={sizeof(layout),&captures,
    offsetof(Frame,ticket),offsetof(Frame,watch),offsetof(Frame,source),offsetof(Frame,outer)};
static bool no_edges(const void* data,xrtownershipvisitor visit,void* context)
{(void)data;(void)visit;(void)context;return true;}
static void no_data(void* data) {assert(!data);}
static void borrowed_stats(void* data) {assert(data);}
static void finalize(xvalue* object,void* data)
{
    (void)object;Stats* stats=data;Frame* frame=stats->frame;
    assert(!frame->capture&&!frame->initialized&&stats->runs==1&&!stats->finalizers);
    ++stats->finalizers;cross_thread();uninspectable(frame);
    assert(api->Retain(frame)==frame);api->Release(frame);
    if(stats->mode==1){stats->resurrected=api->Retain(frame);assert(stats->resurrected);}
    if(stats->mode==2)xl6ErrorScopePropagateFailure((xl6_error_result){false,NULL});
}
static bool finalize_checked(xvalue* object,void* data) {finalize(object,data);return ((Stats*)data)->mode!=2;}
static const xvalueobjectownershipv1 objectPolicy={sizeof(objectPolicy),finalize,no_edges,borrowed_stats,no_edges,no_data,finalize_checked};
static bool resolve(xrtownershipref ref,void* deny,const xrtownershipadapterv1** adapter,const xrtownershippreparationv1** preparation)
{
    *preparation=NULL;
    if((*adapter=xl6ContinuationFrameOwnershipAdapterV1(ref,preparation)))return !deny;
    if((*adapter=xrtValueObjectOwnershipAdapterV1(ref,&objectPolicy)))return true;
    *adapter=xrtValueOwnershipAdapterV1(ref);return *adapter!=NULL;
}
static Frame* new_frame(Stats* stats,bool object)
{
    Frame* frame=api->Allocate(sizeof(*frame),&layout);assert(frame);
    frame->stats=stats;frame->capture=NULL;frame->initialized=0;stats->frame=frame;
    if(object){
        frame->capture=xrtValueObject();assert(frame->capture);
        assert(xrtValueObjectLifetimeBindOwned(frame->capture,NULL,no_edges,no_data)&&
            xrtValueObjectFinalizerPrepareOwned(frame->capture,finalize,stats,no_edges,borrowed_stats)&&
            xrtValueObjectOwnershipBindV1(frame->capture,&objectPolicy)&&xrtValueObjectFinalizerCommit(frame->capture));
        frame->initialized=1;
    }
    return frame;
}
static void run(void* data)
{
    Frame* frame=data;Stats* stats=frame->stats;++stats->runs;cross_thread();uninspectable(frame);
    xl6_error_result outcome=api->Cleanup(frame,(xl6_error_result){stats->mode!=3,NULL});
    assert(outcome.ok==(stats->mode!=2&&stats->mode!=3)&&!outcome.error);
    if(!outcome.ok)++failures;
}
static void native(unsigned mode,unsigned order)
{
    static const unsigned events[6][3]={{0,1,2},{0,2,1},{1,0,2},{1,2,0},{2,0,1},{2,1,0}};
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);Stats stats={0};stats.mode=mode;
    Frame* frame=new_frame(&stats,true);uninspectable(frame);
    XL6_ContinuationQueue* queue=xl6ContinuationQueueCreate(1);assert(queue);
    assert(xl6ContinuationReserve(queue,&frame->ticket,run,api->Retire,frame)==XL6_CONTINUATION_ADMITTED);
    assert(api->Bind(frame));assert(!api->Bind(frame));++refusals;xrtClearError();
    assert(!xl6ContinuationTraceBind(&frame->ticket,no_edges));++refusals;xrtClearError();
    assert(api->Retain(frame)==frame);
    xrtownershipref root=api->Ownership(frame),ticket=xl6ContinuationOwnership(&frame->ticket);
    assert(root.Data==ticket.Data&&root.Ops==ticket.Ops);
    for(unsigned i=0;i<3;++i){
        if(events[order][i]==0)xl6ContinuationCommit(&frame->ticket);
        else if(events[order][i]==1)xl6ContinuationReady(&frame->ticket);
        else xl6ContinuationTransportReleased(&frame->ticket);
        if(i<2)assert(!xl6ContinuationRunOne(queue));
    }
    assert(count(frame)==2);
    xrtownershipscope freeze={0};const xrtownershippreparationv1* prep=NULL;int token=1;
    assert(xrtOwnershipFreezeTryBegin(&freeze));
    const xrtownershipadapterv1* adapter=xl6ContinuationFrameOwnershipAdapterV1(root,&prep);
    assert(adapter&&prep&&!prep->Ready(root.Data)&&adapter->Claim(root.Data,&token));
    assert(xrtOwnershipScopeEnd(&freeze));
    assert(prep->Prepare(root.Data,&token)==XRT_OWNERSHIP_PREPARE_BUSY&&!stats.runs&&!stats.finalizers);
    assert(xrtOwnershipFreezeTryBegin(&freeze));adapter->Restore(root.Data,&token);assert(xrtOwnershipScopeEnd(&freeze));
    XL6_OwnershipPlan* plan=NULL;
    assert(xl6OwnershipPlanTryCreateV2(root,resolve,&token,&plan)==XL6_OWNERSHIP_PLAN_ERROR&&!plan);++refusals;xrtClearError();
    assert(xl6OwnershipPlanTryCreateV2(root,resolve,NULL,&plan)==XL6_OWNERSHIP_PLAN_ROOTED&&plan);
    assert(xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_ROOTED);
    assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY);plan=NULL;
    xl6ContinuationQueueClose(queue);assert(xl6ContinuationRunOne(queue)&&!xl6ContinuationRunOne(queue));
    assert(stats.runs==1&&stats.finalizers==1&&count(frame)==(mode==1?2u:1u));
    assert(xl6ContinuationQueueDestroy(queue));
    assert(xl6OwnershipPlanTryCreateV2(root,resolve,NULL,&plan)==XL6_OWNERSHIP_PLAN_ROOTED&&plan);
    assert(prep->Ready(root.Data));api->Release(frame);
    if(stats.resurrected)api->Release(stats.resurrected);
    assert(xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_READY);
    assert(xl6OwnershipPlanFinish(plan)&&xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY);
    ++transactions;balance(&before);
}
static void complete(xpromise* promise,unsigned outcome)
{
    if(!outcome){int64_t* value=xrtMalloc(sizeof(*value));assert(value);*value=86;assert(xl6PayloadResolveOwned(promise,value,XL6_FUTURE_PAYLOAD_VALUE));}
    else if(outcome==1){xerror* error=xrtErrorCreate(XERR_STATE,"test",0,"source failed");assert(error&&xrtPromiseReject(promise,error));xrtErrorFree(error);}
    else if(outcome==2)assert(xrtPromiseClose(promise));
    else assert(xrtPromiseCancel(promise));
}
static void watches(unsigned outcome,bool ready)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);Stats stats={0};Frame* frame=new_frame(&stats,false);
    XL6_ContinuationQueue* queue=xl6ContinuationQueueCreate(1);xfuture* source=NULL;xpromise* producer=xrtPromiseCreate(&source,NULL);
    assert(queue&&producer&&source);frame->source=xrtFutureRef(source);assert(frame->source);
    assert(xl6ContinuationReserve(queue,&frame->ticket,run,api->Retire,frame)==XL6_CONTINUATION_ADMITTED&&api->Bind(frame));
    assert(api->Retain(frame)==frame);
    if(ready)complete(producer,outcome);
    assert(xl6ContinuationWatchRegister(&frame->ticket,&frame->watch,source));
    assert(!xl6ContinuationAbort(&frame->ticket));
    xl6ContinuationCommit(&frame->ticket);assert(count(frame)==(ready?2u:3u));
    if(!ready){assert(!xl6ContinuationRunOne(queue));complete(producer,outcome);assert(count(frame)==2);}
    assert(xl6ContinuationRunOne(queue)&&stats.runs==1&&!stats.finalizers&&count(frame)==1);
    xl6ContinuationQueueClose(queue);assert(xl6ContinuationQueueDestroy(queue));api->Release(frame);
    xrtPromiseDestroy(producer);xrtFutureDestroy(source);++watchTransactions;balance(&before);
}
static XL6_ContinuationQueue* generatedQueue;static xfuture* sources[2];static void* observed[2];static unsigned observations,entered;
extern xfuture* async_managed_create(void);
XL6_ContinuationQueue* async_managed_queue(void) {return generatedQueue;}
void async_managed_observe(void* frame) {assert(observations<2);observed[observations++]=frame;}
xfuture* async_storage_source(int64_t stage) {assert(stage==0||stage==1);return xrtFutureRef(sources[stage]);}
void async_storage_entered(void) {++entered;}
static void generated(unsigned outcome)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);xpromise* producers[2];observations=entered=0;
    generatedQueue=xl6ContinuationQueueCreate(1);assert(generatedQueue);
    for(unsigned i=0;i<2;++i){producers[i]=xrtPromiseCreate(&sources[i],NULL);assert(producers[i]);}
    xfuture* output=async_managed_create();assert(output&&observations==1&&entered==1);
    void* first=api->Retain(observed[0]);assert(first&&count(first)==4);
    complete(producers[0],0);assert(count(first)==3);xl6ContinuationQueueClose(generatedQueue);
    assert(xl6ContinuationRunOne(generatedQueue)&&observations==2&&count(first)==1);
    void* second=api->Retain(observed[1]);assert(second&&second!=first&&count(second)==4);
    if(outcome==4){assert(xrtFutureCancel(output));complete(producers[1],0);}else complete(producers[1],outcome);
    assert(xl6ContinuationRunOne(generatedQueue)&&count(second)==1&&!xl6ContinuationRunOne(generatedQueue));
    assert(xrtFutureState(output)==(!outcome?XFUTURE_RESOLVED:outcome==1?XFUTURE_FAILED:outcome==2?XFUTURE_CLOSED:XFUTURE_CANCELLED));
    if(!outcome)assert(*(int64_t*)xrtFutureValue(output)==172);
    assert(xl6ContinuationQueueDestroy(generatedQueue));generatedQueue=NULL;
    api->Release(first);api->Release(second);xrtFutureDestroy(output);
    for(unsigned i=0;i<2;++i){xrtPromiseDestroy(producers[i]);xrtFutureDestroy(sources[i]);sources[i]=NULL;}
    ++generatedTransactions;balance(&before);
}
static void invalid(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    for(unsigned n=0;n<9;++n){
        XL6_ContinuationFrameLayoutV2 bad=layout;
        if(n==0)bad.size=0;else if(n==1)bad.captures=NULL;else if(n==2)bad.ticket=SIZE_MAX;
        else if(n==3)bad.watch=bad.ticket;else if(n==4)++bad.source;else if(n==5)bad.outer=bad.source;
        else if(n==6)bad.outer=offsetof(Frame,capture);else if(n==7)bad.source=offsetof(Frame,initialized);
        else bad.ticket=sizeof(Frame)-1;
        assert(!api->Allocate(sizeof(Frame),&bad));++refusals;xrtClearError();
    }
    assert(xrtMemDebugFailAfter(0));assert(!api->Allocate(sizeof(Frame),&layout)&&xrtMemDebugFailTriggered());
    ++ooms;xrtMemDebugFailClear();xrtClearError();
    Stats stats={0};Frame* frame=new_frame(&stats,false);assert(api->Retain(frame)==frame);
    assert(!api->Bind(frame));++refusals;xrtClearError();uninspectable(frame);
    assert(xrtMemDebugFailAfter(0));api->Discard(frame);api->Release(frame);assert(!xrtMemDebugFailTriggered());
    xrtMemDebugFailClear();balance(&before);
}
typedef struct DllOwner {
    XL6_CodeOwnerGraph descriptor;AsyncDllHooks hooks;HMODULE library;void* address;void* frame;
    unsigned pins,acquired,released,unmapped;bool closed;
} DllOwner;
static int acquire_owner(void* data)
{DllOwner* owner=data;if(owner->closed)return 0;++owner->pins;++owner->acquired;return 1;}
static void release_owner(void* data)
{
    DllOwner* owner=data;assert(owner->pins);--owner->pins;++owner->released;
    if(!owner->pins){
        MEMORY_BASIC_INFORMATION memory;assert(owner->closed&&owner->hooks.entered==1&&owner->hooks.returned==1);
        assert(FreeLibrary(owner->library)&&VirtualQuery(owner->address,&memory,sizeof(memory))==sizeof(memory)&&memory.State==MEM_FREE);++owner->unmapped;
    }
}
static bool owner_count(const void* data,size_t* count)
{const DllOwner* owner=data;*count=owner->pins+!owner->closed;return true;}
static const xrtownershipops ownerOps={owner_count,no_edges};
static void during_dll(void* data)
{
    DllOwner* owner=data;assert(owner->closed&&owner->pins==1&&!owner->released);
    cross_thread();uninspectable(owner->frame);
    XL6_CodeLease* child=NULL;
    assert(xl6CodeLeaseAcquireScoped(&owner->descriptor.base,&child)==XL6_CODE_LEASE_ACQUIRED&&child);
    xl6CodeLeaseRelease(child);assert(!owner->released);
}
static void dll_lifetime(const char* file,bool adapterLast)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);DllOwner owner={0};XL6_ContinuationProc callback;
    const XL6_ContinuationFrameLayoutV2* (*get_layout)(void);
    owner.library=LoadLibraryA(file);assert(owner.library);
    FARPROC p=GetProcAddress(owner.library,"async_managed_run");memcpy(&callback,&p,sizeof(callback));owner.address=(void*)p;
    p=GetProcAddress(owner.library,"async_managed_layout");memcpy(&get_layout,&p,sizeof(get_layout));assert(callback&&get_layout);
    owner.descriptor=(XL6_CodeOwnerGraph){{sizeof(owner.descriptor),&owner,acquire_owner,release_owner},0x584C36434F574E31ULL,&ownerOps};
    owner.hooks=(AsyncDllHooks){during_dll,&owner,0,0};
    AsyncDllFrame* frame=api->Allocate(sizeof(*frame),get_layout());assert(frame);frame->hooks=&owner.hooks;owner.frame=frame;
    XL6_ContinuationQueue* queue=xl6ContinuationQueueCreate(1);assert(queue);
    assert(xl6ContinuationReserveOwned(queue,&frame->ticket,callback,api->Retire,frame,&owner.descriptor.base)==XL6_CONTINUATION_ADMITTED&&api->Bind(frame));
    assert(api->Retain(frame)==frame);
    xl6ContinuationCommit(&frame->ticket);xl6ContinuationReady(&frame->ticket);xl6ContinuationTransportReleased(&frame->ticket);
    owner.closed=true;xl6ContinuationQueueClose(queue);assert(count(frame)==2&&owner.pins==1&&owner.acquired==1);
    xrtownershipref root=api->Ownership(frame);const xrtownershippreparationv1* prep=NULL;const xrtownershipadapterv1* adapter=NULL;
    if(adapterLast){
        xrtownershipscope freeze={0};assert(xrtOwnershipFreezeTryBegin(&freeze));
        adapter=xl6ContinuationFrameOwnershipAdapterV1(root,&prep);assert(adapter&&prep&&adapter->Hold(root.Data));
        assert(xrtOwnershipScopeEnd(&freeze));api->Release(frame);assert(count(frame)==2);
    }
    assert(xl6ContinuationRunOne(queue)&&owner.hooks.returned==1&&count(frame)==1&&owner.pins==1&&!owner.released);
    assert(xl6ContinuationQueueDestroy(queue));
    MEMORY_BASIC_INFORMATION memory;assert(VirtualQuery(owner.address,&memory,sizeof(memory))==sizeof(memory)&&memory.State==MEM_COMMIT);
    if(adapterLast){assert(prep->Ready(root.Data));adapter->Drop(root.Data);}else api->Release(frame);
    assert(owner.unmapped==1&&owner.released==1&&!owner.pins);++dllLifetimes;balance(&before);
}
int main(int argc,char** argv)
{
    assert(argc==2&&xrtMemDebugEnable(true));api=xl6ContinuationFrameRuntimeV2Get();assert(api);
    for(unsigned round=0;round<10;++round){
        for(unsigned mode=0;mode<4;++mode)for(unsigned order=0;order<6;++order)native(mode,order);
        for(unsigned outcome=0;outcome<4;++outcome){watches(outcome,false);watches(outcome,true);}
        for(unsigned outcome=0;outcome<5;++outcome)generated(outcome);
        invalid();
        dll_lifetime(argv[1],false);dll_lifetime(argv[1],true);
    }
    assert(transactions==240&&watchTransactions==80&&generatedTransactions==50&&probes==580&&refusals==820&&failures==120&&ooms==10&&dllLifetimes==20);
    puts("Managed async: 240 lifecycle transactions, 80 real Watch outcomes, 50 generated successor chains, 580 cross-thread freezes, 820 ABI/admission refusals, 120 exact failure outcomes, 10 real OOM rollbacks, 20 actual DLL unloads; actual pins and plan retirement, exact memory balance");
    return 0;
}
