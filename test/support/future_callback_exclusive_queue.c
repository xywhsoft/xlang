/* Real queued and inline transports, no network or generated callback code. */
#include "callback-modules.h"
#define XRT_MODULE_MEMORY_DEBUG
#define XRUNTIME_IMPLEMENTATION
#ifndef XRT_MODULE_WAIT
#define XRT_MODULE_WAIT
#endif
#include "lib/xruntime/xruntime_features.h"
#if defined(XRUNTIME_IMPLEMENTATION) && !defined(XRT_IMPLEMENTATION)
#define XRT_IMPLEMENTATION
#endif
#include "lib/xrt/xrt.h"
#include "lib/xruntime/xruntime.h"
#include "release/include/xl_clock.h"
#include "src/runtime/xl6_future_callback.h"
#include "src/runtime/xl6_continuation_executor.h"
#include "src/runtime/xl6_ownership_domains.h"
#include "src/runtime/xl6_ownership_catalog.h"
#include "src/api/xl_foreign_error.h"
#include <stdio.h>
#include <stdlib.h>
#define CHECK(C) do{if(!(C)){fprintf(stderr,"line %u: %s\n",(unsigned)__LINE__,#C);abort();}}while(0)
typedef struct State {xatomic32 calls,cleaned,requested;xatomic64 producer;xpromise* promise;bool queued,completeOnCancel;} State;
typedef struct Capture {State* state;xfuture* source;} Capture;
static const XL6_CaptureSlotV2 slots[]={
    {{XL6_CAPTURE_TRIVIAL,offsetof(Capture,state),sizeof(State*)},NULL},
    {{XL6_CAPTURE_FUTURE,offsetof(Capture,source),sizeof(xfuture*)},NULL}};
