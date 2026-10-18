/* Native finite provider proof only; no public finite-language claim. */
#include "src/runtime/xl6_channel_direct_deadline.h"
#include "src/runtime/xl6_channel_select_deadline.h"
#include "src/runtime/xl6_channel_future_core.h"
#include "src/runtime/xl6_channel_case.h"
#include "src/runtime/xl6_channel_internal.h"
#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_ownership_domains.h"
#include "src/api/xl_foreign_error.h"
#include <assert.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#ifndef TEST_SELECT
#define TEST_SELECT 0
#endif
#define ID UINT64_C(0xCD8101)
#define ELEMENT UINT64_C(0xCD8102)
#define SLOT(Value) (XL6_ChannelSlotV1){&(Value),sizeof(Value)}
typedef XL6_ChannelFutureFrameV1 Frame;
typedef struct Prepared {Frame* frame;XL6_ChannelCaseOwnerV1* item;bool sending;unsigned transfer;} Prepared;
#if TEST_SELECT
static const XL6_ChannelSelectDeadlineRuntimeV1* api;
#else
static const XL6_ChannelDirectDeadlineRuntimeV1* api;
#endif
static void wait_flag(atomic_bool* flag)
{xdeadline until=xrtDeadlineAfter(10000000);while(!atomic_load_explicit(flag,memory_order_acquire)){assert(!xrtDeadlineExpired(until));xrtThreadYield();}}
#ifdef XL6_CHANNEL_DEADLINE_SOURCE_PROBE
static atomic_bool captureSource,captureThreads,stopBeforeStart,timeoutStopped,holdTail,tailEntered,tailGo;
static atomic_bool holdNotify,notifyEntered,notifyGo,sourceStopped;
static atomic_uint threadCreates,failWait;
static _Atomic(xcond*) sourceCondition;
xcond* __real_xrtCondCreate(void);
xcond* __wrap_xrtCondCreate(void)
{xcond* value=__real_xrtCondCreate();if(atomic_load(&captureSource)&&!atomic_load(&sourceCondition))atomic_store(&sourceCondition,value);return value;}
xwaitresult __real_xrtCondWaitUntil(xcond*,xmutex*,xdeadline);
xwaitresult __wrap_xrtCondWaitUntil(xcond* condition,xmutex* mutex,xdeadline until)
{
    unsigned mode=condition==atomic_load(&sourceCondition)?atomic_exchange(&failWait,0):0;
    if(mode){xrtClearError();if(mode==1)xrtSetErrorKind(XERR_RANGE);return XWAIT_ERROR;}
    return __real_xrtCondWaitUntil(condition,mutex,until);
}
xthread* __real_xrtThreadCreate(xthreadproc,void*,size_t);
xthread* __wrap_xrtThreadCreate(xthreadproc proc,void* data,size_t stack)
{if(atomic_load(&captureThreads))atomic_fetch_add(&threadCreates,1);return __real_xrtThreadCreate(proc,data,stack);}
bool __real_xrtChannelSubscribeStart(xchannelsubscription*);
bool __wrap_xrtChannelSubscribeStart(xchannelsubscription* value)
{if(atomic_load(&stopBeforeStart))wait_flag(&timeoutStopped);return __real_xrtChannelSubscribeStart(value);}
bool __real_xrtChannelSubscribeStop(xchannelsubscription*,xwaitresult);
bool __wrap_xrtChannelSubscribeStop(xchannelsubscription* value,xwaitresult reason)
{bool result=__real_xrtChannelSubscribeStop(value,reason);if(reason==XWAIT_TIMEOUT)atomic_store(&timeoutStopped,true);return result;}
void __real_xl6ChannelFutureCoreStopV1(Frame*,xchannelsubscription*,xwaitresult,bool,const xerror*);
void __wrap_xl6ChannelFutureCoreStopV1(Frame* frame,xchannelsubscription* sub,xwaitresult reason,bool failed,const xerror* error)
{
    __real_xl6ChannelFutureCoreStopV1(frame,sub,reason,failed,error);
    atomic_store(&sourceStopped,true);
    if(atomic_load(&holdTail)){atomic_store(&tailEntered,true);wait_flag(&tailGo);}
}
void __real_xl6ChannelFutureCoreNotifyV1(xchannelsubscription*,void*);
void __wrap_xl6ChannelFutureCoreNotifyV1(xchannelsubscription* sub,void* data)
{
    if(atomic_load(&holdNotify)){atomic_store(&notifyEntered,true);wait_flag(&notifyGo);}
    __real_xl6ChannelFutureCoreNotifyV1(sub,data);
}
#endif
static XL6_DeadlineServiceV1* service(size_t capacity)
{
#ifdef XL6_CHANNEL_DEADLINE_SOURCE_PROBE
    atomic_store(&sourceCondition,NULL);atomic_store(&captureSource,true);
#endif
    XL6_DeadlineServiceV1* result=xl6DeadlineServiceCreateV1(capacity);assert(result);
#ifdef XL6_CHANNEL_DEADLINE_SOURCE_PROBE
    atomic_store(&captureSource,false);assert(atomic_load(&sourceCondition));
#endif
    return result;
}
static XL6_SequenceLayoutV1 layout(unsigned transfer)
{return (XL6_SequenceLayoutV1){ID,ELEMENT,transfer,transfer==2?sizeof(xstrview):transfer==3?sizeof(xvalue*):sizeof(int64_t),
    transfer==2?xl6StringTypeV1():transfer==3?xrtTypeValue():xrtTypeInt64()};}
