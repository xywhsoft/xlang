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
#include <windows.h>

static const XL6_ContinuationFrameRuntimeV2* api;
static const XL6_ContinuationFrameRuntimeV3* produced;
static unsigned graphs,denials,rootRetries,successorRetries,steps,freezes,oomAttempts,trueOoms;
static void balance(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtClearError();xrtMemDebugSnapshot(&after);
    assert(before->LiveCount==after.LiveCount&&before->LiveBytes==after.LiveBytes&&
        after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount&&
        before->InvalidFreeCount==after.InvalidFreeCount&&before->DoubleFreeCount==after.DoubleFreeCount&&
        before->UseAfterFreeCount==after.UseAfterFreeCount);
}
static DWORD WINAPI freeze_worker(void* unused)
{
    (void)unused;xrtownershipscope freeze={0};DWORD started=GetTickCount();
    while(!xrtOwnershipFreezeTryBegin(&freeze)){assert((DWORD)(GetTickCount()-started)<5000);SwitchToThread();}
    assert(xrtOwnershipScopeEnd(&freeze));return 0;
}
static void cross_thread(void)
{
    HANDLE worker=CreateThread(NULL,0,freeze_worker,NULL,0,NULL);
    assert(worker&&WaitForSingleObject(worker,10000)==WAIT_OBJECT_0&&CloseHandle(worker));++freezes;
}
static bool same(xrtownershipref a,xrtownershipref b) {return a.Data==b.Data&&a.Ops==b.Ops;}
typedef struct Edges {xrtownershipref items[32];size_t count;} Edges;
static bool collect(xrtownershipref ref,void* data)
{
    Edges* edges=data;if(!ref.Data&&!ref.Ops)return true;
    assert(ref.Data&&ref.Ops&&edges->count<32);edges->items[edges->count++]=ref;return true;
}
static Edges edges_of(xrtownershipref ref)
{
    xrtownershipscope freeze={0};Edges edges={0};assert(xrtOwnershipFreezeTryBegin(&freeze));
    assert(ref.Ops->Trace(ref.Data,collect,&edges));assert(xrtOwnershipScopeEnd(&freeze));return edges;
}
static size_t refs(xrtownershipref ref)
{
    xrtownershipscope freeze={0};size_t count=0;assert(xrtOwnershipFreezeTryBegin(&freeze));
    assert(ref.Ops->Count(ref.Data,&count));assert(xrtOwnershipScopeEnd(&freeze));return count;
}
static xrtownershipref producer_of(xfuture* output,xrtownershipref frame)
{
    Edges edges=edges_of(xrtFutureOwnership(output));xrtownershipref found={0};
    xrtownershipscope freeze={0};assert(xrtOwnershipFreezeTryBegin(&freeze));
    for(size_t i=0;i<edges.count;++i){const xrtownershippreparationv1* preparation=NULL;
        if(xl6ContinuationFrameOwnershipAdapterV1(edges.items[i],&preparation)){
            assert(!found.Data&&edges.items[i].Ops!=frame.Ops);found=edges.items[i];
        }
    }
    assert(xrtOwnershipScopeEnd(&freeze)&&found.Data);return found;
}
static void current_is(xrtownershipref producer,xrtownershipref frame)
{
    Edges edges=edges_of(producer);assert(edges.count==(frame.Data?1u:0u));
    if(frame.Data)assert(same(edges.items[0],frame));
}
typedef struct Resolve {xrtownershipref denied;bool omitProducer,omitWatch;} Resolve;
static bool resolve(xrtownershipref ref,void* data,const xrtownershipadapterv1** adapter,const xrtownershippreparationv1** preparation)
{
    Resolve* context=data;*adapter=NULL;*preparation=NULL;
    if(context&&same(ref,context->denied))return false;
    if((*adapter=xl6ContinuationFrameOwnershipAdapterV1(ref,preparation)))return true;
    const xfutureproducerownershipv1* producers[]={xl6ContinuationProducerPolicyV1Get()};
    const xfuturewatchownershipv1* watches[]={xl6ContinuationWatchPolicyV1Get()};
    const xfuturepayloadownershipv1* payloads[XL6_FUTURE_PAYLOAD_COUNT-XL6_FUTURE_PAYLOAD_VALUE];
    for(unsigned i=0;i<sizeof(payloads)/sizeof(*payloads);++i)payloads[i]=xl6PayloadPolicyGet((XL6_FuturePayloadKind)(XL6_FUTURE_PAYLOAD_VALUE+i));
    if((*adapter=xrtFutureOwnershipAdapterV3(ref,payloads,sizeof(payloads)/sizeof(*payloads),producers,context&&context->omitProducer?0:1,
        watches,context&&context->omitWatch?0:1,preparation)))return true;
    if((*adapter=xl6CodeLeaseOwnershipAdapterV1(ref)))return true;
    if((*adapter=xrtCancelOwnershipAdapterV1(ref)))return true;
    if((*adapter=xrtErrorOwnershipAdapterV1(ref)))return true;
    *adapter=xrtValueOwnershipAdapterV1(ref);return *adapter!=NULL;
}
static void complete(xpromise* promise,unsigned outcome)
{
    if(!outcome){int64_t* value=xrtMalloc(sizeof(*value));assert(value);*value=86;assert(xl6PayloadResolveOwned(promise,value,XL6_FUTURE_PAYLOAD_VALUE));}
    else if(outcome==1){xerror* error=xrtErrorCreate(XERR_STATE,"test",0,"source failed");assert(error&&xrtPromiseReject(promise,error));xrtErrorFree(error);}
    else if(outcome==2)assert(xrtPromiseClose(promise));
    else assert(xrtPromiseCancel(promise));
}
static XL6_ContinuationQueue* queue;static xfuture* sources[2];static void* observed[2];static unsigned observations,entered;
extern xfuture* async_managed_create(void);
extern void async_managed_prepare(void), async_managed_finish(void);
XL6_ContinuationQueue* async_managed_queue(void) {return queue;}
void async_managed_observe(void* frame) {assert(observations<2);observed[observations++]=frame;}
xfuture* async_storage_source(int64_t stage) {assert(stage==0||stage==1);return xrtFutureRef(sources[stage]);}
void async_storage_entered(void) {++entered;}
static void generated(unsigned outcome,bool ready,bool allocationSweep)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);xpromise* promises[2];observations=entered=0;
    queue=xl6ContinuationQueueCreate(1);assert(queue);
    for(unsigned i=0;i<2;++i){promises[i]=xrtPromiseCreate(&sources[i],NULL);assert(promises[i]);}
    if(ready)complete(promises[1],outcome==4?0:outcome);
    xfuture* output=async_managed_create();assert(output&&!observations&&!entered);
    assert(xl6ContinuationRunOne(queue)&&observations==1&&entered==1);
    xrtownershipref root=xrtFutureOwnership(output),first=api->Ownership(observed[0]);
    xrtownershipref producer=producer_of(output,first);assert(refs(producer)==2&&refs(first)==3);current_is(producer,first);
    /* Each physical child and each exact XRT policy must be admitted independently. */
    Resolve refused[4]={{first,false,false},{producer,false,false},{{0},true,false},{{0},false,true}};
    for(unsigned i=0;i<4;++i){XL6_OwnershipPlan* rejected=NULL;
        assert(xl6OwnershipPlanTryCreateV2(root,resolve,&refused[i],&rejected)==XL6_OWNERSHIP_PLAN_ERROR&&!rejected);
        ++denials;xrtClearError();assert(refs(producer)==2&&refs(first)==3);
    }
    if(allocationSweep)for(unsigned budget=0;budget<32;++budget){
        XL6_OwnershipPlan* probe=NULL;assert(xrtMemDebugFailAfter(budget));
        XL6_OwnershipPlanResult result=xl6OwnershipPlanTryCreateV2(root,resolve,NULL,&probe);
        bool failed=xrtMemDebugFailTriggered();xrtMemDebugFailClear();++oomAttempts;
        if(result==XL6_OWNERSHIP_PLAN_ERROR){assert(failed&&!probe);++trueOoms;}
        else{assert(result==XL6_OWNERSHIP_PLAN_ROOTED&&probe);assert(xl6OwnershipPlanTryDestroy(probe)==XL6_OWNERSHIP_PLAN_READY);}
        xrtClearError();assert(refs(producer)==2&&refs(first)==3);
    }
    XL6_OwnershipPlan* plan=NULL;
    assert(xl6OwnershipPlanTryCreateV2(root,resolve,NULL,&plan)==XL6_OWNERSHIP_PLAN_ROOTED&&plan);
    assert(refs(producer)==3&&refs(first)==4);xrtFutureDestroy(output); /* real Plan Hold now owns this observer */
    assert(xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_ROOTED&&!xl6ContinuationRunOne(queue));
    cross_thread();complete(promises[0],0);xl6ContinuationQueueClose(queue);
    assert(xl6ContinuationRunOne(queue)&&observations==2&&refs(first)==1);
    xrtownershipref second=api->Ownership(observed[1]);assert(!same(first,second));
    assert(same(producer_of(output,second),producer));current_is(producer,second);
    assert(refs(second)==(ready?2u:3u));
    assert(xl6OwnershipPlanTryRefresh(plan)==XL6_OWNERSHIP_PLAN_ROOTED&&refs(second)==(ready?3u:4u));
    assert(refs(first)==1&&refs(producer)==3); /* no retained predecessor chain */
    if(outcome==4)assert(xrtFutureCancel(output));
    if(!ready)complete(promises[1],outcome==4?0:outcome);
    assert(xl6ContinuationRunOne(queue)&&!xl6ContinuationRunOne(queue));
    assert(xrtFutureState(output)==(!outcome?XFUTURE_RESOLVED:outcome==1?XFUTURE_FAILED:outcome==2?XFUTURE_CLOSED:XFUTURE_CANCELLED));
    if(!outcome)assert(*(int64_t*)xrtFutureValue(output)==172);
    assert(refs(first)==1&&refs(second)==1&&refs(producer)==1);current_is(producer,(xrtownershipref){0});
    assert(xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_READY);
    cross_thread();assert(xl6OwnershipPlanFinish(plan)&&xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY);
    assert(xl6ContinuationQueueDestroy(queue));queue=NULL;
    for(unsigned i=0;i<2;++i){xrtPromiseDestroy(promises[i]);xrtFutureDestroy(sources[i]);sources[i]=NULL;}
    ++graphs;balance(&before);
}

