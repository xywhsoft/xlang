#define XRT_MODULE_FUTURE
#define XRT_MODULE_VALUE
#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_THREAD
#ifndef XRT_MODULE_WAIT
#define XRT_MODULE_WAIT
#endif
#include "lib/xrt/xrt.h"
#include "release/include/xl_clock.h"
#include "src/runtime/xl6_future_callback.h"
#include "src/runtime/xl6_continuation_executor.h"
#include "src/api/xl_foreign_error.h"
#include <assert.h>
#include <stdatomic.h>
#include <stdio.h>
#ifdef TEST_PRODUCT_CATALOG
#include "src/runtime/xl6_ownership_domains.h"
#endif

typedef struct State {
    atomic_uint calls,cleaned,acquires,drops,handles;
    uint64_t producer;
    unsigned failureMode;
    xerror* primary;
} State;
typedef struct Payload {State* state;} Payload;
static const XL6_CaptureLayoutV2 layout={sizeof(layout),sizeof(Payload),0,NULL};
static int value=217;
static const XL6_FutureCallbackRuntimeV5* api;
typedef struct FailurePayload {State* state;xvalue* item;} FailurePayload;
typedef struct FailingData {State* state;bool diagnostic;} FailingData;
static const XL6_CaptureSlotV2 failureSlot={
    {XL6_CAPTURE_XVALUE,offsetof(FailurePayload,item),sizeof(xvalue*)},NULL};
static const XL6_CaptureLayoutV2 failureLayout={sizeof(failureLayout),sizeof(FailurePayload),1,&failureSlot};
static void failing_drop(void* value,void* context)
{
    FailingData* box=value;assert(!context);State* state=box->state;bool diagnostic=box->diagnostic;
    atomic_fetch_add(&state->handles,1);xrtFree(box);
    if(diagnostic)xrtSetErrorKind(XERR_RANGE);
    else xl6ErrorScopePropagateFailure((xl6_error_result){false,NULL});
}
static const xvaluehandleops failingOps={NULL,failing_drop,NULL,NULL};
static void failure_callback(const xfutureresult* input,xpromise* output,xcancel* cancel,void* data)
{
    (void)cancel;FailurePayload* payload=data;State* state=payload->state;assert(input&&input->State==XFUTURE_RESOLVED);
    assert(xrtThreadCurrentId()!=state->producer);atomic_fetch_add(&state->calls,1);
    xl6_error_result incoming={state->failureMode==0||state->failureMode==3,
        state->failureMode==2?xrtErrorRef(state->primary):NULL};
    xl6_error_result outcome=api->Cleanup(data,incoming);assert(!outcome.ok&&!payload->item);
    if(state->failureMode<2)assert(!outcome.error); /* includes first false/NULL before later RangeError */
    else if(state->failureMode==2)assert(outcome.error==state->primary);
    else assert(outcome.error&&xrtErrorKind(outcome.error)==XERR_RANGE);
    atomic_fetch_add(&state->cleaned,1);api->Fail(output,outcome.error);
}
static int acquire(void* data) {atomic_fetch_add(&((State*)data)->acquires,1);return 1;}
static void owner_drop(void* data) {atomic_fetch_add_explicit(&((State*)data)->drops,1,memory_order_release);}
static void callback(const xfutureresult* input,xpromise* output,xcancel* cancel,void* data)
{
    Payload* payload=data;State* state=payload->state;
    assert(input&&output&&xrtThreadCurrentId()!=state->producer&&!xrtGetError());
    atomic_fetch_add(&state->calls,1);
    xl6_error_result cleaned=api->Cleanup(data,(xl6_error_result){true,NULL});assert(cleaned.ok&&!cleaned.error);
    atomic_fetch_add_explicit(&state->cleaned,1,memory_order_release);
    if(xrtCancelRequested(cancel)){(void)xrtPromiseCancel(output);return;}
    switch(input->State){
    case XFUTURE_RESOLVED:assert(input->Value==&value&&xrtPromiseResolve(output,&value));break;
    case XFUTURE_FAILED:assert(input->Error&&xrtPromiseReject(output,input->Error));break;
    case XFUTURE_CANCELLED:assert(xrtPromiseCancel(output));break;
    case XFUTURE_CLOSED:assert(xrtPromiseClose(output));break;
    default:assert(0);
    }
}
static void finish(XL6_ContinuationExecutor* executor)
{
    XL6_DeadlineV1 deadline=xl6DeadlineAfterMillisecondsV1(10000);xl6ContinuationExecutorClose(executor);
    while(!xl6ContinuationExecutorDestroy(executor)){assert(!xl6DeadlineExpiredV1(deadline));xrtThreadYield();}
}
static void await_idle(XL6_ContinuationExecutor* executor)
{
    XL6_DeadlineV1 deadline=xl6DeadlineAfterMillisecondsV1(10000);
    while(!xl6ContinuationExecutorIsIdle(executor)){assert(!xl6DeadlineExpiredV1(deadline));xrtThreadYield();}
}
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtClearError();xrtMemDebugSnapshot(&after);
    assert(before->LiveCount==after.LiveCount&&before->LiveBytes==after.LiveBytes);
    assert(after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount);
    assert(before->InvalidFreeCount==after.InvalidFreeCount&&before->DoubleFreeCount==after.DoubleFreeCount&&
        before->UseAfterFreeCount==after.UseAfterFreeCount);
}
static void complete(xpromise* promise,unsigned mode,xerror* failure)
{
    assert(mode==0?xrtPromiseResolve(promise,&value):mode==1?xrtPromiseReject(promise,failure):
        mode==2?xrtPromiseCancel(promise):xrtPromiseClose(promise));
}
typedef struct CompletionRace {
    State* state;xpromise* promise;xerror* failure;unsigned mode;
    atomic_bool entered,go;
} CompletionRace;
static int32_t complete_racing(void* data)
{
    CompletionRace* race=data;race->state->producer=xrtThreadCurrentId();
    atomic_store_explicit(&race->entered,true,memory_order_release);
    while(!atomic_load_explicit(&race->go,memory_order_acquire))xrtThreadYield();
    complete(race->promise,race->mode,race->failure);return 0;
}
static Payload* create(State* state,bool owned)
{
    XL6_CodeOwner owner={sizeof(owner),state,acquire,owner_drop};
    Payload* payload=api->Allocate(owned?&owner:NULL,sizeof(*payload),callback,&layout);
    if(payload)*payload=(Payload){state};
    return payload;
}

