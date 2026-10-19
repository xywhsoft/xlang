/* Source-only deterministic notification barrier. --wrap affects the actual
 * Core object's native result read, not a replacement provider or language
 * callback. Ordinary product tests separately validate the public surface. */
#include "src/runtime/xl6_channel_direct_deadline.h"
#include "src/runtime/xl6_channel_select_deadline.h"
#include "src/runtime/xl6_channel_future_core.h"
#include "src/runtime/xl6_channel_internal.h"
#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_ownership_domains.h"
#include <assert.h>
#include <stdio.h>
#include <stdatomic.h>
#define ID UINT64_C(0xCD7101)
#define ELEMENT UINT64_C(0xCD7102)
#ifndef XL6_TEST_NOTIFY_BARRIER
#define XL6_TEST_NOTIFY_BARRIER 1
#endif
#ifndef XL6_TEST_PUBLISHED_BUILDER
#define XL6_TEST_PUBLISHED_BUILDER 1
#endif
#if XL6_TEST_NOTIFY_BARRIER
static atomic_bool armed,entered,released;
/* Derive declarations from the pinned native ABI so this barrier cannot
 * silently omit an output parameter when that ABI changes. */
extern __typeof__(xrtChannelSubscribeResult) __real_xrtChannelSubscribeResult;
extern __typeof__(xrtChannelSubscribeResult) __wrap_xrtChannelSubscribeResult;
bool __wrap_xrtChannelSubscribeResult(const xchannelsubscription* sub,xchannelselectresult* result,ptr* value)
{
    if(atomic_exchange_explicit(&armed,false,memory_order_acq_rel)){
        atomic_store_explicit(&entered,true,memory_order_release);
        while(!atomic_load_explicit(&released,memory_order_acquire))xrtThreadYield();
    }
    return __real_xrtChannelSubscribeResult(sub,result,value);
}
#endif
static int32_t peer(void* data)
{
    int64_t value=42;XL6_ChannelStatusV1 status;
    assert(xl6ChannelSendV1(data,ID,ELEMENT,xrtTypeInt64(),(XL6_ChannelSlotV1){&value,sizeof(value)},
        XL6_CHANNEL_WAIT_V1,0,&status)&&status==XL6_CHANNEL_OK_V1);return 0;
}
static void probe(XL6_OwnershipCatalog* catalog,xrtownershipref ref)
{
    xrtownershipscope freeze={0};xdeadline until=xrtDeadlineAfter(10000000);
    while(!xrtOwnershipFreezeTryBegin(&freeze)){assert(!xrtDeadlineExpired(until));xrtThreadYield();}
    size_t count=0;assert(!ref.Ops->Count(ref.Data,&count));
    const xrtownershipadapterv1* adapter=(const void*)1;
    const xrtownershippreparationv1* preparation=(const void*)1;
    const XL6_OwnershipFinishV1* finish=(const void*)1;
    assert(xl6OwnershipCatalogProbeV1(ref,catalog,&adapter,&preparation,&finish)==XL6_OWNERSHIP_ADMISSION_BUSY);
    assert(!adapter&&!preparation&&!finish&&xrtOwnershipScopeEnd(&freeze));
}
int main(void)
{
    assert(xrtMemDebugEnable(true));
    for(unsigned selecting=0;selecting<2;++selecting){
        xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);
        const XL6_OwnershipDomainV1* domains[]={selecting?xl6OwnershipDomainChannelFutureV1Get():
            xl6OwnershipDomainChannelDirectFutureV1Get()};
        XL6_OwnershipCatalog* catalog=xl6OwnershipCatalogCreateV1(domains,1);assert(catalog);
        XL6_ContinuationExecutor* executor=xl6ContinuationExecutorCreateFixedV1(4,1);assert(executor);
        XL6_DeadlineServiceV1* clock=xl6DeadlineServiceCreateV1(4);assert(clock);
        xvalue* schema=xl6SequencePrimitiveCreateV1(1,123,ELEMENT,1,sizeof(int64_t),xrtTypeInt64(),XL6_VALUE_ADMIT_I64,false,ELEMENT);
        assert(schema);xvalue* channel=xl6ChannelCreateTakeV1(ID,schema,1);assert(channel);
        XL6_SequenceLayoutV1 layout={ID,ELEMENT,1,sizeof(int64_t),xrtTypeInt64()};
        XL6_ChannelCaseOwnerV1* selected=selecting?xl6ChannelReceiveCaseCreateV1(channel,ID,ELEMENT,xrtTypeInt64(),sizeof(int64_t)):NULL;
        XL6_ChannelFutureFrameV1* frame=selecting?xl6ChannelSelectDeadlineRuntimeV1Get()->PrepareFor(&selected,1,30000):
            xl6ChannelDirectDeadlineRuntimeV1Get()->PrepareReceiveFor(layout,channel,30000);assert(frame);
        xrtownershipref ref=xl6ChannelFutureCoreOwnershipV1(frame);
        probe(catalog,ref); /* Precommit refusal is independently BUSY. */
        #if XL6_TEST_PUBLISHED_BUILDER
        assert(xl6ChannelFutureCorePublishBuilderV1(frame));
        #endif
        /* Stable builder publication admits observation, not raw construction.
         * Private rollback is a separate preparation outside Freeze. */
        xrtownershipscope stable={0};xdeadline inspectUntil=xrtDeadlineAfter(10000000);
        while(!xrtOwnershipFreezeTryBegin(&stable)){assert(!xrtDeadlineExpired(inspectUntil));xrtThreadYield();}
        #if XL6_TEST_PUBLISHED_BUILDER
        size_t stableCount=0;assert(ref.Ops->Count(ref.Data,&stableCount)&&stableCount>0);
        #endif
        assert(xrtOwnershipScopeEnd(&stable));
        assert(xl6ChannelFutureCoreRetainV1(frame)); /* Our actual inspection owner. */
        const xvalue* result=xl6ChannelFutureCoreResultHandleV1(frame);
        xfuture* future=selecting?xl6ChannelSelectDeadlineRuntimeV1Get()->Start(executor,clock,frame,result,NULL,0,NULL):
            xl6ChannelDirectDeadlineRuntimeV1Get()->StartReceive(executor,clock,frame,result,NULL,0,NULL);assert(future);
        /* Accepted work cannot remain claimed while its worker still needs
         * to reactivate a parked Channel. A ready graph is not logical idle. */
        while(!xrtOwnershipFreezeTryBegin(&stable)){assert(!xrtDeadlineExpired(inspectUntil));xrtThreadYield();}
        const xrtownershipadapterv1* adapter=NULL;const xrtownershippreparationv1* preparation=NULL;
        const XL6_OwnershipFinishV1* finish=NULL;static const unsigned token=1;
        assert(xl6OwnershipCatalogProbeV1(ref,catalog,&adapter,&preparation,&finish)==XL6_OWNERSHIP_ADMISSION_ADMITTED);
        assert(adapter&&preparation&&!adapter->Claim(ref.Data,&token));
        assert(xrtOwnershipScopeEnd(&stable));
        xl6ChannelCaseDestroyV1(selected);
#if XL6_TEST_NOTIFY_BARRIER
        atomic_store(&entered,false);atomic_store(&released,false);atomic_store(&armed,true);
#endif
        xthread* sender=xrtThreadCreate(peer,channel,0);assert(sender);
#if XL6_TEST_NOTIFY_BARRIER
        xdeadline until=xrtDeadlineAfter(10000000);
        while(!atomic_load_explicit(&entered,memory_order_acquire)){assert(!xrtDeadlineExpired(until));xrtThreadYield();}
        probe(catalog,ref); /* Committed frame, inside Notify after active++. */
        atomic_store_explicit(&released,true,memory_order_release);
#endif
        assert(xrtThreadWait(sender)==XWAIT_OK);xrtThreadDestroy(sender);
        assert(xrtFutureWaitFor(future,10000000)==XWAIT_OK);
        xfutureresult outcome={0};assert(xrtFutureResult(future,&outcome));
        if(outcome.State!=XFUTURE_RESOLVED)fprintf(stderr,"select=%u state=%d error=%s\n",selecting,
            (int)outcome.State,outcome.Error?xrtErrorMessage(outcome.Error):"false/null");
        assert(outcome.State==XFUTURE_RESOLVED);
        xrtFutureDestroy(future);xl6ChannelFutureCoreDataDropV1(frame);
        xl6DeadlineServiceCloseV1(clock);xl6ContinuationExecutorClose(executor);
        assert(xl6ContinuationExecutorDestroyCompleted(executor)&&xl6DeadlineServiceDestroyCompletedV1(clock));
        xrtValueRelease(channel);xl6OwnershipCatalogDestroyV1(catalog);xrtClearError();xrtMemDebugSnapshot(&after);
        assert(before.LiveCount==after.LiveCount&&before.LiveBytes==after.LiveBytes&&
            after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount&&
            before.InvalidFreeCount==after.InvalidFreeCount&&before.DoubleFreeCount==after.DoubleFreeCount);
    }
    puts(XL6_TEST_NOTIFY_BARRIER?"direct/select precommit and active notification: exact BUSY, null outputs, balanced":
        "direct/select ordinary product precommit: exact BUSY, null outputs, balanced");return 0;
}
