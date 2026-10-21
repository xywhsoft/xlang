/* Reuse the independently asserted V1-6 baseline, actual native implementation,
 * queue and catalog. Tests here are offline; no network is selected or used. */
#define main sharedCallbackRegressionMain
#include "future_callback_exclusive_queue.c"
#undef main
#include "src/runtime/xl6_future_callback_interrupt.h"

static unsigned interrupted,constructionPrefixes,constructionFailures,selectedRaces;
static XL6_OwnershipCatalog* missingInterruptCatalog;
typedef struct Gate {XL6_ContinuationTicket ticket;xatomic32 entered,open;} Gate;
static void gate_run(void* data)
{
    Gate* gate=data;xrtAtomic32Store(&gate->entered,1,XMEMORY_RELEASE);
    xdeadline until=xrtDeadlineAfter(UINT64_C(10000000));
    while(!xrtAtomic32Load(&gate->open,XMEMORY_ACQUIRE)){CHECK(!xrtDeadlineExpired(until));xrtThreadYield();}
}
static void gate_drop(void* data){(void)data;}
static void gate_start(XL6_ContinuationExecutor* executor,Gate* gate)
{
    *gate=(Gate){0};xrtAtomic32Init(&gate->entered,0);xrtAtomic32Init(&gate->open,0);
    CHECK(xl6ContinuationExecutorReserve(executor,&gate->ticket,gate_run,gate_drop,gate)==XL6_CONTINUATION_ADMITTED);
    xl6ContinuationCommit(&gate->ticket);xl6ContinuationReady(&gate->ticket);xl6ContinuationTransportReleased(&gate->ticket);
    xdeadline until=xrtDeadlineAfter(UINT64_C(5000000));
    while(!xrtAtomic32Load(&gate->entered,XMEMORY_ACQUIRE)){CHECK(!xrtDeadlineExpired(until));xrtThreadYield();}
}
static void gate_open(Gate* gate){xrtAtomic32Store(&gate->open,1,XMEMORY_RELEASE);}
static void service_finish(XL6_DeadlineServiceV1* service)
{
    xl6DeadlineServiceCloseV1(service);CHECK(xl6DeadlineServiceDestroyCompletedV1(service));
}
typedef struct Edges {unsigned count,refused;bool busy;XL6_OwnershipCatalog* catalog;} Edges;
static bool certified_edge(xrtownershipref ref,void* data)
{
    Edges* edges=data;if(!ref.Data)return true;
    const xrtownershipadapterv1* adapter=NULL;const xrtownershippreparationv1* prepare=NULL;const XL6_OwnershipFinishV1* completion=NULL;
    XL6_OwnershipAdmissionV1 status=xl6OwnershipCatalogProbeV1(ref,edges->catalog,&adapter,&prepare,&completion);
    if(status==XL6_OWNERSHIP_ADMISSION_REFUSED){++edges->refused;
        if(edges->catalog==catalog)fprintf(stderr,"uncertified child edge %u\n",edges->count);}
    else if(status==XL6_OWNERSHIP_ADMISSION_BUSY)edges->busy=true;
    else{size_t count=0;CHECK(adapter&&ref.Ops->Count(ref.Data,&count)&&count);}
    ++edges->count;return true;
}
static void inspect_interrupt(Capture* capture)
{
    xdeadline until=xrtDeadlineAfter(UINT64_C(5000000));
    for(;;){xrtownershipscope frozen={0};CHECK(!xrtDeadlineExpired(until));
        if(!xrtOwnershipFreezeTryBegin(&frozen)){xrtThreadYield();continue;}
        xrtownershipref ref=xl6FutureCallbackRuntimeV1Get()->Ownership(capture);size_t count=0;
        CHECK(ref.Ops->Count(ref.Data,&count)&&count==6);
        Edges edges={.catalog=catalog},missing={.catalog=missingInterruptCatalog};
        CHECK(ref.Ops->Trace(ref.Data,certified_edge,&edges)&&edges.count==7&&!edges.refused);
        CHECK(ref.Ops->Trace(ref.Data,certified_edge,&missing)&&missing.count==7&&missing.refused==1);
        const xrtownershipadapterv1* adapter=NULL;const xrtownershippreparationv1* prepare=NULL;const XL6_OwnershipFinishV1* completion=NULL;
        CHECK(xl6OwnershipCatalogResolveV1(ref,catalog,&adapter,&prepare,&completion)&&adapter&&prepare&&!completion);
        CHECK(xrtOwnershipScopeEnd(&frozen));if(!edges.busy&&!missing.busy)break;xrtThreadYield();
    }
}
static void interrupted_cases(void)
{
    /* 0 pre-cancel, 1 expired before work, 2 live parent, 3 live timeout,
     * 4 native-success then parent, 5 native-success then deadline,
     * 6 selected native result cannot subsequently be revoked. */
    for(unsigned mode=0;mode<7;++mode)for(unsigned repeat=0;repeat<16;++repeat){
        xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);State state=state_new(true);
        XL6_ContinuationExecutor* executor=xl6ContinuationExecutorCreateFixedV1(4,1);CHECK(executor);
        XL6_DeadlineServiceV1* service=xl6DeadlineServiceCreateV1(4);CHECK(service);
        xcancel* parent=xrtCancelCreate();CHECK(parent);
        xfuture* source=NULL;state.promise=xrtPromiseCreate(&source,NULL);CHECK(state.promise);
        xcancel* sourceCancel=xrtPromiseCancelToken(state.promise);CHECK(sourceCancel);
        xcancelwatch* observer=xrtCancelWatch(sourceCancel,requested,&state);CHECK(observer);
        if(mode==0)CHECK(xrtCancelRequest(parent));
        xdeadline due=mode==1?xrtClock():mode==3||mode==5?xrtDeadlineAfter(UINT64_C(100000)):xrtDeadlineAfter(UINT64_C(5000000));
        Capture* capture=allocate(&state,source);CHECK(capture);
        CHECK(xl6FutureCallbackInterruptArmV1(capture,parent,service,due));
        bool stopped=xl6FutureCallbackInterruptStoppedV1(capture);CHECK(stopped==(mode<2));
        Gate gate;bool gated=mode==4||mode==5;
        if(gated)gate_start(executor,&gate);
        xfuture* output=xl6FutureCallbackInterruptStartV1(executor,capture,stopped?NULL:source);CHECK(output);
        if(mode==2){
            /* The borrowed C-stack diagnostic observer intentionally has no
             * ownership certificate. Remove it for the closed graph proof,
             * then restore it for cancellation request accounting. */
            xrtCancelUnwatch(observer);inspect_interrupt(capture);
            observer=xrtCancelWatch(sourceCancel,requested,&state);CHECK(observer&&xrtCancelRequest(parent));
        }
        if(mode==3){
            xdeadline until=xrtDeadlineAfter(UINT64_C(5000000));
            while(!xrtAtomic32Load(&state.requested,XMEMORY_ACQUIRE)){CHECK(!xrtDeadlineExpired(until));xrtThreadYield();}
        }
        if(mode==2||mode==3){
            CHECK(xrtCancelRequested(sourceCancel)&&xrtAtomic32Load(&state.requested,XMEMORY_ACQUIRE)==1);
            CHECK(xrtFutureState(output)==XFUTURE_PENDING&&!xrtAtomic32Load(&state.calls,XMEMORY_ACQUIRE));
        }
        if(!stopped)complete(state.promise,0);
        if(mode==4)CHECK(xrtCancelRequest(parent));
        if(mode==5){xdeadline until=xrtDeadlineAfter(UINT64_C(5000000));
            while(!xrtDeadlineExpired(due)){CHECK(!xrtDeadlineExpired(until));xrtThreadYield();}}
        if(gated)gate_open(&gate);
        CHECK(xrtFutureWaitFor(output,UINT64_C(5000000))==XWAIT_OK);idle(executor);
        xfuturestate expected=mode==1||mode==3||mode==5?XFUTURE_FAILED:mode==6?XFUTURE_RESOLVED:XFUTURE_CANCELLED;
        CHECK(xrtFutureState(output)==expected);
        if(expected==XFUTURE_FAILED){xfutureresult result={0};CHECK(xrtFutureResult(output,&result)&&xrtErrorKind(result.Error)==XERR_TIMEOUT);}
        CHECK(xrtAtomic32Load(&state.calls,XMEMORY_ACQUIRE)==1&&xrtAtomic32Load(&state.cleaned,XMEMORY_ACQUIRE)==1);
        if(stopped){CHECK(xrtFutureState(source)==XFUTURE_PENDING&&!xrtCancelRequested(sourceCancel));complete(state.promise,0);}
        if(mode==6){CHECK(xrtCancelRequest(parent)&&xrtFutureState(output)==XFUTURE_RESOLVED&&!xrtCancelRequested(sourceCancel));}
        xrtCancelUnwatch(observer);xrtCancelDestroy(sourceCancel);xrtCancelDestroy(parent);xrtFutureDestroy(output);
        xrtFutureDestroy(source);xrtPromiseDestroy(state.promise);finish(executor);service_finish(service);balanced(&before);++interrupted;
    }
}
static void construction_interrupt(void)
{
    for(unsigned ready=0;ready<2;++ready)for(unsigned budget=0;budget<48;++budget){
        xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);State state=state_new(true);
        XL6_ContinuationExecutor* executor=xl6ContinuationExecutorCreateFixedV1(4,1);CHECK(executor);
        XL6_DeadlineServiceV1* service=xl6DeadlineServiceCreateV1(4);CHECK(service);xcancel* parent=xrtCancelCreate();CHECK(parent);
        xfuture* source=NULL;state.promise=xrtPromiseCreate(&source,NULL);CHECK(state.promise);
        if(ready)CHECK(xrtCancelRequest(parent));
        Capture* capture=allocate(&state,source);CHECK(capture);CHECK(xrtMemDebugFailAfter(budget));++constructionPrefixes;
        bool armed=xl6FutureCallbackInterruptArmV1(capture,parent,service,xrtDeadlineAfter(UINT64_C(5000000)));
        xfuture* output=armed?xl6FutureCallbackInterruptStartV1(executor,capture,ready?NULL:source):NULL;
        bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
        if(!output){CHECK(hit);xl6FutureCallbackRuntimeV1Get()->Discard(capture);xrtClearError();++constructionFailures;}
        if(!ready)complete(state.promise,0);
        else if(!output)complete(state.promise,0);
        if(output){CHECK(xrtFutureWaitFor(output,UINT64_C(5000000))==XWAIT_OK);idle(executor);xrtFutureDestroy(output);}
        xrtCancelDestroy(parent);xrtFutureDestroy(source);xrtPromiseDestroy(state.promise);finish(executor);service_finish(service);balanced(&before);
    }
    /* Closed service / full service / closed queue reject without consuming
     * the initialized builder or leaving parent sources attached. */
    for(unsigned mode=0;mode<3;++mode){
        xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);State state=state_new(true);
        XL6_ContinuationExecutor* executor=xl6ContinuationExecutorCreateFixedV1(4,1);CHECK(executor);
        XL6_DeadlineServiceV1* service=xl6DeadlineServiceCreateV1(1);CHECK(service);xcancel* parent=xrtCancelCreate();CHECK(parent);
        xfuture* source=NULL;state.promise=xrtPromiseCreate(&source,NULL);CHECK(state.promise);
        xcancel* native=xrtPromiseCancelToken(state.promise);CHECK(native);
        Capture* occupied=NULL;
        if(mode==0)xl6DeadlineServiceCloseV1(service);
        if(mode==1){occupied=allocate(&state,source);CHECK(occupied);
            CHECK(xl6FutureCallbackInterruptArmV1(occupied,parent,service,xrtDeadlineAfter(UINT64_C(5000000))));}
        if(mode==2)xl6ContinuationExecutorClose(executor);
        Capture* capture=allocate(&state,source);CHECK(capture);
        bool armed=xl6FutureCallbackInterruptArmV1(capture,parent,service,xrtDeadlineAfter(UINT64_C(5000000)));
        if(mode<2)CHECK(!armed&&xrtErrorKind(xrtGetError())==(mode==0?XERR_STATE:XERR_RANGE));
        else CHECK(armed&&!xl6FutureCallbackInterruptStartV1(executor,capture,source));
        xrtClearError();xl6FutureCallbackRuntimeV1Get()->Discard(capture);xl6FutureCallbackRuntimeV1Get()->Discard(occupied);
        CHECK(xrtCancelRequest(parent)&&!xrtCancelRequested(native)&&!xrtAtomic32Load(&state.calls,XMEMORY_ACQUIRE));
        complete(state.promise,0);xrtCancelDestroy(native);xrtCancelDestroy(parent);xrtFutureDestroy(source);
        xrtPromiseDestroy(state.promise);finish(executor);service_finish(service);balanced(&before);
    }
}
typedef struct ParentRace {xcancel* parent;xpromise* promise;xatomic32 go;bool cancel;} ParentRace;
static int32 parent_race(void* raw)
{
    ParentRace* race=raw;while(!xrtAtomic32Load(&race->go,XMEMORY_ACQUIRE))xrtThreadYield();
    if(race->cancel)CHECK(xrtCancelRequest(race->parent));else complete(race->promise,0);return 0;
}
static void racing_interrupt(void)
{
    for(unsigned repeat=0;repeat<128;++repeat){
        xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);State state=state_new(true);
        XL6_ContinuationExecutor* executor=xl6ContinuationExecutorCreateFixedV1(4,1);CHECK(executor);
        xcancel* parent=xrtCancelCreate();CHECK(parent);xfuture* source=NULL;state.promise=xrtPromiseCreate(&source,NULL);CHECK(state.promise);
        Capture* capture=allocate(&state,source);CHECK(capture);
        CHECK(xl6FutureCallbackInterruptArmV1(capture,parent,NULL,XRT_DEADLINE_NEVER));
        xfuture* output=xl6FutureCallbackInterruptStartV1(executor,capture,source);CHECK(output);
        ParentRace a={.parent=parent,.cancel=true},b={.promise=state.promise};xrtAtomic32Init(&a.go,0);xrtAtomic32Init(&b.go,0);
        xthread* threads[]={xrtThreadCreate(parent_race,&a,0),xrtThreadCreate(parent_race,&b,0)};CHECK(threads[0]&&threads[1]);
        xrtAtomic32Store(&a.go,1,XMEMORY_RELEASE);xrtAtomic32Store(&b.go,1,XMEMORY_RELEASE);
        for(unsigned i=0;i<2;++i){CHECK(xrtThreadWaitFor(threads[i],UINT64_C(5000000))==XWAIT_OK);xrtThreadDestroy(threads[i]);}
        CHECK(xrtFutureWaitFor(output,UINT64_C(5000000))==XWAIT_OK);idle(executor);
        CHECK(xrtFutureState(output)==XFUTURE_RESOLVED||xrtFutureState(output)==XFUTURE_CANCELLED);
        CHECK(xrtAtomic32Load(&state.calls,XMEMORY_ACQUIRE)==1&&xrtAtomic32Load(&state.cleaned,XMEMORY_ACQUIRE)==1);
        xrtCancelDestroy(parent);xrtFutureDestroy(output);xrtFutureDestroy(source);xrtPromiseDestroy(state.promise);finish(executor);balanced(&before);++selectedRaces;
    }
}
int main(void)
{
    CHECK(sharedCallbackRegressionMain()==0);CHECK(xrtMemDebugEnable(true));
    const XL6_OwnershipDomainV1* domains[]={
        #include "callback-domains.h"
    };catalog=xl6OwnershipCatalogCreateV1(domains,sizeof(domains)/sizeof(domains[0]));CHECK(catalog);
    const XL6_OwnershipDomainV1* reduced[sizeof(domains)/sizeof(domains[0])];size_t count=0;
    for(size_t i=0;i<sizeof(domains)/sizeof(domains[0]);++i)
        if(domains[i]!=xl6OwnershipDomainFutureCallbackInterruptV1Get())reduced[count++]=domains[i];
    CHECK(count+1==sizeof(domains)/sizeof(domains[0]));missingInterruptCatalog=xl6OwnershipCatalogCreateV1(reduced,count);CHECK(missingInterruptCatalog);
    interrupted_cases();construction_interrupt();racing_interrupt();xl6OwnershipCatalogDestroyV1(missingInterruptCatalog);
    xl6OwnershipCatalogDestroyV1(catalog);CHECK(xrtMemDebugReset());
    printf("interruptible callbacks: cases=%u prefixes=%u failures=%u races=%u; actual frame/parent/deadline owners, real source terminal, worker cleanup, balanced\n",
        interrupted,constructionPrefixes,constructionFailures,selectedRaces);return 0;
}