static xvalue* channel(unsigned transfer,size_t capacity)
{
    XL6_SequenceLayoutV1 l=layout(transfer);
    xvalue* schema=transfer==3?xl6SequenceReferenceCreateV1(1,123,ELEMENT,XL6_VALUE_ADMIT_ANY,true,ELEMENT):
        xl6SequencePrimitiveCreateV1(1,123,ELEMENT,transfer,l.size,l.type,
            transfer==2?XL6_VALUE_ADMIT_STRING:XL6_VALUE_ADMIT_I64,false,ELEMENT);
    assert(schema);xvalue* q=xl6ChannelCreateTakeV1(ID,schema,(int64_t)capacity);assert(q);return q;
}
static void send(xvalue* q,unsigned transfer,const void* data)
{
    XL6_SequenceLayoutV1 l=layout(transfer);XL6_ChannelStatusV1 status;
    assert(xl6ChannelSendV1(q,ID,ELEMENT,l.type,(XL6_ChannelSlotV1){(void*)data,l.size},XL6_CHANNEL_WAIT_V1,0,&status)&&status==XL6_CHANNEL_OK_V1);
}
static int64_t take(xvalue* q)
{int64_t value=0;XL6_ChannelStatusV1 status;assert(xl6ChannelReceiveV1(q,ID,ELEMENT,xrtTypeInt64(),SLOT(value),XL6_CHANNEL_WAIT_V1,0,&status)&&status==XL6_CHANNEL_OK_V1);return value;}
static Prepared prepare(xvalue* q,unsigned transfer,bool sending,const void* value,bool relative,int64_t ms,xdeadline until)
{
    Prepared p={NULL,NULL,sending,transfer};XL6_SequenceLayoutV1 l=layout(transfer);
#if TEST_SELECT
    p.item=xl6ChannelCaseCreateSchemaV1(l,q,value,sending);
    if(p.item)p.frame=relative?api->PrepareFor(&p.item,1,ms):api->PrepareUntil(&p.item,1,until);
#else
    if(sending)p.frame=relative?api->PrepareSendFor(l,q,value,ms):api->PrepareSendUntil(l,q,value,until);
    else p.frame=relative?api->PrepareReceiveFor(l,q,ms):api->PrepareReceiveUntil(l,q,until);
#endif
    return p;
}
static void drop_case(Prepared p)
{
#if TEST_SELECT
    xl6ChannelCaseDestroyV1(p.item);
#else
    (void)p;
#endif
}
static void discard(Prepared p) {api->Discard(p.frame);drop_case(p);}
static xfuture* start(XL6_ContinuationExecutor* executor,XL6_DeadlineServiceV1* source,Prepared p,xcancel* parent)
{
#if TEST_SELECT
    return api->Start(executor,source,p.frame,api->ResultHandle(p.frame),NULL,0,parent);
#else
    return p.sending?api->StartSend(executor,source,p.frame,parent):
        api->StartReceive(executor,source,p.frame,api->ResultHandle(p.frame),NULL,0,parent);
#endif
}
static xfutureresult outcome(xfuture* future)
{assert(xrtFutureWaitFor(future,10000000)==XWAIT_OK);xfutureresult result={0};assert(xrtFutureResult(future,&result));return result;}
static void* record(xfuture* future)
{
    xfutureresult result=outcome(future);assert(result.State==XFUTURE_RESOLVED&&result.Value);
    void* data=NULL;void* user=NULL;const xvaluehandleops* ops=NULL;
    assert(xrtValueGetHandle(*(xvalue**)result.Value,&data,&ops,&user)&&data&&!user);return data;
}
static int64_t status(xfuture* future,Prepared p)
{
#if TEST_SELECT
    XL6_ChannelSelectionV1 info;assert(xl6ChannelSelectionInfoV1(record(future),&info));return info.status;
#else
    if(p.sending){xfutureresult result=outcome(future);assert(result.State==XFUTURE_RESOLVED&&result.Value);return *(int64_t*)result.Value;}
    int64_t result=-1;assert(xl6ChannelReceiveResultInfoV1(record(future),&result));return result;
#endif
}
static void read_value(xfuture* future,Prepared p,void* output)
{
    XL6_SequenceLayoutV1 l=layout(p.transfer);
#if TEST_SELECT
    assert(xl6ChannelSelectionValueV1(record(future),p.item,ELEMENT,l.type,(XL6_ChannelSlotV1){output,l.size}));
#else
    XL6_FutureValueProfileV1 profile={sizeof(profile),p.transfer==1?XL6_FUTURE_VALUE_TRIVIAL:
        p.transfer==2?XL6_FUTURE_VALUE_STRING:XL6_FUTURE_VALUE_XVALUE,l.size,ELEMENT,NULL};
    assert(xl6ChannelReceiveResultReadV1(profile,record(future),output));
#endif
}
static void idle(XL6_ContinuationExecutor* executor)
{xdeadline until=xrtDeadlineAfter(10000000);while(!xl6ContinuationExecutorIsIdle(executor)){assert(!xrtDeadlineExpired(until));xrtThreadYield();}}
static void finish(XL6_ContinuationExecutor* executor,XL6_DeadlineServiceV1* source)
{
    idle(executor);xl6ContinuationExecutorClose(executor);assert(xl6ContinuationExecutorDestroy(executor));
    xl6DeadlineServiceCloseV1(source);assert(xl6DeadlineServiceDestroyCompletedV1(source));
}
static void balanced(const xmemdebugsnapshot* before)
{
    xrtClearError();xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(before->LiveCount==after.LiveCount&&before->LiveBytes==after.LiveBytes&&
        after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount&&
        before->InvalidFreeCount==after.InvalidFreeCount&&before->DoubleFreeCount==after.DoubleFreeCount&&before->UseAfterFreeCount==after.UseAfterFreeCount);
}
typedef struct Edges {const void* source;size_t sources,subscriptions;} Edges;
static bool edge(xrtownershipref ref,void* data)
{
    Edges* edges=data;if(ref.Data==edges->source)++edges->sources;
    const xchannelsubscribeownershipv1* policy=
#if TEST_SELECT
        xl6ChannelFutureSubscriptionPolicyV1Get();
#else
        xl6ChannelDirectFutureSubscriptionPolicyV1Get();
#endif
    if(xrtChannelSubscribeOwnershipAdapterV1(ref,&policy,1))++edges->subscriptions;
    return true;
}
static void freeze_source(XL6_DeadlineServiceV1* source,xrtownershipscope* freeze)
{
    xdeadline until=xrtDeadlineAfter(10000000);
    for(;;){if(xrtOwnershipFreezeTryBegin(freeze)){size_t count;xrtownershipref ref=xl6DeadlineServiceOwnershipV1(source);
            if(ref.Ops->Count(ref.Data,&count))return;
            assert(xrtOwnershipScopeEnd(freeze));}
        assert(!xrtDeadlineExpired(until));xrtThreadYield();}
}
typedef struct Peer {xvalue* q;unsigned turns;} Peer;
static int32_t peer_send(void* data)
{Peer* peer=data;for(unsigned i=0;i<peer->turns;++i)xrtThreadYield();int64_t value=73;send(peer->q,1,&value);return 0;}
typedef struct Starter {XL6_ContinuationExecutor* executor;XL6_DeadlineServiceV1* source;Prepared item;xfuture* future;} Starter;
static int32_t peer_start(void* data)
{Starter* starter=data;starter->future=start(starter->executor,starter->source,starter->item,NULL);assert(starter->future);return 0;}
typedef struct Probe {unsigned drops,failure;uint64_t thread;} Probe;
static void probe_drop(void* data,void* context)
{
    Probe* probe=data;assert(!context);++probe->drops;probe->thread=xrtThreadCurrentId();
    if(probe->failure==1)xl6ErrorScopePropagateFailure((xl6_error_result){false,NULL});
    else if(probe->failure==2)xrtSetErrorKind(XERR_RANGE);
}
static const xvaluehandleops probeOps={NULL,probe_drop,NULL,NULL};
int main(void)
{
    assert(xrtMemDebugEnable(true));
#if TEST_SELECT
    api=xl6ChannelSelectDeadlineRuntimeV1Get();
#else
    api=xl6ChannelDirectDeadlineRuntimeV1Get();
#endif
    assert(api);
    // Invalid budgets are rejected before dereferencing/copying inputs.
    for(unsigned mode=0;mode<3;++mode){xl6_error_scope scope={0};assert(xl6ErrorScopeBegin(&scope));Frame* frame=NULL;
#if TEST_SELECT
        frame=mode==2?api->PrepareUntil(NULL,0,XRT_DEADLINE_NEVER):api->PrepareFor(NULL,0,mode==0?-1:INT64_MAX);
#else
        frame=mode==2?api->PrepareSendUntil(layout(1),NULL,NULL,XRT_DEADLINE_NEVER):api->PrepareSendFor(layout(1),NULL,NULL,mode==0?-1:INT64_MAX);
#endif
        assert(!frame);xl6ErrorScopeCaptureFailure(&scope);xl6_error_result result=xl6ErrorScopeEnd(&scope);
        assert(!result.ok&&result.error&&xrtErrorKind(result.error)==(mode==2?XERR_ARGUMENT:XERR_RANGE));xrtErrorFree(result.error);}
    // Relative/absolute budgets, both directions, ready-before-stop, close,
    // pending timeout, output/parent/pre-parent cancellation; no lost messages.
    for(unsigned relative=0;relative<2;++relative)for(unsigned sending=0;sending<2;++sending)for(unsigned mode=0;mode<8;++mode){
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);XL6_DeadlineServiceV1* source=service(4);
        XL6_ContinuationExecutor* executor=xl6ContinuationExecutorCreateFixedV1(4,1);assert(executor);
        xvalue* q=channel(1,1);int64_t value=11;
        bool ready=mode==0||mode==7;if((sending&&!ready)||(!sending&&ready))send(q,1,&value);
        if(mode==2)assert(xl6ChannelCloseV1(q,ID));
        xcancel* parent=xrtCancelCreate();assert(parent);
        if(mode==6||mode==7)assert(xrtCancelRequest(parent));
        bool immediate=mode==0||mode==2||mode==7;int64_t ms=immediate?0:mode==1?2:60000;
        xdeadline due=immediate?0:xrtClock()+(mode==1?2000:60000000);
        Prepared p=prepare(q,1,sending,&value,relative,ms,due);assert(p.frame);
#ifdef XL6_CHANNEL_DEADLINE_SOURCE_PROBE
        atomic_store(&timeoutStopped,false);atomic_store(&stopBeforeStart,immediate);
#endif
        xfuture* future=start(executor,source,p,parent);assert(future);
#ifdef XL6_CHANNEL_DEADLINE_SOURCE_PROBE
        atomic_store(&stopBeforeStart,false);
#endif
        if(mode==3){if(sending)assert(take(q)==11);else send(q,1,&value);}
        if(mode==4)assert(xrtFutureCancel(future));
        if(mode==5)assert(xrtCancelRequest(parent));
        int64_t expected=mode==0||mode==3||mode==7?XL6_CHANNEL_OK_V1:mode==1?XL6_CHANNEL_TIMEOUT_V1:
            mode==2?XL6_CHANNEL_CLOSED_V1:XL6_CHANNEL_CANCELLED_V1;
        assert(status(future,p)==expected);
        if(expected==XL6_CHANNEL_OK_V1){if(sending)assert(take(q)==11);else {int64_t got=0;read_value(future,p,&got);assert(got==11);}}
        else if(sending&&mode!=2)assert(take(q)==11);
        xrtFutureDestroy(future);xrtCancelDestroy(parent);drop_case(p);xrtValueRelease(q);finish(executor,source);balanced(&before);
    }
    // Expired deadlines cannot override already-ready UTF-8/NUL or nullable null.
    for(unsigned transfer=2;transfer<=3;++transfer){xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
        XL6_DeadlineServiceV1* source=service(1);XL6_ContinuationExecutor* executor=xl6ContinuationExecutorCreateFixedV1(1,1);assert(executor);
        xvalue* q=channel(transfer,1);xstrview text={"a\0b",3};xvalue* null=NULL;send(q,transfer,transfer==2?(void*)&text:(void*)&null);
        Prepared p=prepare(q,transfer,false,NULL,false,0,0);assert(p.frame);xfuture* future=start(executor,source,p,NULL);assert(future&&status(future,p)==XL6_CHANNEL_OK_V1);
        if(transfer==2){xstrview got={0};read_value(future,p,&got);assert(got.Size==3&&!memcmp(got.Data,"a\0b",3));xrtTypeDropValue(xl6StringTypeV1(),&got);}
        else {xvalue* got=NULL;read_value(future,p,&got);assert(!got);}
        xrtFutureDestroy(future);drop_case(p);xrtValueRelease(q);finish(executor,source);balanced(&before);}
    // Capacity refusal occurs BEFORE native Start/transfer; abort restores the
    // executor credit, and cancellation makes the timer slot reusable.
    {xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);XL6_DeadlineServiceV1* source=service(1);
        XL6_ContinuationExecutor* executor=xl6ContinuationExecutorCreateFixedV1(2,1);assert(executor);
        xvalue* q=channel(1,1);Prepared first=prepare(q,1,false,NULL,true,60000,0);assert(first.frame);
        xfuture* a=start(executor,source,first,NULL);assert(a);xvalue* ready=channel(1,1);int64_t value=77;send(ready,1,&value);
        Prepared second=prepare(ready,1,false,NULL,true,60000,0);assert(second.frame);
        assert(!start(executor,source,second,NULL));xrtClearError();discard(second);assert(take(ready)==77);
        assert(xrtFutureCancel(a)&&status(a,first)==XL6_CHANNEL_CANCELLED_V1);idle(executor);xrtFutureDestroy(a);drop_case(first);
        Prepared third=prepare(ready,1,false,NULL,false,0,0);assert(third.frame);xfuture* b=start(executor,source,third,NULL);assert(b&&status(b,third)==XL6_CHANNEL_TIMEOUT_V1);
        xrtFutureDestroy(b);drop_case(third);xrtValueRelease(ready);xrtValueRelease(q);finish(executor,source);balanced(&before);}
    // Close rejects new work without cancelling an accepted finite wait.
    {xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);XL6_DeadlineServiceV1* source=service(2);
        XL6_ContinuationExecutor* executor=xl6ContinuationExecutorCreateFixedV1(2,1);assert(executor);xvalue* q=channel(1,1);
        Prepared first=prepare(q,1,false,NULL,true,2,0);assert(first.frame);xfuture* future=start(executor,source,first,NULL);assert(future);
        xl6DeadlineServiceCloseV1(source);Prepared second=prepare(q,1,false,NULL,true,60000,0);assert(second.frame);
        assert(!start(executor,source,second,NULL));xrtClearError();discard(second);assert(status(future,first)==XL6_CHANNEL_TIMEOUT_V1);
        xrtFutureDestroy(future);drop_case(first);xrtValueRelease(q);finish(executor,source);balanced(&before);}
    // Finite/unbounded context and direct direction are distinct contracts;
    // invalid entry does not publish a Future or transfer a message.
    {xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);XL6_DeadlineServiceV1* source=service(1);
        XL6_ContinuationExecutor* executor=xl6ContinuationExecutorCreateFixedV1(1,1);assert(executor);xvalue* q=channel(1,1);
        Prepared finite=prepare(q,1,false,NULL,true,60000,0);assert(finite.frame);