static const XL6_CaptureLayoutV2 layout={sizeof(layout),sizeof(Capture),2,slots};
static unsigned outcomes,requests,sharedControls,races,failures,prefixes;
static XL6_OwnershipCatalog* catalog;
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtClearError();xrtMemDebugSnapshot(&after);
    CHECK(before->LiveCount==after.LiveCount&&before->LiveBytes==after.LiveBytes);
    CHECK(after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount);
    CHECK(before->InvalidFreeCount==after.InvalidFreeCount&&before->DoubleFreeCount==after.DoubleFreeCount&&
        before->UseAfterFreeCount==after.UseAfterFreeCount&&before->OverflowCount==after.OverflowCount&&before->UnderflowCount==after.UnderflowCount);
}
static void idle(XL6_ContinuationExecutor* executor)
{
    XL6_DeadlineV1 until=xl6DeadlineAfterMillisecondsV1(5000);
    while(!xl6ContinuationExecutorIsIdle(executor)){CHECK(!xl6DeadlineExpiredV1(until));xrtThreadYield();}
}
static void finish(XL6_ContinuationExecutor* executor)
{
    if(!executor)return;
    xl6ContinuationExecutorClose(executor);XL6_DeadlineV1 until=xl6DeadlineAfterMillisecondsV1(5000);
    while(!xl6ContinuationExecutorDestroy(executor)){CHECK(!xl6DeadlineExpiredV1(until));xrtThreadYield();}
}
static void complete(xpromise* promise,unsigned outcome)
{
    switch(outcome){
        case 0:CHECK(xrtPromiseResolve(promise,NULL));break;
        case 1:xrtSetErrorKind(XERR_RANGE);CHECK(xrtPromiseReject(promise,xrtGetError()));xrtClearError();break;
        case 2:CHECK(xrtPromiseCancel(promise));break;
        case 3:CHECK(xrtPromiseClose(promise));break;
        default:abort();
    }
}
static void callback(const xfutureresult* input,xpromise* output,xcancel* cancel,void* raw)
{
    Capture* capture=raw;State* state=capture->state;CHECK(input&&input->State!=XFUTURE_PENDING);
    CHECK(state->queued==(xrtThreadCurrentId()!=xrtAtomic64Load(&state->producer,XMEMORY_ACQUIRE)));
    CHECK(xrtAtomic32FetchAdd(&state->calls,1,XMEMORY_ACQ_REL)==0);
    if(state->queued){xl6_error_result result=xl6FutureCallbackRuntimeV5Get()->Cleanup(raw,(xl6_error_result){true,NULL});CHECK(result.ok&&!result.error);}
    else xl6FutureCallbackRuntimeV4Get()->Cleanup(raw);
    CHECK(!capture->source);(void)xrtAtomic32FetchAdd(&state->cleaned,1,XMEMORY_RELEASE);
    if(xrtCancelRequested(cancel)||input->State==XFUTURE_CANCELLED){CHECK(xrtPromiseCancel(output));return;}
    if(input->State==XFUTURE_RESOLVED)CHECK(xrtPromiseResolve(output,NULL));
    else if(input->State==XFUTURE_FAILED)CHECK(xrtPromiseReject(output,input->Error));
    else CHECK(xrtPromiseClose(output));
}
static Capture* allocate(State* state,xfuture* source)
{
    Capture* result=state->queued?xl6FutureCallbackRuntimeV5Get()->Allocate(NULL,sizeof(*result),callback,&layout):
        xl6FutureCallbackRuntimeV4Get()->Allocate(NULL,sizeof(*result),callback,&layout);
    if(result){*result=(Capture){.state=state};result->source=xrtFutureRef(source);CHECK(result->source);}
    return result;
}
static xfuture* start(State* state,XL6_ContinuationExecutor* executor,Capture* capture,xfuture* source,bool exclusive)
{
    if(state->queued)return exclusive?xl6FutureCallbackRuntimeV6Get()->StartExclusive(executor,capture,source):
        xl6FutureCallbackRuntimeV5Get()->Start(executor,capture,source);
    return exclusive?xl6FutureCallbackRuntimeV3Get()->StartExclusive(capture,source):
        xl6FutureCallbackRuntimeV1Get()->Start(capture,source);
}
static State state_new(bool queued)
{
    State state={.queued=queued};xrtAtomic32Init(&state.calls,0);xrtAtomic32Init(&state.cleaned,0);
    xrtAtomic32Init(&state.requested,0);xrtAtomic64Init(&state.producer,xrtThreadCurrentId());return state;
}
static bool edge(xrtownershipref ref,void* raw){if(ref.Data){CHECK(ref.Ops);++*(unsigned*)raw;}return true;}
static void inspect(Capture* capture,bool queued)
{
    xrtownershipscope freeze={0};XL6_DeadlineV1 until=xl6DeadlineAfterMillisecondsV1(5000);
    while(!xrtOwnershipFreezeTryBegin(&freeze)){CHECK(!xl6DeadlineExpiredV1(until));xrtThreadYield();}
    xrtownershipref ref=xl6FutureCallbackRuntimeV1Get()->Ownership(capture);size_t count;unsigned edges=0;
    CHECK(ref.Ops->Count(ref.Data,&count)&&count==(queued?4u:3u));
    CHECK(ref.Ops->Trace(ref.Data,edge,&edges)&&edges==5);
    const xrtownershipadapterv1* adapter=NULL;const xrtownershippreparationv1* preparation=NULL;const xrtownershipfinishv1* completion=NULL;
    CHECK(xl6OwnershipCatalogResolveV1(ref,catalog,&adapter,&preparation,&completion)&&adapter&&preparation&&!completion);
    CHECK(adapter->Hold(ref.Data));CHECK(ref.Ops->Count(ref.Data,&count)&&count==(queued?5u:4u));
    CHECK(xrtOwnershipScopeEnd(&freeze));adapter->Drop(ref.Data);
}
static void terminal_cases(void)
{
    static const xfuturestate expected[]={XFUTURE_RESOLVED,XFUTURE_FAILED,XFUTURE_CANCELLED,XFUTURE_CLOSED};
    for(unsigned queued=0;queued<2;++queued)for(unsigned ready=0;ready<2;++ready)for(unsigned mode=0;mode<4;++mode)
    for(unsigned repeat=0;repeat<8;++repeat){
        xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);State state=state_new(queued!=0);
        XL6_ContinuationExecutor* executor=queued?xl6ContinuationExecutorCreateFixedV1(1,1):NULL;CHECK(!queued||executor);
        xfuture* source=NULL;xpromise* promise=xrtPromiseCreate(&source,NULL);CHECK(promise);
        if(ready)complete(promise,mode);
        Capture* capture=allocate(&state,source);CHECK(capture);
        xfuture* output=start(&state,executor,capture,source,true);CHECK(output);
        if(!ready){inspect(capture,queued!=0);complete(promise,mode);}
        CHECK(xrtFutureWaitFor(output,UINT64_C(5000000))==XWAIT_OK&&xrtFutureState(output)==expected[mode]);idle(executor);
        CHECK(xrtAtomic32Load(&state.calls,XMEMORY_ACQUIRE)==1&&xrtAtomic32Load(&state.cleaned,XMEMORY_ACQUIRE)==1);
        xrtFutureDestroy(output);xrtFutureDestroy(source);xrtPromiseDestroy(promise);finish(executor);balanced(&before);++outcomes;
    }
}
static void requested(void* raw)
{
    State* state=raw;(void)xrtAtomic32FetchAdd(&state->requested,1,XMEMORY_RELEASE);
    if(state->completeOnCancel)complete(state->promise,2);
}
static void cancellation(void)
{
    for(unsigned queued=0;queued<2;++queued)for(unsigned immediate=0;immediate<2;++immediate)for(unsigned repeat=0;repeat<16;++repeat){
        xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);State state=state_new(queued!=0);state.completeOnCancel=immediate!=0;
        XL6_ContinuationExecutor* executor=queued?xl6ContinuationExecutorCreateFixedV1(1,1):NULL;CHECK(!queued||executor);
        xfuture* source=NULL;state.promise=xrtPromiseCreate(&source,NULL);CHECK(state.promise);
        xcancel* cancel=xrtPromiseCancelToken(state.promise);CHECK(cancel);xcancelwatch* observer=xrtCancelWatch(cancel,requested,&state);CHECK(observer);
        xfuture* output=start(&state,executor,allocate(&state,source),source,true);CHECK(output);
        xrtSetErrorKind(XERR_STATE);const xerror* ambient=xrtGetError();CHECK(ambient);
        CHECK(xrtFutureCancel(output)&&xrtGetError()==ambient);xrtClearError();
        CHECK(xrtCancelRequested(cancel)&&xrtAtomic32Load(&state.requested,XMEMORY_ACQUIRE)==1);
        if(!immediate){CHECK(xrtFutureState(source)==XFUTURE_PENDING&&xrtFutureState(output)==XFUTURE_PENDING);
            CHECK(!xrtAtomic32Load(&state.calls,XMEMORY_ACQUIRE));complete(state.promise,0);}
        CHECK(xrtFutureWaitFor(output,UINT64_C(5000000))==XWAIT_OK&&xrtFutureState(output)==XFUTURE_CANCELLED);idle(executor);
        CHECK(xrtAtomic32Load(&state.calls,XMEMORY_ACQUIRE)==1&&xrtAtomic32Load(&state.cleaned,XMEMORY_ACQUIRE)==1);
        xrtCancelUnwatch(observer);xrtCancelDestroy(cancel);xrtFutureDestroy(output);xrtFutureDestroy(source);xrtPromiseDestroy(state.promise);
        finish(executor);balanced(&before);++requests;
    }
    for(unsigned queued=0;queued<2;++queued)for(unsigned repeat=0;repeat<16;++repeat){
        xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);State state=state_new(queued!=0);
        XL6_ContinuationExecutor* executor=queued?xl6ContinuationExecutorCreateFixedV1(1,1):NULL;CHECK(!queued||executor);
        xfuture* source=NULL;xpromise* promise=xrtPromiseCreate(&source,NULL);CHECK(promise);xcancel* cancel=xrtPromiseCancelToken(promise);CHECK(cancel);
        xfuture* output=start(&state,executor,allocate(&state,source),source,false);CHECK(output&&xrtFutureCancel(output));
        CHECK(!xrtCancelRequested(cancel)&&!xrtAtomic32Load(&state.calls,XMEMORY_ACQUIRE));complete(promise,0);
        CHECK(xrtFutureWaitFor(output,UINT64_C(5000000))==XWAIT_OK&&xrtFutureState(output)==XFUTURE_CANCELLED);idle(executor);
        xrtCancelDestroy(cancel);xrtFutureDestroy(output);xrtFutureDestroy(source);xrtPromiseDestroy(promise);finish(executor);balanced(&before);++sharedControls;
    }
}
static void construction(void)
{
    for(unsigned queued=0;queued<2;++queued)for(unsigned ready=0;ready<2;++ready)for(unsigned budget=0;budget<64;++budget){
        xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);State state=state_new(queued!=0);
        XL6_ContinuationExecutor* executor=queued?xl6ContinuationExecutorCreateFixedV1(1,1):NULL;CHECK(!queued||executor);
        xfuture* source=NULL;xpromise* promise=xrtPromiseCreate(&source,NULL);CHECK(promise);xcancel* cancel=xrtPromiseCancelToken(promise);CHECK(cancel);
        if(ready)complete(promise,0);
        Capture* capture=allocate(&state,source);CHECK(capture);
        CHECK(xrtMemDebugFailAfter(budget));xfuture* output=start(&state,executor,capture,source,true);
        bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();++prefixes;
        if(!output){CHECK(hit&&!xrtAtomic32Load(&state.calls,XMEMORY_ACQUIRE)&&!xrtCancelRequested(cancel));
            xl6FutureCallbackRuntimeV1Get()->Discard(capture);xrtClearError();++failures;}
        else{CHECK(!hit);if(!ready)complete(promise,0);
            CHECK(xrtFutureWaitFor(output,UINT64_C(5000000))==XWAIT_OK);idle(executor);xrtFutureDestroy(output);}
        xrtCancelDestroy(cancel);xrtFutureDestroy(source);xrtPromiseDestroy(promise);finish(executor);balanced(&before);
        if(!hit)break;
        CHECK(budget!=63);
    }
    for(unsigned closed=0;closed<2;++closed){
        xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);State state=state_new(true);
        XL6_ContinuationExecutor* executor=xl6ContinuationExecutorCreateFixedV1(1,1);CHECK(executor);
        xfuture* source=NULL;xpromise* promise=xrtPromiseCreate(&source,NULL);CHECK(promise);xcancel* cancel=xrtPromiseCancelToken(promise);CHECK(cancel);
        xfuture* accepted=start(&state,executor,allocate(&state,source),source,false);CHECK(accepted);
        if(closed)xl6ContinuationExecutorClose(executor);
        Capture* capture=allocate(&state,source);CHECK(capture&&!start(&state,executor,capture,source,true));
        CHECK(!xrtCancelRequested(cancel));xrtClearError();xl6FutureCallbackRuntimeV1Get()->Discard(capture);CHECK(!xrtCancelRequested(cancel));
        complete(promise,0);CHECK(xrtFutureWaitFor(accepted,UINT64_C(5000000))==XWAIT_OK);idle(executor);
        xrtFutureDestroy(accepted);xrtCancelDestroy(cancel);xrtFutureDestroy(source);xrtPromiseDestroy(promise);finish(executor);balanced(&before);
    }
}
typedef struct Race {State* state;xfuture* output;xatomic32 go;bool cancel;} Race;
static int32 race(void* raw)
{
    Race* r=raw;while(!xrtAtomic32Load(&r->go,XMEMORY_ACQUIRE))xrtThreadYield();
    if(r->cancel)(void)xrtFutureCancel(r->output);
    else{xrtAtomic64Store(&r->state->producer,xrtThreadCurrentId(),XMEMORY_RELEASE);complete(r->state->promise,0);}
    return 0;
}
static void racing(void)
{
    for(unsigned queued=0;queued<2;++queued)for(unsigned repeat=0;repeat<128;++repeat){
        xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);State state=state_new(queued!=0);
        XL6_ContinuationExecutor* executor=queued?xl6ContinuationExecutorCreateFixedV1(1,1):NULL;CHECK(!queued||executor);
        xfuture* source=NULL;state.promise=xrtPromiseCreate(&source,NULL);CHECK(state.promise);
        xfuture* output=start(&state,executor,allocate(&state,source),source,true);CHECK(output);
        Race a={.state=&state,.output=output,.cancel=true},b={.state=&state};xrtAtomic32Init(&a.go,0);xrtAtomic32Init(&b.go,0);
        xthread* threads[]={xrtThreadCreate(race,&a,0),xrtThreadCreate(race,&b,0)};CHECK(threads[0]&&threads[1]);
        xrtAtomic32Store(&a.go,1,XMEMORY_RELEASE);xrtAtomic32Store(&b.go,1,XMEMORY_RELEASE);
        for(size_t i=0;i<2;++i){CHECK(xrtThreadWaitFor(threads[i],UINT64_C(5000000))==XWAIT_OK&&xrtThreadExitCode(threads[i])==0);xrtThreadDestroy(threads[i]);}
        CHECK(xrtFutureWaitFor(output,UINT64_C(5000000))==XWAIT_OK);idle(executor);
        CHECK(xrtFutureState(output)==XFUTURE_RESOLVED||xrtFutureState(output)==XFUTURE_CANCELLED);
        CHECK(xrtAtomic32Load(&state.calls,XMEMORY_ACQUIRE)==1&&xrtAtomic32Load(&state.cleaned,XMEMORY_ACQUIRE)==1);
        xrtFutureDestroy(output);xrtFutureDestroy(source);xrtPromiseDestroy(state.promise);finish(executor);balanced(&before);++races;
    }
}
int main(void)
{
    CHECK(xrtMemDebugEnable(true));xl6_error_scope scope={0};CHECK(xl6ErrorScopeBegin(&scope));
    xl6_error_result result=xl6ErrorScopeEnd(&scope);CHECK(result.ok);xrtErrorFree(result.error);
    const XL6_OwnershipDomainV1* domains[]={
        #include "callback-domains.h"
    };catalog=xl6OwnershipCatalogCreateV1(domains,sizeof(domains)/sizeof(domains[0]));CHECK(catalog);
    terminal_cases();cancellation();construction();racing();xl6OwnershipCatalogDestroyV1(catalog);CHECK(xrtMemDebugReset());
    printf("exclusive callbacks: outcomes=%u cancellations=%u shared-controls=%u races=%u prefixes=%u failures=%u; actual queue/catalog/Watch refs, cooperative terminals, worker cleanup; balanced\n",
        outcomes,requests,sharedControls,races,prefixes,failures);return 0;
}
