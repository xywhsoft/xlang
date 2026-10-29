/* Direct native prerequisite only; public language bindings remain separate. */
#include "release/include/xl_clock.h"
#include "src/runtime/xl6_channel_direct_future.h"
#include "src/runtime/xl6_channel_internal.h"
#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_native_handle.h"
#include "src/runtime/xl6_ownership_domains.h"
#include "src/api/xl_foreign_error.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdatomic.h>
#define ID UINT64_C(0xCD7101)
#define ELEMENT UINT64_C(0xCD7102)
#define SLOT(Value) (XL6_ChannelSlotV1){&(Value),sizeof(Value)}
static const XL6_ChannelDirectFutureRuntimeV1* api;
static XL6_SequenceLayoutV1 layout(unsigned transfer)
{return (XL6_SequenceLayoutV1){ID,ELEMENT,transfer,transfer==2?sizeof(xstrview):transfer==3?sizeof(xvalue*):sizeof(int64_t),
    transfer==2?xl6StringTypeV1():transfer==3?xrtTypeValue():xrtTypeInt64()};}
static XL6_FutureValueProfileV1 profile(unsigned transfer)
{XL6_SequenceLayoutV1 l=layout(transfer);return (XL6_FutureValueProfileV1){sizeof(XL6_FutureValueProfileV1),
    transfer==2?XL6_FUTURE_VALUE_STRING:transfer==3?XL6_FUTURE_VALUE_XVALUE:XL6_FUTURE_VALUE_TRIVIAL,l.size,ELEMENT,NULL};}
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
static xfutureresult outcome(xfuture* future)
{assert(xrtFutureWaitFor(future,10000000)==XWAIT_OK);xfutureresult result={0};assert(xrtFutureResult(future,&result));return result;}
static const xvalue* received(xfuture* future)
{xfutureresult result=outcome(future);assert(result.State==XFUTURE_RESOLVED&&result.Value);return *(xvalue**)result.Value;}
static int64_t sent(xfuture* future)
{xfutureresult result=outcome(future);assert(result.State==XFUTURE_RESOLVED&&result.Value);return *(int64_t*)result.Value;}
static XL6_ChannelReceiveResultV1* record(const xvalue* value,bool object)
{
    const xvalue* handle=object?xrtValueObjectGet(value,XRT_STR_LITERAL("native")):value;
    void* raw=NULL;void* user=NULL;const xvaluehandleops* ops=NULL;
    assert(xrtValueGetHandle(handle,&raw,&ops,&user)&&raw&&!user&&ops==xl6ChannelReceiveResultHandlePolicyV1Get()->Ops);return raw;
}
static xvalue* carrier(XL6_ChannelFutureFrameV1* frame)
{xvalue* value=xrtValueObject();assert(value&&xrtValueObjectSet(value,XRT_STR_LITERAL("native"),api->ResultHandle(frame)));return value;}
static void idle(XL6_ContinuationExecutor* executor)
{XL6_DeadlineV1 deadline=xl6DeadlineAfterMillisecondsV1(10000);while(!xl6ContinuationExecutorIsIdle(executor)){assert(!xl6DeadlineExpiredV1(deadline));xrtThreadYield();}}
static void finish(XL6_ContinuationExecutor* executor)
{idle(executor);xl6ContinuationExecutorClose(executor);assert(xl6ContinuationExecutorDestroy(executor));}
static void balanced(const xmemdebugsnapshot* before)
{
    xrtClearError();xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(before->LiveCount==after.LiveCount&&before->LiveBytes==after.LiveBytes&&
        after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount&&
        before->InvalidFreeCount==after.InvalidFreeCount&&before->DoubleFreeCount==after.DoubleFreeCount&&before->UseAfterFreeCount==after.UseAfterFreeCount);
}
typedef struct Pin {xrtownershipref ref;const xrtownershipadapterv1* adapter;} Pin;
static bool pin_subscription(xrtownershipref ref,void* data)
{
    Pin* pin=data;if(pin->adapter)return true;
    const xchannelsubscribeownershipv1* policy=xl6ChannelDirectFutureSubscriptionPolicyV1Get();
    const xrtownershipadapterv1* adapter=xrtChannelSubscribeOwnershipAdapterV1(ref,&policy,1);
    if(adapter){assert(adapter->Hold(ref.Data));pin->ref=ref;pin->adapter=adapter;}return true;
}
static bool edge(xrtownershipref ref,void* data)
{if(ref.Data){assert(ref.Ops);++*(size_t*)data;}return true;}
static void inspect(XL6_ChannelFutureFrameV1* frame,bool sending,Pin* pin)
{
    const XL6_OwnershipDomainV1* domain=xl6OwnershipDomainChannelDirectFutureV1Get();
    XL6_OwnershipCatalog* catalog=xl6OwnershipCatalogCreateV1(&domain,1);assert(catalog);
    xrtownershipscope freeze={0};XL6_DeadlineV1 deadline=xl6DeadlineAfterMillisecondsV1(10000);
    while(!xrtOwnershipFreezeTryBegin(&freeze)){assert(!xl6DeadlineExpiredV1(deadline));xrtThreadYield();}
    xrtownershipref ref=api->Ownership(frame);size_t count=0,edges=0;
    assert(ref.Ops->Count(ref.Data,&count)&&count==4);
    assert(ref.Ops->Trace(ref.Data,edge,&edges)&&edges==(sending?6u:7u));
    const xrtownershipadapterv1* adapter=NULL;const xrtownershippreparationv1* plan=NULL;const XL6_OwnershipFinishV1* completion=NULL;
    assert(xl6OwnershipCatalogResolveV1(ref,catalog,&adapter,&plan,&completion)&&adapter&&plan&&!completion);
    assert(adapter->Hold(ref.Data));assert(ref.Ops->Count(ref.Data,&count)&&count==5);
    xrtownershipresult graph={0};bool inspected=xrtOwnershipInspect(&ref,1,NULL,0,&graph);
    // Receive uses an object carrier here; send has neither carrier nor record.
    assert(inspected&&graph.NodeCount==(sending?7u:11u));
    assert(ref.Ops->Trace(ref.Data,pin_subscription,pin)&&pin->adapter);
    assert(xrtOwnershipScopeEnd(&freeze));adapter->Drop(ref.Data);xl6OwnershipCatalogDestroyV1(catalog);
}
static void only_pin(XL6_ChannelFutureFrameV1* frame)
{
    xrtownershipscope freeze={0};XL6_DeadlineV1 deadline=xl6DeadlineAfterMillisecondsV1(10000);bool done=false;
    while(!done){
        if(xrtOwnershipFreezeTryBegin(&freeze)){xrtownershipref ref=api->Ownership(frame);size_t count=0;
            done=ref.Ops->Count(ref.Data,&count)&&count==1;assert(xrtOwnershipScopeEnd(&freeze));}
        if(!done){assert(!xl6DeadlineExpiredV1(deadline));xrtThreadYield();}
    }
}
typedef struct Probe {unsigned dropped,failure;uint64_t thread;} Probe;
static void probe_drop(void* data,void* context)
{
    Probe* probe=data;assert(!context);++probe->dropped;probe->thread=xrtThreadCurrentId();
    if(probe->failure==1)xl6ErrorScopePropagateFailure((xl6_error_result){false,NULL});
    if(probe->failure==2)xrtSetErrorKind(XERR_RANGE);
}
static const xvaluehandleops probeOps={NULL,probe_drop,NULL,NULL};
typedef struct Race {xvalue* q;bool sending;atomic_bool entered,go;} Race;
static int32_t race_peer(void* data)
{
    Race* race=data;atomic_store_explicit(&race->entered,true,memory_order_release);
    while(!atomic_load_explicit(&race->go,memory_order_acquire))xrtThreadYield();
    if(race->sending)assert(take(race->q)==11);
    else {int64_t value=44;send(race->q,1,&value);}
    return 0;
}
static bool result_edge(xrtownershipref ref,void* data)
{assert(!((xrtownershipref*)data)->Data&&ref.Data&&ref.Ops);*(xrtownershipref*)data=ref;return true;}
int main(void)
{
    assert(xrtMemDebugEnable(true));api=xl6ChannelDirectFutureRuntimeV1Get();assert(api);
    for(unsigned sending=0;sending<2;++sending)for(unsigned mode=0;mode<7;++mode){
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
        XL6_ContinuationExecutor* executor=xl6ContinuationExecutorCreateFixedV1(1,1);assert(executor);
        xvalue* q=channel(1,1);int64_t input=44,old=11;
        bool ready=mode==0||mode==6;
        if(sending&&!ready&&mode!=2)send(q,1,&old);
        if(!sending&&ready)send(q,1,&input);
        if(mode==2)assert(xl6ChannelCloseV1(q,ID));
        xcancel* parent=xrtCancelCreate();assert(parent);
        if(mode==3||mode==6)assert(xrtCancelRequest(parent));
        XL6_ChannelFutureFrameV1* frame=sending?api->PrepareSend(layout(1),q,&input):api->PrepareReceive(layout(1),q);assert(frame);
        xvalue* owner=NULL;bool object=!sending&&(mode&1);
        if(object)owner=carrier(frame);
        if(!sending){int64_t status=987;assert(!xl6ChannelReceiveResultInfoV1(record(api->ResultHandle(frame),false),&status)&&status==987);xrtClearError();}
        input=99; // send preparation owns its one value snapshot.
        xfuture* future=sending?api->StartSend(executor,frame,parent):api->StartReceive(executor,frame,
            object?owner:api->ResultHandle(frame),object?"native":NULL,object?6:0,parent);assert(future);xrtValueRelease(owner);
        Pin pin={0};if(mode==1)inspect(frame,sending,&pin);
        if(mode==1){if(sending)assert(take(q)==old);else {int64_t value=44;send(q,1,&value);}}
        if(mode==4)assert(xrtCancelRequest(parent));
        if(mode==5)assert(xrtFutureCancel(future));
        int64_t expected=mode==2?XL6_CHANNEL_CLOSED_V1:(mode>=3&&mode<=5)?XL6_CHANNEL_CANCELLED_V1:XL6_CHANNEL_OK_V1;
        if(sending){assert(sent(future)==expected);if(expected==XL6_CHANNEL_OK_V1)assert(take(q)==44);}
        else {XL6_ChannelReceiveResultV1* result=record(received(future),object);int64_t status=-1,value=0;
            assert(xl6ChannelReceiveResultInfoV1(result,&status)&&status==expected);
            if(status==XL6_CHANNEL_OK_V1){assert(xl6ChannelReceiveResultReadV1(profile(1),result,&value)&&value==44);
                XL6_FutureValueProfileV1 bad=profile(1);bad.type++;value=71;
                assert(!xl6ChannelReceiveResultReadV1(bad,result,&value)&&value==71);xrtClearError();}
            else {value=71;assert(!xl6ChannelReceiveResultReadV1(profile(1),result,&value)&&value==71);xrtClearError();}}
        idle(executor);if(pin.adapter){only_pin(frame);pin.adapter->Drop(pin.ref.Data);}
        xrtFutureDestroy(future);xrtCancelDestroy(parent);xrtValueRelease(q);finish(executor);balanced(&before);
    }
    // Exact transfer mismatch is rejected before a send copy or native start.
    for(unsigned sending=0;sending<2;++sending){xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
        xvalue* q=channel(1,1);int64_t input=12;XL6_SequenceLayoutV1 bad=layout(1);bad.transfer=2;
        XL6_ChannelFutureFrameV1* frame=sending?api->PrepareSend(bad,q,&input):api->PrepareReceive(bad,q);
        assert(!frame);xrtClearError();assert(xl6ChannelQueryV1(q,ID,XL6_CHANNEL_QUERY_COUNT_V1)==0);
        xrtValueRelease(q);balanced(&before);}
    // Receive commits directly into the owned record; typed reads snapshot NUL text.
    for(unsigned transfer=2;transfer<=3;++transfer){xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
        XL6_ContinuationExecutor* executor=xl6ContinuationExecutorCreateFixedV1(2,1);assert(executor);
        xvalue* q=channel(transfer,0);xstrview text=XRT_STR_LITERAL("a\0你🙂");
        unsigned char raw[]={0,0xff,0x61};xvalue* binary=transfer==3?xrtValueBytes((xbytesview){raw,sizeof(raw)}):NULL;
        XL6_ChannelFutureFrameV1* recvFrame=api->PrepareReceive(layout(transfer),q);assert(recvFrame);
        xfuture* recv=api->StartReceive(executor,recvFrame,api->ResultHandle(recvFrame),NULL,0,NULL);assert(recv);
        XL6_ChannelFutureFrameV1* sendFrame=api->PrepareSend(layout(transfer),q,transfer==2?(void*)&text:(void*)&binary);assert(sendFrame);
        xrtValueRelease(binary);xfuture* sending=api->StartSend(executor,sendFrame,NULL);assert(sending&&sent(sending)==XL6_CHANNEL_OK_V1);
        XL6_ChannelReceiveResultV1* result=record(received(recv),false);xrtValueRelease(q);q=NULL;
        if(transfer==2){xstrview copy={0};assert(xl6ChannelReceiveResultReadV1(profile(2),result,&copy)&&copy.Size==text.Size&&!memcmp(copy.Data,text.Data,text.Size));
            xrtTypeDropValue(xl6StringTypeV1(),&copy);}
        else {xvalue* copy=NULL;xbytesview bytes={0};assert(xl6ChannelReceiveResultReadV1(profile(3),result,&copy)&&xrtValueGetBytes(copy,&bytes));
            assert(bytes.Size==sizeof(raw)&&!memcmp(bytes.Data,raw,sizeof(raw)));xrtValueRelease(copy);}
        xrtFutureDestroy(sending);xrtFutureDestroy(recv);finish(executor);balanced(&before);
    }
    // A successful nullable receive is distinct from an absent value/status.
    {xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
        XL6_ContinuationExecutor* executor=xl6ContinuationExecutorCreateFixedV1(1,1);assert(executor);
        xvalue* q=channel(3,1);xvalue* nil=NULL;send(q,3,&nil);
        XL6_ChannelFutureFrameV1* frame=api->PrepareReceive(layout(3),q);assert(frame);
        xfuture* future=api->StartReceive(executor,frame,api->ResultHandle(frame),NULL,0,NULL);assert(future);
        XL6_ChannelReceiveResultV1* result=record(received(future),false);int64_t status=-1;xvalue* copy=NULL;
        assert(xl6ChannelReceiveResultInfoV1(result,&status)&&status==XL6_CHANNEL_OK_V1);
        assert(xl6ChannelReceiveResultReadV1(profile(3),result,&copy)&&!copy);
        xrtFutureDestroy(future);xrtValueRelease(q);finish(executor);balanced(&before);
    }
    // Allocation failure while COPYING a string never publishes a partial slot.
    {xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
        XL6_ContinuationExecutor* executor=xl6ContinuationExecutorCreateFixedV1(1,1);assert(executor);
        xvalue* q=channel(2,1);xstrview text=XRT_STR_LITERAL("a\0copy");send(q,2,&text);
        XL6_ChannelFutureFrameV1* frame=api->PrepareReceive(layout(2),q);assert(frame);
        xfuture* future=api->StartReceive(executor,frame,api->ResultHandle(frame),NULL,0,NULL);assert(future);
        XL6_ChannelReceiveResultV1* result=record(received(future),false);xstrview copy={0};
        idle(executor);assert(xrtMemDebugFailAfter(0));
        assert(!xl6ChannelReceiveResultReadV1(profile(2),result,&copy)&&!copy.Data&&!copy.Size&&xrtMemDebugFailTriggered());
        xrtMemDebugFailClear();xrtClearError();
        assert(xl6ChannelReceiveResultReadV1(profile(2),result,&copy)&&copy.Size==text.Size&&!memcmp(copy.Data,text.Data,text.Size));
        xrtTypeDropValue(xl6StringTypeV1(),&copy);xrtFutureDestroy(future);xrtValueRelease(q);finish(executor);balanced(&before);
    }
    // Full/closed executor refuses publication and preserves private rollback.
    for(unsigned sending=0;sending<2;++sending)for(unsigned closed=0;closed<2;++closed){
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
        XL6_ContinuationExecutor* executor=xl6ContinuationExecutorCreateFixedV1(1,1);assert(executor);
        xvalue* q=channel(1,0);xfuture* accepted=NULL;
        if(!closed){XL6_ChannelFutureFrameV1* first=api->PrepareReceive(layout(1),q);assert(first);
            accepted=api->StartReceive(executor,first,api->ResultHandle(first),NULL,0,NULL);assert(accepted);}
        else xl6ContinuationExecutorClose(executor);
        int64_t input=17;XL6_ChannelFutureFrameV1* frame=sending?api->PrepareSend(layout(1),q,&input):api->PrepareReceive(layout(1),q);assert(frame);
        assert(!(sending?api->StartSend(executor,frame,NULL):api->StartReceive(executor,frame,api->ResultHandle(frame),NULL,0,NULL)));
        assert(xrtErrorKind(xrtGetError())==(closed?XERR_STATE:XERR_RANGE));api->Discard(frame);xrtClearError();
        xl6ContinuationExecutorClose(executor);
        if(accepted){assert(xrtFutureCancel(accepted));(void)received(accepted);xrtFutureDestroy(accepted);}
        xrtValueRelease(q);finish(executor);balanced(&before);
    }
    for(unsigned round=0;round<200;++round){
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);bool sending=(round&1)!=0;
        XL6_ContinuationExecutor* executor=xl6ContinuationExecutorCreateFixedV1(1,1);assert(executor);
        xvalue* q=channel(1,1);int64_t input=44,old=11;if(sending)send(q,1,&old);
        XL6_ChannelFutureFrameV1* frame=sending?api->PrepareSend(layout(1),q,&input):api->PrepareReceive(layout(1),q);assert(frame);
        Race race={.q=q,.sending=sending};xthread* peer=xrtThreadCreate(race_peer,&race,0);assert(peer);
        XL6_DeadlineV1 deadline=xl6DeadlineAfterMillisecondsV1(10000);
        while(!atomic_load_explicit(&race.entered,memory_order_acquire)){assert(!xl6DeadlineExpiredV1(deadline));xrtThreadYield();}
        atomic_store_explicit(&race.go,true,memory_order_release);
        xfuture* future=sending?api->StartSend(executor,frame,NULL):api->StartReceive(executor,frame,api->ResultHandle(frame),NULL,0,NULL);assert(future);
        if(sending){assert(sent(future)==XL6_CHANNEL_OK_V1&&take(q)==44);}
        else {int64_t value=0;assert(xl6ChannelReceiveResultReadV1(profile(1),record(received(future),false),&value)&&value==44);}
        assert(xrtThreadWait(peer)==XWAIT_OK);xrtThreadDestroy(peer);xrtFutureDestroy(future);xrtValueRelease(q);finish(executor);balanced(&before);
    }
    // Two suspended native producers rendezvous without blocking the one worker.
    for(unsigned round=0;round<50;++round){xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
        XL6_ContinuationExecutor* executor=xl6ContinuationExecutorCreateFixedV1(2,1);assert(executor);
        xvalue* q=channel(1,0);int64_t input=(int64_t)round;
        XL6_ChannelFutureFrameV1* left=api->PrepareSend(layout(1),q,&input);
        XL6_ChannelFutureFrameV1* right=api->PrepareReceive(layout(1),q);assert(left&&right);
        xfuture* sender=NULL;xfuture* receiver=NULL;
        if(round&1){receiver=api->StartReceive(executor,right,api->ResultHandle(right),NULL,0,NULL);
            sender=api->StartSend(executor,left,NULL);}
        else {sender=api->StartSend(executor,left,NULL);
            receiver=api->StartReceive(executor,right,api->ResultHandle(right),NULL,0,NULL);}
        assert(sender&&receiver&&sent(sender)==XL6_CHANNEL_OK_V1);int64_t value=-1;
        assert(xl6ChannelReceiveResultReadV1(profile(1),record(received(receiver),false),&value)&&value==input);
        xrtFutureDestroy(sender);xrtFutureDestroy(receiver);xrtValueRelease(q);finish(executor);balanced(&before);
    }
    // Full four allocation prefixes: send/receive preparation and start.
    unsigned prefixes[4]={0};
    for(unsigned phase=0;phase<4;++phase)for(size_t point=0;point<128;++point){
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
        XL6_ContinuationExecutor* executor=xl6ContinuationExecutorCreateFixedV1(1,1);assert(executor);
        xvalue* q=channel(1,1);int64_t input=31;bool sending=(phase&1)==0;
        if(!sending)send(q,1,&input);
        XL6_ChannelFutureFrameV1* frame=NULL;xfuture* future=NULL;
        if(phase>=2){frame=sending?api->PrepareSend(layout(1),q,&input):api->PrepareReceive(layout(1),q);assert(frame);}
        assert(xrtMemDebugFailAfter(point));
        if(phase<2)frame=sending?api->PrepareSend(layout(1),q,&input):api->PrepareReceive(layout(1),q);
        else future=sending?api->StartSend(executor,frame,NULL):api->StartReceive(executor,frame,api->ResultHandle(frame),NULL,0,NULL);
        bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();xrtClearError();
        if(future){if(sending)assert(sent(future)==XL6_CHANNEL_OK_V1);else (void)received(future);xrtFutureDestroy(future);}
        else if(frame)api->Discard(frame);
        xrtValueRelease(q);finish(executor);balanced(&before);
        if(!hit){prefixes[phase]=(unsigned)point;break;}assert(point<127);
    }
    // One reference copy, worker-only unsent cleanup, false/NULL and Range errors.
    for(unsigned failure=0;failure<3;++failure){xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);Probe probe={0};probe.failure=failure;
        XL6_ContinuationExecutor* executor=xl6ContinuationExecutorCreateFixedV1(1,1);assert(executor);
        xvalue* q=channel(3,1);xvalue* nil=NULL;send(q,3,&nil);
        void* raw=&probe;xvalue* input=xrtValueHandleTake(&raw,&probeOps,NULL);assert(input&&!raw);
        XL6_ChannelFutureFrameV1* frame=api->PrepareSend(layout(3),q,&input);assert(frame);
        xrtownershipref inputRef=xrtValueOwnership(input);size_t refs=0;assert(inputRef.Ops->Count(inputRef.Data,&refs)&&refs==2);
        xrtValueRelease(input);uint64_t producer=xrtThreadCurrentId();xfuture* future=api->StartSend(executor,frame,NULL);assert(future);
        assert(xrtMemDebugFailAfter(0));assert(xrtFutureCancel(future));
        if(!failure)assert(sent(future)==XL6_CHANNEL_CANCELLED_V1);
        else {xfutureresult value=outcome(future);assert(value.State==XFUTURE_FAILED&&value.Error&&xrtErrorKind(value.Error)==(failure==1?XERR_INTERNAL:XERR_RANGE));}
        idle(executor);assert(probe.dropped==1&&probe.thread!=producer&&!xrtMemDebugFailTriggered());xrtMemDebugFailClear();
        xrtFutureDestroy(future);xrtValueRelease(q);finish(executor);balanced(&before);
    }
    // No allocation from native readiness through completion or primitive read.
    {xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
        XL6_ContinuationExecutor* executor=xl6ContinuationExecutorCreateFixedV1(1,1);assert(executor);
        xvalue* q=channel(1,1);int64_t input=17;XL6_ChannelStatusV1 status;XL6_ChannelOperationV1 sender={0};
        XL6_ChannelFutureFrameV1* frame=api->PrepareReceive(layout(1),q);assert(frame);
        xfuture* future=api->StartReceive(executor,frame,api->ResultHandle(frame),NULL,0,NULL);assert(future);
        assert(xl6ChannelOperationOpenSchemaV1(layout(1),q,SLOT(input),true,&status,&sender));
        assert(xrtMemDebugFailAfter(0));assert(xrtChannelSendFor(sender.native,sender.message,xl6DeadlineRemainingMillisecondsV1(0))==XWAIT_OK);
        assert(xl6ChannelOperationCommitV1(&sender));xl6ChannelOperationCloseV1(&sender);
        int64_t value=0;assert(xl6ChannelReceiveResultReadV1(profile(1),record(received(future),false),&value)&&value==17);
        idle(executor);assert(!xrtMemDebugFailTriggered());xrtMemDebugFailClear();
        xrtFutureDestroy(future);xrtValueRelease(q);finish(executor);balanced(&before);
    }
    // Result nodes participate in the real catalog. A physical pin cannot keep
    // a cleared value alive; Finish runs semantic cleanup outside Freeze once.
    for(unsigned failure=0;failure<3;++failure){xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);Probe probe={.failure=failure};
        XL6_ContinuationExecutor* executor=xl6ContinuationExecutorCreateFixedV1(1,1);assert(executor);
        xvalue* q=channel(3,1);void* raw=&probe;xvalue* input=xrtValueHandleTake(&raw,&probeOps,NULL);assert(input);
        send(q,3,&input);xrtValueRelease(input);
        XL6_ChannelFutureFrameV1* frame=api->PrepareReceive(layout(3),q);assert(frame);
        xfuture* future=api->StartReceive(executor,frame,api->ResultHandle(frame),NULL,0,NULL);assert(future);
        const xvalue* handle=received(future);idle(executor);xrtownershipref ref={0};
        const XL6_OwnershipDomainV1* domain=xl6OwnershipDomainChannelDirectFutureV1Get();
        XL6_OwnershipCatalog* catalog=xl6OwnershipCatalogCreateV1(&domain,1);assert(catalog);
        xrtownershipscope freeze={0};assert(xrtOwnershipFreezeTryBegin(&freeze));
        assert(xl6ChannelReceiveResultHandlePolicyV1Get()->Trace(handle,result_edge,&ref));
        const xrtownershipadapterv1* adapter=NULL;const xrtownershippreparationv1* plan=NULL;const XL6_OwnershipFinishV1* completion=NULL;
        size_t refs=0,edges=0;assert(ref.Ops->Count(ref.Data,&refs)&&refs==1);
        assert(ref.Ops->Trace(ref.Data,edge,&edges)&&edges==1);
        assert(xl6OwnershipCatalogResolveV1(ref,catalog,&adapter,&plan,&completion)&&adapter&&!plan&&!completion&&adapter->Finish);
        assert(adapter->Hold(ref.Data));assert(xrtOwnershipScopeEnd(&freeze));
        xrtFutureDestroy(future);xrtValueRelease(q);assert(!probe.dropped);int token=0;
        assert(xrtOwnershipFreezeTryBegin(&freeze));assert(ref.Ops->Count(ref.Data,&refs)&&refs==1);
        assert(adapter->Claim(ref.Data,&token));adapter->Restore(ref.Data,&token);
        assert(adapter->Claim(ref.Data,&token));adapter->Clear(ref.Data,&token);assert(xrtOwnershipScopeEnd(&freeze));
        xl6_error_scope scope={0};assert(xl6ErrorScopeBegin(&scope));bool ok=adapter->Finish(ref.Data,&token);
        if(!ok)xl6ErrorScopeCaptureFailure(&scope);
        xl6_error_result result=xl6ErrorScopeEnd(&scope);assert(result.ok==!failure);
        if(failure)assert(!result.error||xrtErrorKind(result.error)==XERR_RANGE);
        xrtErrorFree(result.error);xrtClearError();assert(probe.dropped==1);
        adapter->Drop(ref.Data);assert(probe.dropped==1);xl6OwnershipCatalogDestroyV1(catalog);finish(executor);balanced(&before);
    }
    {enum{N=512};xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
        XL6_ContinuationExecutor* executor=xl6ContinuationExecutorCreateFixedV1(N,1);assert(executor);xvalue* q=channel(1,N);xfuture* jobs[N];
        for(size_t i=0;i<N;++i){XL6_ChannelFutureFrameV1* frame=api->PrepareReceive(layout(1),q);assert(frame);
            jobs[i]=api->StartReceive(executor,frame,api->ResultHandle(frame),NULL,0,NULL);assert(jobs[i]);}
        xl6ContinuationExecutorClose(executor);
        for(size_t i=0;i<N;++i){int64_t value=(int64_t)i;send(q,1,&value);}
        for(size_t i=0;i<N;++i){int64_t value=-1;assert(xl6ChannelReceiveResultReadV1(profile(1),record(received(jobs[i]),false),&value)&&value==(int64_t)i);xrtFutureDestroy(jobs[i]);}
        xrtValueRelease(q);finish(executor);balanced(&before);
    }
    printf("direct Channel Future: send/receive ready/pending/closed/pre-parent-output cancel, ready-before-stop; typed records without Channel retention, OK-null/NUL/binary and copy OOM, exact schema/profile refusal; real frame/catalog/subscription pins and result Clear/Finish; full/Close rollback;200 producer/register races and 50 async rendezvous pairs; one message reference copy, worker-only cleanup and false-NULL/Range failures; zero late allocation;512 one-worker waits; prepare/start prefixes %u/%u/%u/%u; balanced\n",prefixes[0],prefixes[1],prefixes[2],prefixes[3]);
    return 0;
}