#if TEST_SELECT
        const XL6_ChannelFutureRuntimeV1* plain=xl6ChannelFutureRuntimeV1Get();
        assert(!plain->Start(executor,finite.frame,api->ResultHandle(finite.frame),NULL,0,NULL));xrtClearError();
        Frame* infinite=plain->Prepare(&finite.item,1);assert(infinite);
        assert(!api->Start(executor,source,infinite,plain->ResultHandle(infinite),NULL,0,NULL));xrtClearError();
#else
        const XL6_ChannelDirectFutureRuntimeV1* plain=xl6ChannelDirectFutureRuntimeV1Get();
        assert(!plain->StartReceive(executor,finite.frame,api->ResultHandle(finite.frame),NULL,0,NULL));xrtClearError();
        assert(!api->StartSend(executor,source,finite.frame,NULL));xrtClearError();
        Frame* infinite=plain->PrepareReceive(layout(1),q);assert(infinite);
        assert(!api->StartReceive(executor,source,infinite,plain->ResultHandle(infinite),NULL,0,NULL));xrtClearError();
#endif
        assert(!xl6ChannelFutureCoreStartWakeV1(executor,infinite,plain->ResultHandle(infinite),NULL,0,NULL,NULL));xrtClearError();
        plain->Discard(infinite);discard(finite);xrtValueRelease(q);finish(executor,source);balanced(&before);}
    // User value cleanup belongs to the fixed worker, never the timer source.
    // Rich false/NULL and ordinary cleanup errors override normal timeout.
    for(unsigned failure=0;failure<3;++failure){xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
        XL6_DeadlineServiceV1* source=service(1);XL6_ContinuationExecutor* executor=xl6ContinuationExecutorCreateFixedV1(1,1);assert(executor);
        xvalue* q=channel(3,1);xvalue* padding=NULL;send(q,3,&padding);Probe probe={0,failure,0};void* raw=&probe;
        xvalue* value=xrtValueHandleTake(&raw,&probeOps,NULL);assert(value&&!raw);
        Prepared p=prepare(q,3,true,&value,true,2,0);assert(p.frame);xrtValueRelease(value);drop_case(p);p.item=NULL;
        xfuture* future=start(executor,source,p,NULL);assert(future);xfutureresult result=outcome(future);
        if(!failure)assert(status(future,p)==XL6_CHANNEL_TIMEOUT_V1);
        else assert(result.State==XFUTURE_FAILED&&result.Error&&xrtErrorKind(result.Error)==(failure==1?XERR_INTERNAL:XERR_RANGE));
        assert(probe.drops==1&&probe.thread&&probe.thread!=xrtThreadCurrentId());
        xrtFutureDestroy(future);xrtValueRelease(q);finish(executor,source);balanced(&before);}
    // Fully enumerate allocation failures in preparation and Start for both
    // directions. Failed Start is always private, safe for exactly one Discard.
    unsigned prefixes[4]={0};
    for(unsigned phase=0;phase<4;++phase)for(size_t point=0;point<256;++point){
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);XL6_DeadlineServiceV1* source=service(1);
        XL6_ContinuationExecutor* executor=xl6ContinuationExecutorCreateFixedV1(1,1);assert(executor);xvalue* q=channel(1,1);
        int64_t value=11;bool sending=phase<2;if(sending)send(q,1,&value);Prepared p={0};xfuture* future=NULL;
        if(phase%2){p=prepare(q,1,sending,&value,true,60000,0);assert(p.frame);}
        assert(xrtMemDebugFailAfter(point));
        if(!(phase%2))p=prepare(q,1,sending,&value,true,60000,0);else future=start(executor,source,p,NULL);
        bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();xrtClearError();
        if(future){assert(xrtFutureCancel(future));assert(status(future,p)==XL6_CHANNEL_CANCELLED_V1);xrtFutureDestroy(future);drop_case(p);}
        else discard(p);
        xrtValueRelease(q);finish(executor,source);balanced(&before);
        if(!hit){prefixes[phase]=(unsigned)point;break;}assert(point<255);
    }
    // One source + one worker, 512 simultaneous waits. The extra native credit
    // and source lease are actual repeated owning edges, not fabricated nodes.
    {enum{N=512};xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
#ifdef XL6_CHANNEL_DEADLINE_SOURCE_PROBE
        atomic_store(&threadCreates,0);atomic_store(&captureThreads,true);
#endif
        XL6_DeadlineServiceV1* source=service(N);XL6_ContinuationExecutor* executor=xl6ContinuationExecutorCreateFixedV1(N,1);assert(executor);
        xvalue* q=channel(1,1);Prepared items[N];xfuture* futures[N];xdeadline due=xrtClock()+60000000;
        for(size_t i=0;i<N;++i){items[i]=prepare(q,1,false,NULL,false,0,due);assert(items[i].frame);futures[i]=start(executor,source,items[i],NULL);assert(futures[i]);}
#ifdef XL6_CHANNEL_DEADLINE_SOURCE_PROBE
        atomic_store(&captureThreads,false);assert(atomic_load(&threadCreates)==2);
#endif
        xrtownershipscope freeze={0};freeze_source(source,&freeze);xrtownershipref ref=xl6DeadlineServiceOwnershipV1(source);size_t refs=0;
        assert(ref.Ops->Count(ref.Data,&refs)&&refs==N+1);xrtownershipref frame=api->Ownership(items[0].frame);
        assert(frame.Ops->Count(frame.Data,&refs)&&refs==5);Edges edges={source,0,0};assert(frame.Ops->Trace(frame.Data,edge,&edges)&&edges.sources==1&&edges.subscriptions==3);
        xrtownershipresult graph={0};assert(xrtOwnershipInspect(&ref,1,NULL,0,&graph)&&graph.NodeCount>N);
        assert(xrtOwnershipScopeEnd(&freeze));assert(xrtMemDebugFailAfter(0));
        for(size_t i=0;i<N;++i)assert(xrtFutureCancel(futures[i]));
        for(size_t i=0;i<N;++i){assert(status(futures[i],items[i])==XL6_CHANNEL_CANCELLED_V1);xrtFutureDestroy(futures[i]);drop_case(items[i]);}
        xrtValueRelease(q);finish(executor,source);assert(!xrtMemDebugFailTriggered());xrtMemDebugFailClear();balanced(&before);}
    // Local native producer race: a timed-out/cancelled receive must leave the
    // peer's message queued, while success owns it exactly once.
    for(unsigned turn=0;turn<100;++turn){xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
        XL6_DeadlineServiceV1* source=service(1);XL6_ContinuationExecutor* executor=xl6ContinuationExecutorCreateFixedV1(1,1);assert(executor);
        xvalue* q=channel(1,1);Prepared p=prepare(q,1,false,NULL,false,0,xrtClock()+100);assert(p.frame);
        Peer peer={q,turn%16};xthread* thread=xrtThreadCreate(peer_send,&peer,0);assert(thread);xfuture* future=start(executor,source,p,NULL);assert(future);
        if(turn%3==0)(void)xrtFutureCancel(future);
        int64_t state=status(future,p);
        assert(xrtThreadWait(thread)==XWAIT_OK);xrtThreadDestroy(thread);
        if(state==XL6_CHANNEL_OK_V1){int64_t got=0;read_value(future,p,&got);assert(got==73);}
        else {assert(state==XL6_CHANNEL_TIMEOUT_V1||state==XL6_CHANNEL_CANCELLED_V1);assert(take(q)==73);}
        xrtFutureDestroy(future);drop_case(p);xrtValueRelease(q);finish(executor,source);balanced(&before);}
