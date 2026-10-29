#define XRUNTIME_MODULE_RUNTIME_TYPE_FUTURE
#include "src/runtime/xl6_channel.h"
#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_ownership_plan.h"
#include "src/api/xl_foreign_error.h"
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define ID UINT64_C(0xBB19)
#define ELEMENT UINT64_C(0xBB20)
#define SLOT(Value) (XL6_ChannelSlotV1){&(Value),sizeof(Value)}
static xvalue* seed(unsigned kind,bool nullable)
{
    if (kind==3) return xl6SequenceReferenceCreateV1(XL6_SEQUENCE_ARRAY_V1,123,ELEMENT,
        XL6_VALUE_ADMIT_ANY,nullable,ELEMENT);
    return xl6SequencePrimitiveCreateV1(XL6_SEQUENCE_ARRAY_V1,123,ELEMENT,kind,
        kind==2 ? sizeof(xstrview) : sizeof(int64_t),kind==2 ? xl6StringTypeV1() : xrtTypeInt64(),
        kind==2 ? XL6_VALUE_ADMIT_STRING : XL6_VALUE_ADMIT_I64,nullable,ELEMENT);
}
static xvalue* create(unsigned kind,bool nullable,int64_t capacity)
{ xvalue* q=xl6ChannelCreateTakeV1(ID,seed(kind,nullable),capacity); assert(q); return q; }
static bool send(const xvalue* q,const xrttype* type,XL6_ChannelSlotV1 slot,XL6_ChannelWaitV1 wait,
    int64_t ms,XL6_ChannelStatusV1* status)
{ return xl6ChannelSendV1(q,ID,ELEMENT,type,slot,wait,ms,status); }
static bool receive(const xvalue* q,const xrttype* type,XL6_ChannelSlotV1 slot,XL6_ChannelWaitV1 wait,
    int64_t ms,XL6_ChannelStatusV1* status)
{ return xl6ChannelReceiveV1(q,ID,ELEMENT,type,slot,wait,ms,status); }
static const xrtownershipadapterv1* resolve(xrtownershipref ref,void* user)
{
    (void)user; const xrtownershipadapterv1* adapter=xl6ChannelOwnershipAdapterV1(ref);
    if (!adapter) adapter=xl6SequenceOwnershipAdapterV1(ref);
    if (!adapter) adapter=xrtValueOwnershipAdapterV1(ref);
    return adapter;
}
static void checkedError(void)
{ assert(xrtGetError()); xrtClearError(); }
static void foreignDrop(void* raw,void* context) { assert(raw==(void*)1 && !context); }
static const xvaluehandleops foreignOps={NULL,foreignDrop,NULL,NULL};
static bool failCopy,failSend,failUnit;
bool xl6ChannelProbeCopy(const xrttype* type,void* target,const void* source)
{ return failCopy ? false : xrtTypeCopyValue(type,target,source); }
xchannelresult xl6ChannelProbeSend(xchannel* channel,void* item)
{ return failSend ? XCHANNEL_ERROR : xrtChannelTrySend(channel,item); }
bool xl6ChannelProbeUnit(xchannel* channel)
{ bool ok=xrtChannelUnit(channel); return failUnit ? false : ok; }
static void logicalFailures(void)
{
    for (unsigned mode=0;mode<3;++mode) {
        xvalue* q=create(2,false,1); xstrview text=XRT_STR_LITERAL("logical\0你");
        XL6_ChannelStatusV1 status=XL6_CHANNEL_FULL_V1;
        xl6_error_scope scope={0}; assert(xl6ErrorScopeBegin(&scope));
        failCopy=mode==0; failSend=mode==1; failUnit=mode==2;
        if (mode<2) {
            assert(!send(q,xl6StringTypeV1(),SLOT(text),XL6_CHANNEL_TRY_V1,0,&status));
            assert(status==XL6_CHANNEL_FULL_V1);
        } else {
            assert(send(q,xl6StringTypeV1(),SLOT(text),XL6_CHANNEL_TRY_V1,0,&status) && status==XL6_CHANNEL_OK_V1);
            xrtValueRelease(q); q=NULL;
        }
        failCopy=failSend=failUnit=false;
        xl6_error_result outcome=xl6ErrorScopeEnd(&scope); assert(!outcome.ok && !outcome.error && !xrtGetError());
        if (q) { assert(xl6ChannelQueryV1(q,ID,0)==0); xrtValueRelease(q); }
    }
}
static void vectors(void)
{
    xvalue* q=create(1,false,1); XL6_ChannelStatusV1 status=XL6_CHANNEL_FULL_V1;
    int64_t input=7,output=99;
    assert(xl6ChannelQueryV1(q,ID,XL6_CHANNEL_QUERY_CAPACITY_V1)==1);
    assert(receive(q,xrtTypeInt64(),SLOT(output),XL6_CHANNEL_TRY_V1,0,&status));
    assert(status==XL6_CHANNEL_EMPTY_V1 && output==99 && !xrtGetError());
    assert(receive(q,xrtTypeInt64(),SLOT(output),XL6_CHANNEL_FOR_V1,0,&status));
    assert(status==XL6_CHANNEL_TIMEOUT_V1 && output==99);
    status=XL6_CHANNEL_FULL_V1;
    assert(!receive(q,xrtTypeInt64(),SLOT(output),XL6_CHANNEL_FOR_V1,INT64_MAX,&status)); checkedError();
    assert(status==XL6_CHANNEL_FULL_V1 && output==99 && xl6ChannelQueryV1(q,ID,0)==0);
    assert(!send(q,xrtTypeInt64(),SLOT(input),XL6_CHANNEL_FOR_V1,INT64_MAX,&status)); checkedError();
    assert(status==XL6_CHANNEL_FULL_V1 && xl6ChannelQueryV1(q,ID,0)==0);
    assert(send(q,xrtTypeInt64(),SLOT(input),XL6_CHANNEL_TRY_V1,0,&status) && status==XL6_CHANNEL_OK_V1);
    assert(send(q,xrtTypeInt64(),SLOT(input),XL6_CHANNEL_TRY_V1,0,&status) && status==XL6_CHANNEL_FULL_V1);
    assert(send(q,xrtTypeInt64(),SLOT(input),XL6_CHANNEL_FOR_V1,0,&status) && status==XL6_CHANNEL_TIMEOUT_V1);
    assert(send(q,xrtTypeInt64(),SLOT(input),XL6_CHANNEL_FOR_V1,1,&status) && status==XL6_CHANNEL_TIMEOUT_V1);
    status=XL6_CHANNEL_EMPTY_V1;
    assert(!receive(q,xrtTypeInt64(),SLOT(output),XL6_CHANNEL_FOR_V1,-1,&status)); checkedError();
    assert(status==XL6_CHANNEL_EMPTY_V1 && output==99 && xl6ChannelQueryV1(q,ID,0)==1);
    assert(!receive(q,xrtTypeInt64(),SLOT(output),XL6_CHANNEL_WAIT_V1,1,&status)); checkedError();
    assert(!receive(q,xrtTypeInt64(),SLOT(output),(XL6_ChannelWaitV1)3,0,&status)); checkedError();
    assert(!xl6ChannelReceiveV1(q,ID,ELEMENT+1,xrtTypeInt64(),SLOT(output),XL6_CHANNEL_TRY_V1,0,&status)); checkedError();
    assert(!xl6ChannelReceiveV1(q,ID+1,ELEMENT,xrtTypeInt64(),SLOT(output),XL6_CHANNEL_TRY_V1,0,&status)); checkedError();
    assert(!receive(q,xrtTypeInt64(),(XL6_ChannelSlotV1){&output,1},XL6_CHANNEL_TRY_V1,0,&status)); checkedError();
    assert(!receive(q,xrtTypeInt64(),(XL6_ChannelSlotV1){(char*)&output+1,sizeof(output)},XL6_CHANNEL_TRY_V1,0,&status)); checkedError();
    assert(!receive(q,xrtTypeInt64(),SLOT(output),XL6_CHANNEL_TRY_V1,0,(XL6_ChannelStatusV1*)&output)); checkedError();
    assert(xl6ChannelCloseV1(q,ID) && xl6ChannelCloseV1(q,ID));
    assert(send(q,xrtTypeInt64(),SLOT(input),XL6_CHANNEL_TRY_V1,0,&status) && status==XL6_CHANNEL_CLOSED_V1);
    assert(receive(q,xrtTypeInt64(),SLOT(output),XL6_CHANNEL_WAIT_V1,0,&status) && status==XL6_CHANNEL_OK_V1 && output==7);
    assert(receive(q,xrtTypeInt64(),SLOT(output),XL6_CHANNEL_WAIT_V1,0,&status) && status==XL6_CHANNEL_CLOSED_V1);
    assert(xl6ChannelQueryV1(q,ID,3)==1); xrtValueRelease(q);

    q=create(2,false,2); xstrview text=XRT_STR_LITERAL("a\0你🙂"),copy={0};
    assert(send(q,xl6StringTypeV1(),SLOT(text),XL6_CHANNEL_TRY_V1,0,&status) && status==XL6_CHANNEL_OK_V1);
    xstrview occupied=XRT_STR_LITERAL("occupied");
    assert(!receive(q,xl6StringTypeV1(),SLOT(occupied),XL6_CHANNEL_TRY_V1,0,&status)); checkedError();
    assert(occupied.Size==8 && xl6ChannelQueryV1(q,ID,0)==1);
    assert(xrtMemDebugFailAfter(0));
    assert(receive(q,xl6StringTypeV1(),SLOT(copy),XL6_CHANNEL_TRY_V1,0,&status) && status==XL6_CHANNEL_OK_V1);
    assert(!xrtMemDebugFailTriggered()); xrtMemDebugFailClear();
    assert(copy.Data!=text.Data && copy.Size==text.Size && !memcmp(copy.Data,text.Data,text.Size));
    xrtTypeDropValue(xl6StringTypeV1(),&copy); xrtValueRelease(q);

    q=create(3,true,2); xvalue* value=NULL,*got=NULL;
    assert(send(q,xrtTypeValue(),SLOT(value),XL6_CHANNEL_TRY_V1,0,&status) && status==XL6_CHANNEL_OK_V1);
    assert(receive(q,xrtTypeValue(),SLOT(got),XL6_CHANNEL_TRY_V1,0,&status) && status==XL6_CHANNEL_OK_V1 && !got);
    assert(receive(q,xrtTypeValue(),SLOT(got),XL6_CHANNEL_TRY_V1,0,&status) && status==XL6_CHANNEL_EMPTY_V1);
    value=xrtValueBytes((xbytesview){(cbytes)text.Data,text.Size}); assert(value);
    assert(send(q,xrtTypeValue(),SLOT(value),XL6_CHANNEL_TRY_V1,0,&status) && status==XL6_CHANNEL_OK_V1);
    assert(receive(q,xrtTypeValue(),SLOT(got),XL6_CHANNEL_TRY_V1,0,&status) && status==XL6_CHANNEL_OK_V1 && got==value);
    xrtValueRelease(value); xrtValueRelease(got); xrtValueRelease(q);
    q=xl6ChannelCreateTakeV1(ID,xl6SequenceReferenceCreateV1(1,123,ELEMENT,XL6_VALUE_ADMIT_BYTES,false,ELEMENT),1);
    assert(q); value=NULL;
    assert(!send(q,xrtTypeValue(),SLOT(value),XL6_CHANNEL_TRY_V1,0,&status)); checkedError(); xrtValueRelease(q);

    xvalue* witness=seed(1,false); XL6_SequenceRoleV1 role; XL6_SequenceLayoutV1 layout={0}; XL6_SequenceAdmissionV3 policy={0};
    assert(xl6SequenceSchemaCopyV1(witness,&role,&layout,&policy)); q=xl6ChannelCreateTakeV1(ID,xrtValueRetain(witness),2); assert(q);
    xerror* diagnostic=xrtErrorCreate(XERR_ARGUMENT,"test",1,"primary"); assert(diagnostic); xrtSetErrorTake(diagnostic);
    assert(xl6ChannelAdmitsSchemaV1(q,ID,&layout,&policy) && xrtGetError()==diagnostic);
    layout.elementIdentity++; assert(!xl6ChannelAdmitsSchemaV1(q,ID,&layout,&policy)); layout.elementIdentity--;
    layout.identity++; assert(!xl6ChannelAdmitsSchemaV1(q,ID,&layout,&policy)); layout.identity--;
    XL6_SequenceAdmissionV3 malformed=policy; malformed.data=NULL;
    assert(!xl6ChannelAdmitsSchemaV1(q,ID,&layout,&malformed));
    void* fake=(void*)1; xvalue* foreign=xrtValueHandleTake(&fake,&foreignOps,NULL);
    assert(foreign && xrtValueTypeIdBind(foreign,ID) && !xl6ChannelAdmitsSchemaV1(foreign,ID,&layout,&policy));
    assert(xrtGetError()==diagnostic); xrtClearError(); xrtValueRelease(foreign);
    xl6SequenceAdmissionDropV3(&policy); xrtValueRelease(witness); xrtValueRelease(q);
    xl6_error_scope failure={0}; assert(xl6ErrorScopeBegin(&failure));
    assert(!xl6ChannelCreateTakeV1(ID,seed(1,false),-1)); xl6_error_result outcome=xl6ErrorScopeEnd(&failure);
    assert(!outcome.ok); xrtErrorFree(outcome.error);
    /* Future references are a supported physical message profile. Keep the
     * strict identity/non-null/empty-destination checks, then prove that the
     * received reference survives both Channel and producer retirement. */
    q=xl6ChannelCreateTakeV1(ID,xl6SequenceFutureCreateV1(1,123,ELEMENT,ELEMENT,false),1);assert(q);
    xfuture *future=NULL,*received=NULL,*absent=NULL;
    xpromise* promise=xrtPromiseCreate(&future,NULL);assert(promise&&future);
    assert(!send(q,xrtTypeFuture(),SLOT(absent),XL6_CHANNEL_TRY_V1,0,&status));checkedError();
    assert(!xl6ChannelSendV1(q,ID,ELEMENT+1,xrtTypeFuture(),SLOT(future),XL6_CHANNEL_TRY_V1,0,&status));checkedError();
    assert(send(q,xrtTypeFuture(),SLOT(future),XL6_CHANNEL_TRY_V1,0,&status)&&status==XL6_CHANNEL_OK_V1);
    assert(!receive(q,xrtTypeFuture(),SLOT(future),XL6_CHANNEL_TRY_V1,0,&status));checkedError();
    assert(xl6ChannelQueryV1(q,ID,0)==1);
    assert(receive(q,xrtTypeFuture(),SLOT(received),XL6_CHANNEL_TRY_V1,0,&status)&&status==XL6_CHANNEL_OK_V1&&received==future);
    assert(receive(q,xrtTypeFuture(),SLOT(absent),XL6_CHANNEL_TRY_V1,0,&status)&&status==XL6_CHANNEL_EMPTY_V1&&!absent);
    assert(xl6ChannelCloseV1(q,ID));xrtValueRelease(q);xrtFutureDestroy(future);xrtPromiseDestroy(promise);
    xfutureresult terminal={0};assert(xrtFutureResult(received,&terminal)&&terminal.State==XFUTURE_CLOSED);
    xrtFutureDestroy(received);
}
static void cycles(void)
{
    xvalue* q=create(3,false,2),*weak=xrtValueWeakRef(q); assert(weak);
    XL6_ChannelStatusV1 status;
    assert(send(q,xrtTypeValue(),SLOT(q),XL6_CHANNEL_TRY_V1,0,&status) && status==XL6_CHANNEL_OK_V1);
    XL6_OwnershipPlan* plan=NULL;
    assert(xl6OwnershipPlanTryCreate(xrtValueOwnership(q),resolve,NULL,&plan)==XL6_OWNERSHIP_PLAN_ROOTED);
    assert(xl6OwnershipPlanFinalize(plan));
    assert(xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_ROOTED);
    assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY); plan=NULL;
    assert(xl6OwnershipPlanTryCreate(xrtValueOwnership(q),resolve,NULL,&plan)==XL6_OWNERSHIP_PLAN_ROOTED);
    xrtValueRelease(q);
    assert(xl6OwnershipPlanFinalize(plan) && xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_READY);
    assert(xl6OwnershipPlanFinish(plan) && xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY);
    assert(xrtValueWeakRefExpired(weak)); xrtValueRelease(weak);
}
static unsigned dropped;
static void failedDrop(void* payload,void* context)
{ assert(payload==(void*)1 && !context); ++dropped; xl6ForeignCallbackFail(); }
static const xvaluehandleops failedOps={NULL,failedDrop,NULL,NULL};
static void cleanupFailures(void)
{
    for (unsigned fault=0;fault<2;++fault) {
        xvalue* q=create(3,false,3); XL6_ChannelStatusV1 status; unsigned before=dropped;
        for (unsigned i=0;i<3;++i) {
            void* raw=(void*)1; xvalue* item=xrtValueHandleTake(&raw,&failedOps,NULL); assert(item);
            assert(send(q,xrtTypeValue(),SLOT(item),XL6_CHANNEL_TRY_V1,0,&status) && status==XL6_CHANNEL_OK_V1); xrtValueRelease(item);
        }
        xl6_error_scope scope={0}; assert(xl6ErrorScopeBegin(&scope));
        if (fault) assert(xrtMemDebugFailAfter(0));
        xrtValueRelease(q); xrtMemDebugFailClear();
        xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
        assert(!outcome.ok && dropped==before+3); xrtErrorFree(outcome.error); assert(!xrtGetError());
    }
}
typedef struct Transport {
    xvalue* q; bool text;
    xatomic32 seen[2048],received;
} Transport;
typedef struct Lane { Transport* all; unsigned id; bool producer; } Lane;
static int32 worker(void* raw)
{
    Lane* lane=raw; Transport* all=lane->all; XL6_ChannelStatusV1 status;
    if (lane->producer) for (unsigned i=0;i<512;++i) {
        int64_t value=(int64_t)(lane->id*512+i); char bytes[32]; int length=snprintf(bytes,sizeof(bytes),"%lld",(long long)value);
        assert(length>0); memcpy(bytes+length,"\0你",4); xstrview text={bytes,(size_t)length+4};
        assert(all->text ? send(all->q,xl6StringTypeV1(),SLOT(text),XL6_CHANNEL_WAIT_V1,0,&status) :
            send(all->q,xrtTypeInt64(),SLOT(value),XL6_CHANNEL_WAIT_V1,0,&status)); assert(status==XL6_CHANNEL_OK_V1);
    } else for (;;) {
        int64_t value=0; xstrview text={0};
        assert(all->text ? receive(all->q,xl6StringTypeV1(),SLOT(text),XL6_CHANNEL_WAIT_V1,0,&status) :
            receive(all->q,xrtTypeInt64(),SLOT(value),XL6_CHANNEL_WAIT_V1,0,&status));
        if (status==XL6_CHANNEL_CLOSED_V1) break;
        assert(status==XL6_CHANNEL_OK_V1);
        if (all->text) {
            value=strtoll(text.Data,NULL,10); size_t first=strlen(text.Data);
            assert(text.Size==first+4 && !memcmp(text.Data+first,"\0你",4)); xrtTypeDropValue(xl6StringTypeV1(),&text);
        }
        assert(value>=0 && value<2048 && xrtAtomic32FetchAdd(&all->seen[value],1,XMEMORY_ACQ_REL)==0);
        (void)xrtAtomic32FetchAdd(&all->received,1,XMEMORY_ACQ_REL);
    }
    assert(!xrtGetError()); return 0;
}
static void threads(void)
{
    for (unsigned text=0;text<2;++text) for (unsigned rendezvous=0;rendezvous<2;++rendezvous) {
        Transport all={.q=create(text ? 2 : 1,false,rendezvous ? 0 : 5),.text=text!=0};
        xrtAtomic32Init(&all.received,0); for (size_t i=0;i<2048;++i) xrtAtomic32Init(&all.seen[i],0);
        unsigned count=rendezvous ? 1 : 4; Lane lanes[8]; xthread* actors[8];
        for (unsigned i=0;i<count;++i) { lanes[i]=(Lane){&all,i,false}; actors[i]=xrtThreadCreate(worker,&lanes[i],0); assert(actors[i]); }
        for (unsigned i=0;i<count;++i) { lanes[i+count]=(Lane){&all,i,true}; actors[i+count]=xrtThreadCreate(worker,&lanes[i+count],0); assert(actors[i+count]); }
        for (unsigned i=count;i<count*2;++i) { assert(xrtThreadWaitFor(actors[i],UINT64_C(30000000))==XWAIT_OK); xrtThreadDestroy(actors[i]); }
        assert(xl6ChannelCloseV1(all.q,ID));
        for (unsigned i=0;i<count;++i) { assert(xrtThreadWaitFor(actors[i],UINT64_C(30000000))==XWAIT_OK); xrtThreadDestroy(actors[i]); }
        assert(xrtAtomic32Load(&all.received,XMEMORY_ACQUIRE)==count*512 && xl6ChannelQueryV1(all.q,ID,0)==0);
        xrtValueRelease(all.q);
    }
}
/* Resident test policy pauses after operation-pin acquisition, without a
 * world mutation lock. It proves both BUSY graph refusal and last-wrapper
 * release during an active native operation, not just timed sleep guesses. */
