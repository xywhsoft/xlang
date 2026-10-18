/* Real unbounded native producer, no language/finite-deadline claim. */
#include "src/runtime/xl6_channel_future.h"
#include "src/runtime/xl6_channel_case.h"
#include "src/runtime/xl6_channel_internal.h"
#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_ownership_domains.h"
#include "src/api/xl_foreign_error.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdatomic.h>

#define ID UINT64_C(0xCA7101)
#define ELEMENT UINT64_C(0xCA7102)
#define SLOT(Value) (XL6_ChannelSlotV1){&(Value),sizeof(Value)}
static const XL6_ChannelFutureRuntimeV1* api;
static xvalue* channel(size_t capacity)
{
    xvalue* schema=xl6SequencePrimitiveCreateV1(1,123,ELEMENT,1,sizeof(int64_t),xrtTypeInt64(),
        XL6_VALUE_ADMIT_I64,false,ELEMENT);assert(schema);
    xvalue* result=xl6ChannelCreateTakeV1(ID,schema,(int64_t)capacity);assert(result);return result;
}
static void send(xvalue* q,int64_t value)
{
    XL6_ChannelStatusV1 status;
    assert(xl6ChannelSendV1(q,ID,ELEMENT,xrtTypeInt64(),SLOT(value),XL6_CHANNEL_WAIT_V1,0,&status));
    assert(status==XL6_CHANNEL_OK_V1);
}
static XL6_ChannelSelectionOwnerV1* selection(const xvalue* value,bool object)
{
    const xvalue* handle=object?xrtValueObjectGet(value,XRT_STR_LITERAL("native")):value;
    void *data=NULL,*context=NULL;const xvaluehandleops* ops=NULL;
    assert(xrtValueGetHandle(handle,&data,&ops,&context)&&data&&!context);
    assert(ops==xl6ChannelSelectionHandlePolicyV1Get()->Ops);return data;
}
static const xvalue* outcome(xfuture* future)
{
    assert(xrtFutureWaitFor(future,10000000)==XWAIT_OK);
    xfutureresult result={0};assert(xrtFutureResult(future,&result)&&result.State==XFUTURE_RESOLVED&&result.Value);
    return *(xvalue**)result.Value;
}
static void idle(XL6_ContinuationExecutor* executor)
{
    xdeadline deadline=xrtDeadlineAfter(10000000);
    while(!xl6ContinuationExecutorIsIdle(executor)){assert(!xrtDeadlineExpired(deadline));xrtThreadYield();}
}
static void finish(XL6_ContinuationExecutor* executor)
{idle(executor);xl6ContinuationExecutorClose(executor);assert(xl6ContinuationExecutorDestroy(executor));}
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtClearError();xrtMemDebugSnapshot(&after);
    assert(before->LiveCount==after.LiveCount&&before->LiveBytes==after.LiveBytes&&
        after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount&&
        before->InvalidFreeCount==after.InvalidFreeCount&&before->DoubleFreeCount==after.DoubleFreeCount&&
        before->UseAfterFreeCount==after.UseAfterFreeCount);
}
static bool edge(xrtownershipref ref,void* context)
{if(ref.Data){assert(ref.Ops);++*(size_t*)context;}return true;}
static unsigned policyVisits;
static bool policies(xrtownershipref ref,const XL6_OwnershipPoliciesV1* policies,
    const xrtownershipadapterv1** adapter,const xrtownershippreparationv1** plan,const XL6_OwnershipFinishV1** finish)
{
    (void)ref;(void)adapter;(void)plan;(void)finish;
    assert(policies->ChannelSubscriptionCount==1&&policies->ChannelSubscription[0]==xl6ChannelFutureSubscriptionPolicyV1Get());
    assert(policies->ProducerCount==1&&policies->CancelWatchCount==1&&policies->QueuedStorageCount==1);
    ++policyVisits;return false;
}
typedef struct NativePin {xrtownershipref ref;const xrtownershipadapterv1* adapter;} NativePin;
static bool pin_subscription(xrtownershipref ref,void* context)
{
    NativePin* pin=context;if(pin->adapter)return true;
    const xchannelsubscribeownershipv1* policy=xl6ChannelFutureSubscriptionPolicyV1Get();
    const xrtownershipadapterv1* adapter=xrtChannelSubscribeOwnershipAdapterV1(ref,&policy,1);
    if(adapter){assert(adapter->Hold(ref.Data));pin->ref=ref;pin->adapter=adapter;}
    return true;
}
static void inspect(XL6_ChannelFutureFrameV1* frame,NativePin* pin)
{
    const XL6_OwnershipDomainV1 observer={sizeof(observer),NULL,policies,NULL};
    const XL6_OwnershipDomainV1* domains[]={&observer,xl6OwnershipDomainChannelFutureV1Get()};
    XL6_OwnershipCatalog* catalog=xl6OwnershipCatalogCreateV1(domains,2);assert(catalog);
    xrtownershipscope freeze={0};xdeadline deadline=xrtDeadlineAfter(10000000);
    while(!xrtOwnershipFreezeTryBegin(&freeze)){assert(!xrtDeadlineExpired(deadline));xrtThreadYield();}
    xrtownershipref ref=api->Ownership(frame);size_t count=0,edges=0;
    assert(ref.Ops->Count(ref.Data,&count)&&count==4);
    assert(ref.Ops->Trace(ref.Data,edge,&edges)&&edges==8);
    const xrtownershippreparationv1* plan=NULL;
    const xrtownershipadapterv1* adapter=xl6ChannelFutureOwnershipAdapterV1(ref,&plan);
    assert(adapter&&plan&&adapter->Hold(ref.Data));
    assert(ref.Ops->Count(ref.Data,&count)&&count==5);
    const xrtownershipadapterv1* admitted=NULL;const xrtownershippreparationv1* preparation=NULL;
    const XL6_OwnershipFinishV1* finish=NULL;xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);
    unsigned observed=policyVisits;
    assert(xl6OwnershipCatalogResolveV1(ref,catalog,&admitted,&preparation,&finish)&&admitted==adapter&&preparation==plan&&!finish);
    assert(policyVisits==observed+1);xrtMemDebugSnapshot(&after);
    assert(before.AllocCount==after.AllocCount&&before.FreeCount==after.FreeCount);
    xrtownershipresult graph={0};assert(xrtOwnershipInspect(&ref,1,NULL,0,&graph)&&graph.NodeCount>=10);
    assert(ref.Ops->Trace(ref.Data,pin_subscription,pin)&&pin->adapter);
    assert(xrtOwnershipScopeEnd(&freeze));adapter->Drop(ref.Data);xl6OwnershipCatalogDestroyV1(catalog);
}
typedef struct Probe {unsigned dropped;uint64_t thread;unsigned failure;} Probe;
static void probe_drop(void* value,void* context)
{
    Probe* probe=value;assert(!context);++probe->dropped;probe->thread=xrtThreadCurrentId();
    if(probe->failure==1)xl6ErrorScopePropagateFailure((xl6_error_result){false,NULL});
    if(probe->failure==2)xrtSetErrorKind(XERR_RANGE);
}
static const xvaluehandleops probeOps={NULL,probe_drop,NULL,NULL};
typedef struct Race {xvalue* channel;atomic_bool entered,go;} Race;
static int32_t race_send(void* data)
{
    Race* race=data;atomic_store_explicit(&race->entered,true,memory_order_release);
    while(!atomic_load_explicit(&race->go,memory_order_acquire))xrtThreadYield();
    send(race->channel,234);return 0;
}
int main(void)
{
    assert(xrtMemDebugEnable(true));api=xl6ChannelFutureRuntimeV1Get();assert(api);
    for(unsigned mode=0;mode<6;++mode)for(unsigned object=0;object<2;++object){
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
        XL6_ContinuationExecutor* executor=xl6ContinuationExecutorCreateFixedV1(4,1);assert(executor);
        xvalue* q=channel(1);
        XL6_ChannelCaseOwnerV1* item=xl6ChannelReceiveCaseCreateV1(q,ID,ELEMENT,xrtTypeInt64(),sizeof(int64_t));assert(item);
        if(mode==0)send(q,217);
        if(mode==2)assert(xl6ChannelCloseV1(q,ID));
        xcancel* parent=xrtCancelCreate();assert(parent);
        if(mode==5)assert(xrtCancelRequest(parent));
        XL6_ChannelFutureFrameV1* frame=api->Prepare(&item,1);assert(frame);
        const xvalue* handle=api->ResultHandle(frame);assert(handle);
        xvalue* owner=object?xrtValueObject():NULL;
        if(object)assert(owner&&xrtValueObjectSet(owner,XRT_STR_LITERAL("native"),handle));
        xfuture* future=api->Start(executor,frame,object?owner:handle,object?"native":NULL,object?6:0,parent);assert(future);
        xrtValueRelease(owner);
        NativePin pin={0};
        if(mode==1){
            inspect(frame,&pin);XL6_ChannelOperationV1 sender={0};XL6_ChannelStatusV1 status;int64_t value=218;
            assert(xl6ChannelOperationOpenV1(q,ID,ELEMENT,xrtTypeInt64(),SLOT(value),true,&status,&sender));
            assert(xrtMemDebugFailAfter(0));
            assert(xrtChannelSendUntil(sender.native,sender.message,XRT_DEADLINE_NEVER)==XWAIT_OK);
            assert(xl6ChannelOperationCommitV1(&sender));xl6ChannelOperationCloseV1(&sender);
        }
        if(mode==3)assert(xrtFutureCancel(future));
        if(mode==4)assert(xrtCancelRequest(parent));
        const xvalue* result=outcome(future);XL6_ChannelSelectionV1 info;
        assert(xl6ChannelSelectionInfoV1(selection(result,object!=0),&info));
        if(mode<2){int64_t received=0;assert(info.index==0&&info.status==XL6_CHANNEL_OK_V1);
            assert(xl6ChannelSelectionValueV1(selection(result,object!=0),item,ELEMENT,xrtTypeInt64(),SLOT(received)));
            assert(received==(mode?218:217));}
        else assert(info.status==(mode==2?XL6_CHANNEL_CLOSED_V1:XL6_CHANNEL_CANCELLED_V1));
        idle(executor);if(mode==1){
            assert(!xrtMemDebugFailTriggered());xrtMemDebugFailClear();
            /* An inspection pin preserves physical storage, not accepted
             * logical work. Completion must NOT wait for this independent pin. */
            xrtownershipscope freeze={0};assert(xrtOwnershipFreezeTryBegin(&freeze));
            xrtownershipref retained=api->Ownership(frame);size_t references=0;
            assert(retained.Ops->Count(retained.Data,&references)&&references==1);
            assert(xrtOwnershipScopeEnd(&freeze));pin.adapter->Drop(pin.ref.Data);
        }
        xrtFutureDestroy(future);xrtCancelDestroy(parent);xl6ChannelCaseDestroyV1(item);xrtValueRelease(q);finish(executor);balanced(&before);
    }
    unsigned createPrefix=0,startPrefix=0,largePrefix=0;
    for(unsigned phase=0;phase<3;++phase)for(size_t point=0;point<256;++point){
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
        XL6_ContinuationExecutor* executor=xl6ContinuationExecutorCreateFixedV1(1,1);assert(executor);
        xvalue* q=channel(1);XL6_ChannelCaseOwnerV1* item=xl6ChannelReceiveCaseCreateV1(q,ID,ELEMENT,xrtTypeInt64(),sizeof(int64_t));assert(item);
        XL6_ChannelFutureFrameV1* frame=NULL;xfuture* future=NULL;
        XL6_ChannelCaseOwnerV1* many[12];for(size_t i=0;i<12;++i)many[i]=item;
        if(phase==1){frame=api->Prepare(&item,1);assert(frame);}
        assert(xrtMemDebugFailAfter(point));
        if(phase!=1)frame=api->Prepare(phase==2?many:&item,phase==2?12:1);
        else future=api->Start(executor,frame,api->ResultHandle(frame),NULL,0,NULL);
        bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();xrtClearError();
        if(future){send(q,219);(void)outcome(future);xrtFutureDestroy(future);idle(executor);}
        else if(frame)api->Discard(frame);
        xl6ChannelCaseDestroyV1(item);xrtValueRelease(q);finish(executor);balanced(&before);
        if(!hit){if(phase==1)startPrefix=(unsigned)point;else if(phase==2)largePrefix=(unsigned)point;else createPrefix=(unsigned)point;break;}
        assert(point<255);
    }
    for(unsigned round=0;round<50;++round){
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
        XL6_ContinuationExecutor* executor=xl6ContinuationExecutorCreateFixedV1(2,1);assert(executor);
        xvalue* q=channel(0);int64_t input=233;
        XL6_ChannelCaseOwnerV1* sender=xl6ChannelSendCaseCreateV1(q,ID,ELEMENT,xrtTypeInt64(),SLOT(input));
        XL6_ChannelCaseOwnerV1* receiver=xl6ChannelReceiveCaseCreateV1(q,ID,ELEMENT,xrtTypeInt64(),sizeof(input));assert(sender&&receiver);
        XL6_ChannelFutureFrameV1 *left=api->Prepare(&sender,1),*right=api->Prepare(&receiver,1);assert(left&&right);
        xfuture* sent=api->Start(executor,left,api->ResultHandle(left),NULL,0,NULL);assert(sent);
        xfuture* received=api->Start(executor,right,api->ResultHandle(right),NULL,0,NULL);assert(received);
        XL6_ChannelSelectionV1 info;assert(xl6ChannelSelectionInfoV1(selection(outcome(sent),false),&info)&&info.status==XL6_CHANNEL_OK_V1);
        int64_t output=0;assert(xl6ChannelSelectionValueV1(selection(outcome(received),false),receiver,ELEMENT,xrtTypeInt64(),SLOT(output))&&output==233);
        xrtFutureDestroy(sent);xrtFutureDestroy(received);idle(executor);
        xl6ChannelCaseDestroyV1(sender);xl6ChannelCaseDestroyV1(receiver);xrtValueRelease(q);finish(executor);balanced(&before);
    }
    for(unsigned closed=0;closed<2;++closed){
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
        XL6_ContinuationExecutor* executor=xl6ContinuationExecutorCreateFixedV1(1,1);assert(executor);
        xvalue* q=channel(0);XL6_ChannelCaseOwnerV1* item=xl6ChannelReceiveCaseCreateV1(q,ID,ELEMENT,xrtTypeInt64(),sizeof(int64_t));assert(item);
        xfuture* accepted=NULL;
        if(!closed){XL6_ChannelFutureFrameV1* first=api->Prepare(&item,1);assert(first);
            accepted=api->Start(executor,first,api->ResultHandle(first),NULL,0,NULL);assert(accepted);}
        else xl6ContinuationExecutorClose(executor);
        XL6_ChannelFutureFrameV1* frame=api->Prepare(&item,1);assert(frame);
        assert(!api->Start(executor,frame,api->ResultHandle(frame),NULL,0,NULL));
        assert(xrtErrorKind(xrtGetError())==(closed?XERR_STATE:XERR_RANGE));
        api->Discard(frame);xrtClearError();xl6ContinuationExecutorClose(executor);
        if(accepted){assert(xrtFutureCancel(accepted));(void)outcome(accepted);xrtFutureDestroy(accepted);}
        idle(executor);xl6ChannelCaseDestroyV1(item);xrtValueRelease(q);assert(xl6ContinuationExecutorDestroy(executor));balanced(&before);
    }
    for(unsigned failure=0;failure<3;++failure){
        enum {N=20};xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);Probe probe={0};
        probe.failure=failure;
        uint64_t producer=xrtThreadCurrentId();XL6_ContinuationExecutor* executor=xl6ContinuationExecutorCreateFixedV1(1,1);assert(executor);
        xvalue* schema=xl6SequenceReferenceCreateV1(1,123,ELEMENT,XL6_VALUE_ADMIT_ANY,true,ELEMENT);assert(schema);
        xvalue* q=xl6ChannelCreateTakeV1(ID,schema,0);assert(q);void* raw=&probe;
        xvalue* input=xrtValueHandleTake(&raw,&probeOps,NULL);assert(input&&!raw);
        XL6_ChannelCaseOwnerV1* item=xl6ChannelSendCaseCreateV1(q,ID,ELEMENT,xrtTypeValue(),SLOT(input));assert(item);
        XL6_ChannelCaseOwnerV1* cases[N];for(size_t i=0;i<N;++i)cases[i]=item;
        XL6_ChannelFutureFrameV1* frame=api->Prepare(cases,N);assert(frame);
        xfuture* future=api->Start(executor,frame,api->ResultHandle(frame),NULL,0,NULL);assert(future);
        xrtValueRelease(input);xl6ChannelCaseDestroyV1(item);assert(!probe.dropped);
        assert(xrtMemDebugFailAfter(0));assert(xrtFutureCancel(future));
        if(!failure){XL6_ChannelSelectionV1 info;assert(xl6ChannelSelectionInfoV1(selection(outcome(future),false),&info)&&info.status==XL6_CHANNEL_CANCELLED_V1);}
        else {xfutureresult result={0};assert(xrtFutureWaitFor(future,10000000)==XWAIT_OK&&xrtFutureResult(future,&result));
            assert(result.State==XFUTURE_FAILED&&result.Error&&xrtErrorKind(result.Error)==(failure==1?XERR_INTERNAL:XERR_RANGE));}
        idle(executor);assert(probe.dropped==1&&probe.thread!=producer&&!xrtMemDebugFailTriggered());xrtMemDebugFailClear();
        xrtFutureDestroy(future);xrtValueRelease(q);finish(executor);balanced(&before);
    }
    for(unsigned round=0;round<200;++round){
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
        XL6_ContinuationExecutor* executor=xl6ContinuationExecutorCreateFixedV1(1,1);assert(executor);
        xvalue* q=channel(1);XL6_ChannelCaseOwnerV1* item=xl6ChannelReceiveCaseCreateV1(q,ID,ELEMENT,xrtTypeInt64(),sizeof(int64_t));assert(item);
        XL6_ChannelFutureFrameV1* frame=api->Prepare(&item,1);assert(frame);Race race={.channel=q};
        xthread* producer=xrtThreadCreate(race_send,&race,0);assert(producer);
        xdeadline deadline=xrtDeadlineAfter(10000000);
        while(!atomic_load_explicit(&race.entered,memory_order_acquire)){assert(!xrtDeadlineExpired(deadline));xrtThreadYield();}
        atomic_store_explicit(&race.go,true,memory_order_release);
        xfuture* future=api->Start(executor,frame,api->ResultHandle(frame),NULL,0,NULL);assert(future);
        int64_t value=0;assert(xl6ChannelSelectionValueV1(selection(outcome(future),false),item,ELEMENT,xrtTypeInt64(),SLOT(value))&&value==234);
        assert(xrtThreadWait(producer)==XWAIT_OK);xrtThreadDestroy(producer);
        xrtFutureDestroy(future);idle(executor);xl6ChannelCaseDestroyV1(item);xrtValueRelease(q);finish(executor);balanced(&before);
    }
    {
        enum {N=512};xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
        XL6_ContinuationExecutor* executor=xl6ContinuationExecutorCreateFixedV1(N,1);assert(executor);
        xvalue* q=channel(N);XL6_ChannelCaseOwnerV1* item=xl6ChannelReceiveCaseCreateV1(q,ID,ELEMENT,xrtTypeInt64(),sizeof(int64_t));assert(item);
        xfuture* futures[N];
        for(size_t i=0;i<N;++i){XL6_ChannelFutureFrameV1* frame=api->Prepare(&item,1);assert(frame);
            futures[i]=api->Start(executor,frame,api->ResultHandle(frame),NULL,0,NULL);assert(futures[i]);}
        xl6ContinuationExecutorClose(executor);
        for(size_t i=0;i<N;++i)send(q,(int64_t)i);
        for(size_t i=0;i<N;++i){int64_t value=-1;
            assert(xl6ChannelSelectionValueV1(selection(outcome(futures[i]),false),item,ELEMENT,xrtTypeInt64(),SLOT(value)));
            assert(value==(int64_t)i);xrtFutureDestroy(futures[i]);}
        idle(executor);xl6ChannelCaseDestroyV1(item);xrtValueRelease(q);assert(xl6ContinuationExecutorDestroy(executor));balanced(&before);
    }
    for(unsigned mode=0;mode<3;++mode){
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
        XL6_ContinuationExecutor* executor=xl6ContinuationExecutorCreateFixedV1(1,1);assert(executor);
        xvalue* q=channel(1);
        XL6_ChannelCaseOwnerV1* item=xl6ChannelReceiveCaseCreateV1(q,ID,ELEMENT,xrtTypeInt64(),sizeof(int64_t));assert(item);
        XL6_ChannelFutureFrameV1* frame=api->Prepare(&item,1);assert(frame);
        xvalue* handle=xl6ChannelFutureBuilderResultV1(frame);assert(handle==api->ResultHandle(frame));
        xvalue* owner=xrtValueObject();assert(owner);
        assert(xrtValueObjectSet(owner,XRT_STR_LITERAL("native"),handle));xrtValueRelease(handle);
        if(mode==2){xl6ChannelFutureBuilderDestroyV1(frame);xrtValueRelease(owner);}
        else {
            xfuture* future=xl6ChannelFutureBuilderStartV1(executor,frame,owner,"native",6,NULL);assert(future);
            if(!mode)xl6ChannelFutureBuilderDestroyV1(frame);
            xrtValueRelease(owner);send(q,91);
            int64_t value=0;
            assert(xl6ChannelSelectionValueV1(selection(outcome(future),true),item,ELEMENT,xrtTypeInt64(),SLOT(value)) && value==91);
            if(mode)xl6ChannelFutureBuilderDestroyV1(frame);
            xrtFutureDestroy(future);
        }
        idle(executor);xl6ChannelCaseDestroyV1(item);xrtValueRelease(q);finish(executor);balanced(&before);
    }
    printf("resident Channel Future: ready/pending/closed/output-parent-pre-cancel, handle/object payloads, real catalog/pins/graph, 50 async rendezvous pairs, 200 concurrent producer/register races, 20 duplicate cases with worker-only success/false-NULL/Range cleanup, 512 waits on one worker, retained builder early/late/unused destruction, full/Close rollback, zero late allocation, full prepare/start/large OOM prefixes %u/%u/%u; balanced\n",createPrefix,startPrefix,largePrefix);
    return 0;
}