#ifdef XL6_CHANNEL_DEADLINE_SOURCE_PROBE
    // Ready has already committed but native Notify is held before the core's
    // boundary. A source error cannot replace that owned result or lose data.
    for(unsigned mode=1;mode<=2;++mode){xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);XL6_DeadlineServiceV1* source=service(1);
        XL6_ContinuationExecutor* executor=xl6ContinuationExecutorCreateFixedV1(1,1);assert(executor);xvalue* q=channel(1,1);int64_t value=91;send(q,1,&value);
        Prepared p=prepare(q,1,false,NULL,true,60000,0);assert(p.frame);Starter starter={executor,source,p,NULL};
        atomic_store(&notifyEntered,false);atomic_store(&notifyGo,false);atomic_store(&sourceStopped,false);atomic_store(&holdNotify,true);
        xthread* thread=xrtThreadCreate(peer_start,&starter,0);assert(thread);wait_flag(&notifyEntered);
        atomic_store(&failWait,mode);xl6DeadlineServiceCloseV1(source);wait_flag(&sourceStopped);
        atomic_store(&holdNotify,false);atomic_store(&notifyGo,true);assert(xrtThreadWait(thread)==XWAIT_OK);xrtThreadDestroy(thread);
        assert(status(starter.future,p)==XL6_CHANNEL_OK_V1);int64_t got=0;read_value(starter.future,p,&got);assert(got==91);
        xl6_error_scope scope={0};assert(xl6ErrorScopeBegin(&scope));assert(!xl6DeadlineServiceCheckV1(source));xl6ErrorScopeCaptureFailure(&scope);
        xl6_error_result failure=xl6ErrorScopeEnd(&scope);assert(!failure.ok);
        if(mode==1)assert(failure.error&&xrtErrorKind(failure.error)==XERR_RANGE);else assert(!failure.error);
        xrtErrorFree(failure.error);xrtFutureDestroy(starter.future);drop_case(p);xrtValueRelease(q);finish(executor,source);balanced(&before);}
    // A physical source callback tail cannot delay logical delivery or worker
    // retirement. It keeps real frame/native credits until its final Drop.
    {xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);XL6_DeadlineServiceV1* source=service(1);
        XL6_ContinuationExecutor* executor=xl6ContinuationExecutorCreateFixedV1(1,1);assert(executor);xvalue* q=channel(1,1);
        atomic_store(&tailEntered,false);atomic_store(&tailGo,false);atomic_store(&holdTail,true);
        Prepared p=prepare(q,1,false,NULL,true,2,0);assert(p.frame);xfuture* future=start(executor,source,p,NULL);assert(future);
        wait_flag(&tailEntered);assert(status(future,p)==XL6_CHANNEL_TIMEOUT_V1);idle(executor);
        xrtownershipscope freeze={0};xdeadline until=xrtDeadlineAfter(10000000);
        while(!xrtOwnershipFreezeTryBegin(&freeze)){assert(!xrtDeadlineExpired(until));xrtThreadYield();}
        xrtownershipref frame=api->Ownership(p.frame);size_t refs=0;
        assert(frame.Ops->Count(frame.Data,&refs)&&refs==2);
        const xrtownershippreparationv1* preparation=NULL;const xrtownershipadapterv1* adapter=