#ifdef TEST_PRODUCT_CATALOG
static unsigned policyObservations;
static bool observe_policies(xrtownershipref ref,const XL6_OwnershipPoliciesV1* policies,
    const xrtownershipadapterv1** adapter,const xrtownershippreparationv1** preparation,
    const XL6_OwnershipFinishV1** completion)
{
    (void)ref;(void)adapter;(void)preparation;(void)completion;
    assert(policies->ProducerCount==1&&policies->Producer[0]==xl6FutureCallbackProducerPolicyV1Get());
    assert(policies->WatchCount==2&&policies->QueuedStorageCount==1);
    assert(policies->QueuedStorage[0]==xl6FutureCallbackQueuedStorageV2Get());
    bool queued=false;
    for(size_t i=0;i<policies->WatchCount;++i)
        if(policies->Watch[i]==xl6FutureCallbackQueuedWatchPolicyV1Get())queued=true;
    assert(queued);++policyObservations;return false;
}
static void catalog_contract(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);State state={0};state.producer=xrtThreadCurrentId();
    XL6_ContinuationExecutor* executor=xl6ContinuationExecutorCreateFixedV1(1,1);assert(executor);
    xfuture* source=NULL;xpromise* promise=xrtPromiseCreate(&source,NULL);assert(promise);
    Payload* payload=create(&state,false);xfuture* output=api->Start(executor,payload,source);assert(output);
    const XL6_OwnershipDomainV1 observer={sizeof(observer),NULL,observe_policies,NULL};
    const XL6_OwnershipDomainV1* domains[]={&observer,xl6OwnershipDomainFutureCallbackV1Get(),
        xl6OwnershipDomainFutureCallbackQueueV1Get()};
    XL6_OwnershipCatalog* catalog=xl6OwnershipCatalogCreateV1(domains,3);assert(catalog);
    const xrtownershipadapterv1* adapter=NULL;const xrtownershippreparationv1* preparation=NULL;
    const XL6_OwnershipFinishV1* completion=NULL;xmemdebugsnapshot frozenBefore,frozenAfter;
    xrtMemDebugSnapshot(&frozenBefore);xrtownershipscope freeze={0};assert(xrtOwnershipFreezeTryBegin(&freeze));
    xrtownershipref ref=xl6FutureCallbackRuntimeV1Get()->Ownership(payload);
    assert(xl6OwnershipCatalogResolveV1(ref,catalog,&adapter,&preparation,&completion));
    assert(adapter&&preparation&&!completion&&policyObservations==1);
    assert(xrtOwnershipScopeEnd(&freeze));xrtMemDebugSnapshot(&frozenAfter);
    assert(frozenBefore.AllocCount==frozenAfter.AllocCount&&frozenBefore.FreeCount==frozenAfter.FreeCount);
    xl6OwnershipCatalogDestroyV1(catalog);complete(promise,0,NULL);await_idle(executor);
    xrtFutureDestroy(output);xrtFutureDestroy(source);xrtPromiseDestroy(promise);finish(executor);balanced(&before);
}
#endif
typedef struct Edges {size_t count;} Edges;
static bool edge(xrtownershipref ref,void* data)
{if(ref.Data){assert(ref.Ops);++((Edges*)data)->count;}return true;}
static const xrtownershipadapterv1* inspect(Payload* payload,size_t expected)
{
    xrtownershipscope freeze={0};XL6_DeadlineV1 deadline=xl6DeadlineAfterMillisecondsV1(10000);
    while(!xrtOwnershipFreezeTryBegin(&freeze)){assert(!xl6DeadlineExpiredV1(deadline));xrtThreadYield();}
    xrtownershipref ref=xl6FutureCallbackRuntimeV1Get()->Ownership(payload);size_t count;
    assert(ref.Ops->Count(ref.Data,&count)&&count==expected);
    const xrtownershippreparationv1* preparation=NULL;
    const xrtownershipadapterv1* adapter=xl6FutureCallbackOwnershipAdapterV1(ref,&preparation);
    assert(adapter&&preparation);Edges edges={0};assert(ref.Ops->Trace(ref.Data,edge,&edges)&&edges.count==3);
    /* A real pin, not invented incoming-reference arithmetic. */
    assert(adapter->Hold(ref.Data));assert(ref.Ops->Count(ref.Data,&count)&&count==expected+1);
    assert(xrtOwnershipScopeEnd(&freeze));return adapter;
}
int main(void)
{
    static const xfuturestate states[]={XFUTURE_RESOLVED,XFUTURE_FAILED,XFUTURE_CANCELLED,XFUTURE_CLOSED};
    api=xl6FutureCallbackRuntimeV5Get();assert(api&&api->Allocate&&api->Start&&api->Cleanup);
    assert(xrtMemDebugEnable(true));unsigned prefixes=0;
#ifdef TEST_PRODUCT_CATALOG
    catalog_contract();
#endif
    for(unsigned round=0;round<200;++round){
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);State state={0};
        XL6_ContinuationExecutor* executor=xl6ContinuationExecutorCreateFixedV1(1,1);assert(executor);
        xfuture* source=NULL;xpromise* promise=xrtPromiseCreate(&source,NULL);assert(promise);
        xerror* failure=xrtErrorCreate(XERR_STATE,"callback-race",73,"source race");assert(failure);
        CompletionRace race={.state=&state,.promise=promise,.failure=failure,.mode=round%4};
        Payload* payload=create(&state,true);assert(payload);
        xthread* producer=xrtThreadCreate(complete_racing,&race,0);assert(producer);
        XL6_DeadlineV1 deadline=xl6DeadlineAfterMillisecondsV1(10000);
        while(!atomic_load_explicit(&race.entered,memory_order_acquire)){
            assert(!xl6DeadlineExpiredV1(deadline));xrtThreadYield();
        }
        atomic_store_explicit(&race.go,true,memory_order_release);
        xfuture* output=api->Start(executor,payload,source);assert(output);
        assert(xrtThreadWait(producer)==XWAIT_OK);xrtThreadDestroy(producer);
        assert(xrtFutureWaitFor(output,10000000)==XWAIT_OK&&xrtFutureState(output)==states[round%4]);
        await_idle(executor);
        assert(atomic_load(&state.calls)==1&&atomic_load(&state.cleaned)==1&&atomic_load(&state.drops)==1);
        xrtFutureDestroy(output);xrtFutureDestroy(source);xrtPromiseDestroy(promise);xrtErrorFree(failure);
        finish(executor);balanced(&before);
    }
    for(unsigned mode=0;mode<4;++mode)for(unsigned ready=0;ready<2;++ready)for(unsigned owned=0;owned<2;++owned)
    for(unsigned round=0;round<20;++round){
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);State state={0};state.producer=xrtThreadCurrentId();
        XL6_ContinuationExecutor* executor=xl6ContinuationExecutorCreateFixedV1(2,1);assert(executor);
        xfuture* source=NULL;xpromise* promise=xrtPromiseCreate(&source,NULL);assert(promise);
        xerror* failure=xrtErrorCreate(XERR_STATE,"callback-source",73,"source failure");assert(failure);
        if(ready)complete(promise,mode,failure);
        Payload* payload=create(&state,owned);assert(payload);xfuture* output=api->Start(executor,payload,source);assert(output);
        const xrtownershipadapterv1* pin=NULL;xrtownershipref ref={0};
        if(!ready&&!owned){ref=xl6FutureCallbackRuntimeV1Get()->Ownership(payload);pin=inspect(payload,3);}
        xl6ContinuationExecutorClose(executor); /* accepted callback must run */
        if(!ready){
            xrtSetErrorTake(xrtErrorRef(failure));assert(xrtMemDebugFailAfter(0));complete(promise,mode,failure);
            assert(xrtGetError()==failure);xrtClearError();
        }
        assert(xrtFutureWaitFor(output,10000000)==XWAIT_OK&&xrtFutureState(output)==states[mode]);
        await_idle(executor);
        if(!ready){assert(!xrtMemDebugFailTriggered());xrtMemDebugFailClear();}
        assert(atomic_load(&state.calls)==1&&atomic_load(&state.cleaned)==1);
        if(pin){
            xrtownershipscope freeze={0};XL6_DeadlineV1 deadline=xl6DeadlineAfterMillisecondsV1(10000);size_t count;
            while(!xrtOwnershipFreezeTryBegin(&freeze)){assert(!xl6DeadlineExpiredV1(deadline));xrtThreadYield();}
            assert(ref.Ops->Count(ref.Data,&count)&&count==1);assert(xrtOwnershipScopeEnd(&freeze));pin->Drop(ref.Data);
        }
        xrtFutureDestroy(output);xrtFutureDestroy(source);xrtPromiseDestroy(promise);xrtErrorFree(failure);finish(executor);
        assert(atomic_load(&state.acquires)==owned&&atomic_load(&state.drops)==owned);balanced(&before);
    }
    for(unsigned ready=0;ready<2;++ready)for(unsigned budget=0;budget<128;++budget){
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);State state={0};state.producer=xrtThreadCurrentId();
        XL6_ContinuationExecutor* executor=xl6ContinuationExecutorCreateFixedV1(2,1);assert(executor);
        xfuture* source=NULL;xpromise* promise=xrtPromiseCreate(&source,NULL);assert(promise);
        if(ready)complete(promise,0,NULL);
        assert(xrtMemDebugFailAfter(budget));Payload* payload=create(&state,true);
        xfuture* output=payload?api->Start(executor,payload,source):NULL;
        bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
        if(!output){
            assert(hit&&!atomic_load(&state.calls));++prefixes;
            if(payload)xl6FutureCallbackRuntimeV1Get()->Discard(payload);
            xrtClearError();
        }else{
            assert(!hit);if(!ready)complete(promise,0,NULL);
            assert(xrtFutureWaitFor(output,10000000)==XWAIT_OK);await_idle(executor);
            assert(atomic_load(&state.calls)==1&&atomic_load(&state.cleaned)==1);xrtFutureDestroy(output);
        }
        xrtFutureDestroy(source);xrtPromiseDestroy(promise);finish(executor);
        assert(atomic_load(&state.acquires)==atomic_load(&state.drops));balanced(&before);
        if(!hit)break;
        assert(budget!=127);
    }
    /* Fixed capacity/closed executor reject before Watch publication, without
     * losing a private capture or consuming another accepted callback. */
    for(unsigned closed=0;closed<2;++closed){
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);State a={0},b={0};a.producer=b.producer=xrtThreadCurrentId();
        XL6_ContinuationExecutor* executor=xl6ContinuationExecutorCreateFixedV1(1,1);assert(executor);
        xfuture* source=NULL;xpromise* promise=xrtPromiseCreate(&source,NULL);assert(promise);
        xfuture* output=api->Start(executor,create(&a,true),source);assert(output);
        if(closed)xl6ContinuationExecutorClose(executor);
        Payload* rejected=create(&b,true);assert(rejected&&!api->Start(executor,rejected,source));
        assert(xrtGetError()&&!atomic_load(&b.calls));xrtClearError();xl6FutureCallbackRuntimeV1Get()->Discard(rejected);
        complete(promise,0,NULL);assert(xrtFutureWaitFor(output,10000000)==XWAIT_OK);await_idle(executor);
        xrtFutureDestroy(output);xrtFutureDestroy(source);xrtPromiseDestroy(promise);finish(executor);
        assert(atomic_load(&a.acquires)==atomic_load(&a.drops)&&atomic_load(&b.acquires)==atomic_load(&b.drops));balanced(&before);
    }
    for(unsigned mode=0;mode<4;++mode){
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);State state={0};state.producer=xrtThreadCurrentId();state.failureMode=mode;
        XL6_ContinuationExecutor* executor=xl6ContinuationExecutorCreateFixedV1(1,1);assert(executor);
        xfuture* source=NULL;xpromise* promise=xrtPromiseCreate(&source,NULL);assert(promise);
        state.primary=xrtErrorCreate(XERR_STATE,"first-failure",73,"primary");assert(state.primary);
        FailurePayload* payload=api->Allocate(NULL,sizeof(*payload),failure_callback,&failureLayout);assert(payload);
        *payload=(FailurePayload){.state=&state};FailingData* box=xrtMalloc(sizeof(*box));assert(box);
        *box=(FailingData){&state,mode!=0};payload->item=xrtValueHandleTake((void**)&box,&failingOps,NULL);assert(payload->item&&!box);
        xfuture* output=api->Start(executor,payload,source);assert(output);assert(xrtMemDebugFailAfter(0));complete(promise,0,NULL);
        assert(xrtFutureWaitFor(output,10000000)==XWAIT_OK);await_idle(executor);xfutureresult result={0};
        assert(!xrtMemDebugFailTriggered()&&xrtFutureResult(output,&result)&&result.State==XFUTURE_FAILED&&result.Error);
        assert(xrtErrorKind(result.Error)==(mode<2?XERR_INTERNAL:mode==2?XERR_STATE:XERR_RANGE));
        if(mode==2)assert(result.Error==state.primary&&xrtErrorCode(result.Error)==73);
        xrtMemDebugFailClear();assert(atomic_load(&state.calls)==1&&atomic_load(&state.cleaned)==1&&atomic_load(&state.handles)==1);
        xrtFutureDestroy(output);xrtFutureDestroy(source);xrtPromiseDestroy(promise);xrtErrorFree(state.primary);finish(executor);balanced(&before);
    }
    for(unsigned round=0;round<50;++round){
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);State state={0};state.producer=xrtThreadCurrentId();
        XL6_ContinuationExecutor* executor=xl6ContinuationExecutorCreateFixedV1(1,1);assert(executor);
        xfuture* source=NULL;xpromise* promise=xrtPromiseCreate(&source,NULL);assert(promise);
        xfuture* output=api->Start(executor,create(&state,true),source);assert(output&&xrtFutureCancel(output));
        assert(xrtFutureState(source)==XFUTURE_PENDING&&!atomic_load(&state.calls)&&!atomic_load(&state.drops));
        complete(promise,0,NULL);await_idle(executor);
        assert(xrtFutureState(output)==XFUTURE_CANCELLED&&atomic_load(&state.calls)==1&&atomic_load(&state.cleaned)==1&&atomic_load(&state.drops)==1);
        xrtFutureDestroy(output);xrtFutureDestroy(source);xrtPromiseDestroy(promise);finish(executor);balanced(&before);
    }
    {
        enum {COUNT=1024};xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);State state={0};state.producer=xrtThreadCurrentId();
        XL6_ContinuationExecutor* executor=xl6ContinuationExecutorCreateFixedV1(COUNT,1);assert(executor);
        xfuture* source=NULL;xpromise* promise=xrtPromiseCreate(&source,NULL);assert(promise);
        xfuture** outputs=xrtCalloc(COUNT,sizeof(*outputs));assert(outputs);
        for(size_t i=0;i<COUNT;++i){outputs[i]=api->Start(executor,create(&state,true),source);assert(outputs[i]);}
        assert(!atomic_load(&state.calls));xl6ContinuationExecutorClose(executor);
        assert(xrtMemDebugFailAfter(0));complete(promise,0,NULL);await_idle(executor);assert(!xrtMemDebugFailTriggered());xrtMemDebugFailClear();
        assert(atomic_load(&state.calls)==COUNT&&atomic_load(&state.cleaned)==COUNT&&atomic_load(&state.drops)==COUNT);
        for(size_t i=0;i<COUNT;++i){assert(xrtFutureState(outputs[i])==XFUTURE_RESOLVED);xrtFutureDestroy(outputs[i]);}
        xrtFree(outputs);xrtFutureDestroy(source);xrtPromiseDestroy(promise);finish(executor);balanced(&before);
    }
    printf("queued Future callbacks: 320 terminal/code-owner cases, 200 concurrent registration/completion races, 1024 shared waits, 50 output cancellations, 4 checked cleanup/first-error cases including false-NULL, exact frame/queue/Watch pins, zero late allocation, %u full OOM prefixes, capacity/Close rollback; balanced memory\n",prefixes);
    return 0;
}