typedef struct NativeFrame {unsigned step;XL6_ContinuationTicket ticket;xfuturewatch watch;xfuture* source;xpromise* outer;} NativeFrame;
static const XL6_ActivationCaptureSlot slots[]={{{XL6_CAPTURE_TRIVIAL,offsetof(NativeFrame,step),sizeof(unsigned)},SIZE_MAX}};
static const XL6_ActivationCaptureLayoutV1 captures={sizeof(captures),sizeof(NativeFrame),1,slots,0,NULL};
static const XL6_ContinuationFrameLayoutV2 layout={sizeof(layout),&captures,
    offsetof(NativeFrame,ticket),offsetof(NativeFrame,watch),offsetof(NativeFrame,source),offsetof(NativeFrame,outer)};
static NativeFrame* nativeCurrent;static xrtownershipref nativeProducer;static unsigned nativeRuns;
static NativeFrame* create_frame(unsigned step)
{NativeFrame* frame=api->Allocate(sizeof(*frame),&layout);assert(frame);frame->step=step;return frame;}
static void move_outer(NativeFrame* from,NativeFrame* to)
{
    xrtownershipscope mutation={0};assert(xrtOwnershipMutationBegin(&mutation)&&!to->outer&&from->outer);
    to->outer=from->outer;from->outer=NULL;assert(xrtOwnershipScopeEnd(&mutation));
}
static void publish(NativeFrame* frame)
{xl6ContinuationCommit(&frame->ticket);xl6ContinuationReady(&frame->ticket);xl6ContinuationTransportReleased(&frame->ticket);}
static void run_native(void* data)
{
    NativeFrame* frame=data;++nativeRuns;
    if(frame->step==64){complete(frame->outer,0);return;}
    NativeFrame* next=create_frame(frame->step+1);xfuture* wrong=NULL;
    next->outer=xrtPromiseCreate(&wrong,NULL);assert(next->outer);
    assert(xl6ContinuationReserveSuccessor(&frame->ticket,&next->ticket,run_native,api->Retire,next)==XL6_CONTINUATION_ADMITTED);
    assert(!produced->Bind(next)&&xrtGetError());++denials;xrtClearError();assert(xl6ContinuationAbort(&next->ticket));
    xrtPromiseDestroy(next->outer);next->outer=NULL;xrtFutureDestroy(wrong);
    move_outer(frame,next);
    assert(xl6ContinuationReserveSuccessor(&frame->ticket,&next->ticket,run_native,api->Retire,next)==XL6_CONTINUATION_ADMITTED&&produced->Bind(next));
    assert(xl6ContinuationAbort(&next->ticket));move_outer(next,frame);
    current_is(nativeProducer,api->Ownership(frame));assert(refs(nativeProducer)==2);
    move_outer(frame,next);
    assert(xl6ContinuationReserveSuccessor(&frame->ticket,&next->ticket,run_native,api->Retire,next)==XL6_CONTINUATION_ADMITTED&&produced->Bind(next));
    publish(next);current_is(nativeProducer,api->Ownership(next));nativeCurrent=next;++successorRetries;++steps;
}
static void chain(void)
{
    xmemdebugsnapshot before,steady,now;xrtMemDebugSnapshot(&before);nativeRuns=0;
    queue=xl6ContinuationQueueCreate(1);assert(queue);nativeCurrent=create_frame(0);xfuture* output=NULL;
    nativeCurrent->outer=xrtPromiseCreate(&output,NULL);assert(nativeCurrent->outer);
    assert(xl6ContinuationReserve(queue,&nativeCurrent->ticket,run_native,api->Retire,nativeCurrent)==XL6_CONTINUATION_ADMITTED&&produced->Bind(nativeCurrent));
    publish(nativeCurrent);nativeProducer=producer_of(output,api->Ownership(nativeCurrent));xl6ContinuationQueueClose(queue);
    xrtMemDebugSnapshot(&steady);
    for(unsigned i=0;i<64;++i){
        assert(xl6ContinuationRunOne(queue)&&nativeRuns==i+1);
        assert(refs(api->Ownership(nativeCurrent))==2&&refs(nativeProducer)==2);
        current_is(nativeProducer,api->Ownership(nativeCurrent));xrtMemDebugSnapshot(&now);
        assert(now.LiveCount==steady.LiveCount&&now.LiveBytes==steady.LiveBytes);
    }
    assert(xl6ContinuationRunOne(queue)&&nativeRuns==65&&!xl6ContinuationRunOne(queue));
    assert(xrtFutureState(output)==XFUTURE_RESOLVED&&xl6ContinuationQueueDestroy(queue));queue=NULL;
    xrtFutureDestroy(output);balance(&before);
}
static void root_retry(unsigned mode)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);NativeFrame* frame=create_frame(64);xfuture* output=NULL;
    queue=xl6ContinuationQueueCreate(1);assert(queue);frame->outer=xrtPromiseCreate(&output,NULL);assert(frame->outer);
    assert(xl6ContinuationReserve(queue,&frame->ticket,run_native,api->Retire,frame)==XL6_CONTINUATION_ADMITTED);
    if(mode==2){assert(xrtMemDebugFailAfter(0));assert(!produced->Bind(frame)&&xrtMemDebugFailTriggered());
        xrtMemDebugFailClear();xrtClearError();++trueOoms;++oomAttempts;}
    else assert(mode?produced->Bind(frame):api->Bind(frame));
    assert(xl6ContinuationAbort(&frame->ticket));xrtPromiseDestroy(frame->outer);frame->outer=NULL;xrtFutureDestroy(output);
    frame->outer=xrtPromiseCreate(&output,NULL);assert(frame->outer);
    assert(xl6ContinuationReserve(queue,&frame->ticket,run_native,api->Retire,frame)==XL6_CONTINUATION_ADMITTED);
    assert(mode?produced->Bind(frame):api->Bind(frame));publish(frame);xl6ContinuationQueueClose(queue);
    assert(xl6ContinuationRunOne(queue)&&xrtFutureState(output)==XFUTURE_RESOLVED&&xl6ContinuationQueueDestroy(queue));queue=NULL;
    xrtFutureDestroy(output);++rootRetries;balance(&before);
}
int main(void)
{
    xmemdebugsnapshot whole;
    assert(xrtMemDebugEnable(true));api=xl6ContinuationFrameRuntimeV2Get();produced=xl6ContinuationFrameRuntimeV3Get();
    xrtSetErrorKind(XERR_STATE);xrtClearError();
    xrtMemDebugSnapshot(&whole);async_managed_prepare();
    for(unsigned round=0;round<10;++round){
        for(unsigned outcome=0;outcome<5;++outcome)for(unsigned ready=0;ready<2;++ready)
            generated(outcome,ready!=0,outcome==0&&!ready);
        for(unsigned mode=0;mode<3;++mode)root_retry(mode);
        chain();
    }
    assert(graphs==100&&denials==1040&&rootRetries==30&&successorRetries==640&&steps==640&&freezes==200&&oomAttempts==330&&trueOoms>=20);
    async_managed_finish();balance(&whole);
    puts("Async producer graph: 100 generated pending/ready outcome graphs, 1040 exact policy/identity refusals, 30 root retries, 640 successor rollbacks and handoffs, 200 cross-thread freezes, 330 allocation budgets; stable producer, real Plan pins, constant live chain storage, exact memory balance");
    printf("Async producer OOM: %u actual allocation failures\n",trueOoms);return 0;
}