typedef struct Gate { volatile int32_t refs; xatomic32 entered,leave; } Gate;
static bool gateRetain(const void* raw) { return xrtRefRetain(&((Gate*)raw)->refs)>0; }
static void gateDrop(const void* raw) { if (!xrtRefRelease(&((Gate*)raw)->refs)) xrtFree((void*)raw); }
static bool gateSlot(const void* raw,const void* slot)
{
    Gate* gate=(Gate*)raw; xrtAtomic32Store(&gate->entered,1,XMEMORY_RELEASE);
    while (!xrtAtomic32Load(&gate->leave,XMEMORY_ACQUIRE)) xrtThreadYield();
    return slot!=NULL;
}
static bool gateValue(const void* raw,const xvalue* value) { (void)raw; return xrtValueType(value)==XVALUE_INT; }
static bool gateEquivalent(const void* a,const void* b) { return a==b; }
static bool gateDefault(const void* raw) { (void)raw; return true; }
static const XL6_SequenceAdmissionOpsV2 gateOps={gateRetain,gateDrop,gateSlot,gateValue,gateEquivalent,gateDefault,NULL,NULL};
typedef struct PinLane { xvalue* q; XL6_ChannelStatusV1 status; bool ok; } PinLane;
static int32 pinWorker(void* raw)
{ PinLane* lane=raw; int64_t item=19; lane->ok=send(lane->q,xrtTypeInt64(),SLOT(item),XL6_CHANNEL_TRY_V1,0,&lane->status); return 0; }
static void pins(void)
{
    for (unsigned release=0;release<2;++release) {
        Gate* gate=xrtCalloc(1,sizeof(*gate)); assert(gate); gate->refs=1;
        xrtAtomic32Init(&gate->entered,0); xrtAtomic32Init(&gate->leave,0);
        XL6_SequenceAdmissionV3 policy={&gateOps,gate,ELEMENT,1,0,sizeof(int64_t),xrtTypeInt64(),ELEMENT};
        xvalue* witness=xl6SequenceCreateAdmittedV1(1,123,&policy); assert(witness);
        xvalue* q=xl6ChannelCreateTakeV1(ID,witness,1),*weak=xrtValueWeakRef(q); assert(q && weak);
        PinLane lane={.q=q}; xthread* actor=xrtThreadCreate(pinWorker,&lane,0); assert(actor);
        while (!xrtAtomic32Load(&gate->entered,XMEMORY_ACQUIRE)) xrtThreadYield();
        XL6_OwnershipPlan* plan=NULL;
        assert(xl6OwnershipPlanTryCreate(xrtValueOwnership(q),resolve,NULL,&plan)==XL6_OWNERSHIP_PLAN_ERROR && !plan); xrtClearError();
        assert(xl6ChannelCloseV1(q,ID));
        if (release) { xrtValueRelease(q); assert(xrtValueWeakRefExpired(weak)); }
        xrtAtomic32Store(&gate->leave,1,XMEMORY_RELEASE);
        assert(xrtThreadWaitFor(actor,UINT64_C(30000000))==XWAIT_OK && lane.ok && lane.status==XL6_CHANNEL_CLOSED_V1);
        xrtThreadDestroy(actor); if (!release) xrtValueRelease(q); xrtValueRelease(weak);
    }
}
typedef struct WaitLane { xvalue* q; bool send,ok; XL6_ChannelStatusV1 status; int64_t value; } WaitLane;
static int32 waitWorker(void* raw)
{
    WaitLane* lane=raw;
    lane->ok=lane->send ? send(lane->q,xrtTypeInt64(),SLOT(lane->value),XL6_CHANNEL_WAIT_V1,0,&lane->status) :
        receive(lane->q,xrtTypeInt64(),SLOT(lane->value),XL6_CHANNEL_WAIT_V1,0,&lane->status);
    assert(!xrtGetError()); return 0;
}
static void closeWaiters(void)
{
    for (unsigned sending=0;sending<2;++sending) {
        xvalue* q=create(1,false,1); int64_t value=7; XL6_ChannelStatusV1 status;
        if (sending) assert(send(q,xrtTypeInt64(),SLOT(value),XL6_CHANNEL_TRY_V1,0,&status) && status==XL6_CHANNEL_OK_V1);
        WaitLane lane={.q=q,.send=sending!=0,.value=23};
        xthread* actor=xrtThreadCreate(waitWorker,&lane,0); assert(actor);
        /* A real waiting operation holds activity, but not a world mutation.
         * A completed freeze therefore refuses its owner rather than blocking. */
        for (unsigned attempt=0;;++attempt) {
            assert(attempt<1000000); XL6_OwnershipPlan* plan=NULL;
            XL6_OwnershipPlanResult result=xl6OwnershipPlanTryCreate(xrtValueOwnership(q),resolve,NULL,&plan);
            if (result==XL6_OWNERSHIP_PLAN_ERROR) { assert(!plan); xrtClearError(); break; }
            if (result==XL6_OWNERSHIP_PLAN_BUSY) { assert(!plan); xrtThreadYield(); continue; }
            assert(result==XL6_OWNERSHIP_PLAN_ROOTED && plan);
            do { result=xl6OwnershipPlanTryDestroy(plan); if (result==XL6_OWNERSHIP_PLAN_BUSY) xrtThreadYield(); }
            while (result==XL6_OWNERSHIP_PLAN_BUSY);
            assert(result==XL6_OWNERSHIP_PLAN_READY); xrtThreadYield();
        }
        assert(xl6ChannelCloseV1(q,ID));
        assert(xrtThreadWaitFor(actor,UINT64_C(30000000))==XWAIT_OK && lane.ok && lane.status==XL6_CHANNEL_CLOSED_V1);
        xrtThreadDestroy(actor);
        if (sending) {
            assert(receive(q,xrtTypeInt64(),SLOT(value),XL6_CHANNEL_TRY_V1,0,&status));
            assert(status==XL6_CHANNEL_OK_V1 && value==7);
        }
        xrtValueRelease(q);
    }
}
static void fault(const char* operation,size_t point)
{
    xvalue* q=NULL; bool ok; XL6_ChannelStatusV1 status=XL6_CHANNEL_FULL_V1;
    xstrview text=XRT_STR_LITERAL("fault\0你🙂"),copy={0};
    if (strcmp(operation,"create")) q=create(2,false,1);
    assert(xrtMemDebugFailAfter(point)); xl6_error_scope scope={0}; assert(xl6ErrorScopeBegin(&scope));
    if (!strcmp(operation,"create")) { q=xl6ChannelCreateTakeV1(ID,seed(2,false),1); ok=q!=NULL; }
    else ok=send(q,xl6StringTypeV1(),SLOT(text),!strcmp(operation,"send-timeout") ?
        XL6_CHANNEL_FOR_V1 : XL6_CHANNEL_TRY_V1,0,&status);
    bool hit=xl6ProductMemoryEnd(); xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    assert(ok==outcome.ok); xrtErrorFree(outcome.error); assert(!xrtGetError());
    if (strcmp(operation,"create")) {
        assert(xl6ChannelQueryV1(q,ID,0)==(ok ? 1 : 0));
        assert(receive(q,xl6StringTypeV1(),SLOT(copy),XL6_CHANNEL_TRY_V1,0,&status));
        assert(status==(ok ? XL6_CHANNEL_OK_V1 : XL6_CHANNEL_EMPTY_V1));
        xrtTypeDropValue(xl6StringTypeV1(),&copy);
        assert(send(q,xl6StringTypeV1(),SLOT(text),XL6_CHANNEL_TRY_V1,0,&status) && status==XL6_CHANNEL_OK_V1);
        xrtValueRelease(q);
    } else xrtValueRelease(q);
    xl6ProductMemoryVerify();
    printf("hit=%u complete-prefix balanced\n",hit ? 1u : 0u);
}
int main(int argc,char** argv)
{
    xl6ProductMemoryBegin(SIZE_MAX);
    if (argc==3) { fault(argv[1],(size_t)strtoull(argv[2],NULL,10)); return 0; }
    assert(argc==1 || argc==2);
    if (argc==2 && !strcmp(argv[1],"logical")) logicalFailures();
    else if (argc==2 && !strcmp(argv[1],"repeat")) for (unsigned i=0;i<1000;++i) { vectors(); cycles(); }
    else { vectors(); cycles(); cleanupFailures(); threads(); pins(); closeWaiters(); }
    assert(!xrtGetError()); assert(!xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
    puts("channel owned foundation: exact statuses, nullable presence, schema, cycles, pins and concurrent transport balanced"); return 0;
}