#if TEST_SELECT
            xl6ChannelFutureOwnershipAdapterV1(frame,&preparation);
#else
            xl6ChannelDirectFutureOwnershipAdapterV1(frame,&preparation);
#endif
        assert(adapter&&adapter->Hold(frame.Data));assert(xrtOwnershipScopeEnd(&freeze));
        atomic_store(&holdTail,false);atomic_store(&tailGo,true);xrtFutureDestroy(future);drop_case(p);xrtValueRelease(q);
        finish(executor,source);adapter->Drop(frame.Data);balanced(&before);}
    // Source failure is NOT timeout, including diagnostic-allocation failure.
    // Later worker cleanup cannot replace its first rich false/NULL outcome.
    // A failed source also rolls back a new Start without native transfer.
    for(unsigned mode=1;mode<=2;++mode){xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);XL6_DeadlineServiceV1* source=service(1);
        XL6_ContinuationExecutor* executor=xl6ContinuationExecutorCreateFixedV1(1,1);assert(executor);xvalue* q=channel(3,1);
        xvalue* padding=NULL;send(q,3,&padding);Probe probe={0,mode==1?1u:2u,0};void* raw=&probe;
        xvalue* value=xrtValueHandleTake(&raw,&probeOps,NULL);assert(value&&!raw);
        Prepared p=prepare(q,3,true,&value,true,60000,0);assert(p.frame);xrtValueRelease(value);drop_case(p);p.item=NULL;
        xfuture* future=start(executor,source,p,NULL);assert(future);
        atomic_store(&failWait,mode);xl6DeadlineServiceCloseV1(source);xfutureresult result=outcome(future);
        assert(result.State==XFUTURE_FAILED&&result.Error&&xrtErrorKind(result.Error)==(mode==1?XERR_RANGE:XERR_INTERNAL));
        assert(probe.drops==1&&probe.thread!=xrtThreadCurrentId());
        idle(executor);xrtFutureDestroy(future);drop_case(p);
        Prepared second=prepare(q,3,false,NULL,true,60000,0);assert(second.frame);
        xl6_error_scope scope={0};assert(xl6ErrorScopeBegin(&scope));assert(!start(executor,source,second,NULL));
        xl6ErrorScopeCaptureFailure(&scope);discard(second);xl6_error_result failure=xl6ErrorScopeEnd(&scope);assert(!failure.ok);
        if(mode==1)assert(failure.error&&xrtErrorKind(failure.error)==XERR_RANGE);else assert(!failure.error);
        xrtErrorFree(failure.error);xrtValueRelease(q);finish(executor,source);balanced(&before);}
    puts("source-only: 2 actual threads for 512 waits; deterministic stop-before-Start, held physical tail and rich source failures passed");
#endif
    printf("finite Channel %s: For/Until both directions; ready-before-stop/closed/timeout/cancel; NUL/null; full rollback/reuse; context/direction refusals; worker cleanup errors; OOM %u/%u/%u/%u; 512 waits, shared source/fixed worker; exact service/native owning edges; no late allocation; 100 peer races; balanced\n",
        TEST_SELECT?"selector":"direct",prefixes[0],prefixes[1],prefixes[2],prefixes[3]);
    return 0;
}
