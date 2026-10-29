/* Ordinary in-memory ABI/lifetime test. No provider replacement, external
 * network, fabricated Value shell or synthetic ownership count is used. */
#define XRUNTIME_MODULE_RUNTIME_TYPE_FUTURE
#include "src/runtime/xl6_channel_direct_future.h"
#include "src/runtime/xl6_channel_internal.h"
#include "src/runtime/xl6_channel_case.h"
#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_wait_queue.h"
#include "src/runtime/xl6_slot_map.h"
#include "src/runtime/xl6_slot_map_key_capture.h"
#include "src/runtime/xl6_nullable_slot_map_key_capture.h"
#include "src/runtime/xl6_nullable_slot_map_key_collection.h"
#include "src/runtime/xl6_native_handle.h"
#include "src/api/xl_foreign_error.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define OWNER UINT64_C(0xC04201)
#define FUTURE_ID UINT64_C(0xC04202)
#define OPTIONAL_FUTURE_ID UINT64_C(0xC04203)
typedef union Slot {
    xfuture* future;
    XL6_SlotMapKeyV1 key;
    XL6_NullableSlotMapKeyV1 optional;
} Slot;
typedef struct Message {
    XL6_SequenceLayoutV1 layout;
    XL6_FutureValueProfileV1 profile;
    Slot input;
    unsigned kind;
} Message;
static const XL6_ChannelDirectFutureRuntimeV1* api;
static bool refuseCopy(const void* input,void* output)
{(void)input;(void)output;return false;} /* Valid checked failure, no diagnostic. */
static bool isFuture(unsigned kind){return kind==0 || kind==3;}
static Message message(unsigned kind)
{
    Message m;memset(&m,0,sizeof(m));m.kind=kind;
    if(isFuture(kind)){
        m.layout=(XL6_SequenceLayoutV1){OWNER,kind==3?OPTIONAL_FUTURE_ID:FUTURE_ID,
            XL6_SEQUENCE_TRANSFER_FUTURE_REF_V1,sizeof(xfuture*),xrtTypeFuture()};
        m.profile=(XL6_FutureValueProfileV1){sizeof(m.profile),XL6_FUTURE_VALUE_FUTURE,
            m.layout.size,m.layout.elementIdentity,NULL};
        if(kind==0){
            xpromise* promise=xrtPromiseCreate(&m.input.future,NULL);assert(promise&&m.input.future);
            static int value=31;assert(xrtPromiseResolve(promise,&value));xrtPromiseDestroy(promise);
        }
    }else{
        bool optional=kind!=1;
        const XL6_NativeCaptureProfileV1* profile=optional?xl6NullableSlotMapKeyCaptureProfileV1Get():xl6SlotMapKeyCaptureProfileV1Get();
        m.layout=(XL6_SequenceLayoutV1){OWNER,profile->identity,XL6_SEQUENCE_TRANSFER_NATIVE_VALUE_V1,
            profile->Type()->Size,profile->Type()};
        m.profile=(XL6_FutureValueProfileV1){sizeof(m.profile),XL6_FUTURE_VALUE_NATIVE,m.layout.size,
            m.layout.elementIdentity,profile};
        if(kind!=4){
            XL6_SlotMapLayoutV1 layout={UINT64_C(0xC04204),
                {UINT64_C(0xC04205),UINT64_C(0xC04206),1,sizeof(int64_t),xrtTypeInt64()}};
            xvalue* schema=xl6SequencePrimitiveCreateV1(1,layout.value.identity,layout.value.elementIdentity,
                1,sizeof(int64_t),xrtTypeInt64(),XL6_VALUE_ADMIT_I64,false,layout.value.elementIdentity);
            assert(schema);xvalue* map=xl6SlotMapCreateTakeV1(&layout,schema);assert(map);
            int64_t item=77;XL6_SlotMapKeyV1 key={0};assert(xl6SlotMapInsertV1(map,&layout,&item,&key));
            if(optional)m.input.optional=(XL6_NullableSlotMapKeyV1){true,key};else m.input.key=key;
            xrtValueRelease(map); /* The genuine key does not retain its map. */
        }
    }
    return m;
}
static xvalue* channel(const Message* m)
{
    xvalue* schema=isFuture(m->kind)?xl6SequenceFutureCreateV1(1,123,m->layout.elementIdentity,FUTURE_ID,m->kind==3):
        m->kind==1?xl6SequenceSlotMapKeyCreateV1(1,123):xl6SequenceNullableSlotMapKeyCreateV1(1,123);
    assert(schema);xvalue* value=xl6ChannelCreateTakeV1(OWNER,schema,1);assert(value);return value;
}
static void send(const Message* m,const xvalue* q)
{
    XL6_ChannelStatusV1 status=XL6_CHANNEL_EMPTY_V1;
    assert(xl6ChannelSendV1(q,OWNER,m->layout.elementIdentity,m->layout.type,
        (XL6_ChannelSlotV1){(void*)&m->input,m->layout.size},XL6_CHANNEL_TRY_V1,0,&status));
    assert(status==XL6_CHANNEL_OK_V1);
}
static XL6_ChannelReceiveResultV1* receive(XL6_ContinuationExecutor* executor,XL6_ChannelFutureFrameV1* frame,xfuture** future)
{
    *future=api->StartReceive(executor,frame,api->ResultHandle(frame),NULL,0,NULL);assert(*future);
    assert(xrtFutureWaitFor(*future,10000000)==XWAIT_OK);xfutureresult outcome={0};assert(xrtFutureResult(*future,&outcome));
    assert(outcome.State==XFUTURE_RESOLVED&&outcome.Value);
    xvalue* handle=*(xvalue**)outcome.Value;void* result=NULL;void* user=NULL;const xvaluehandleops* ops=NULL;
    assert(xrtValueGetHandle(handle,&result,&ops,&user)&&result&&!user&&ops==xl6ChannelReceiveResultHandlePolicyV1Get()->Ops);
    return result;
}
static void idle(XL6_ContinuationExecutor* executor)
{
    xdeadline due=xrtDeadlineAfter(10000000);
    while(!xl6ContinuationExecutorIsIdle(executor)){assert(!xrtDeadlineExpired(due));xrtThreadYield();}
}
static void finish(XL6_ContinuationExecutor* executor)
{idle(executor);xl6ContinuationExecutorClose(executor);assert(xl6ContinuationExecutorDestroy(executor));}
static void balanced(const xmemdebugsnapshot* before)
{
    assert(!xrtGetError());xmemdebugsnapshot after={0};xrtMemDebugSnapshot(&after);
    assert(before->LiveCount==after.LiveCount&&before->LiveBytes==after.LiveBytes);
    assert(after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount);
    assert(before->InvalidFreeCount==after.InvalidFreeCount&&before->DoubleFreeCount==after.DoubleFreeCount&&
        before->UseAfterFreeCount==after.UseAfterFreeCount);
}
typedef struct Edge {xrtownershipref expected;size_t matches;} Edge;
static bool edge(xrtownershipref ref,void* data)
{Edge* e=data;if(ref.Data==e->expected.Data&&ref.Ops==e->expected.Ops)++e->matches;return true;}
static bool channelEdge(xrtownershipref ref,void* data)
{return !xl6ChannelOwnershipAdapterV1(ref) || ref.Ops->Trace(ref.Data,edge,data);}
static bool resultEdge(xrtownershipref ref,void* data)
{return ref.Ops && ref.Ops->Trace(ref.Data,edge,data);}
static void freeze(xrtownershipscope* scope)
{xdeadline due=xrtDeadlineAfter(10000000);while(!xrtOwnershipFreezeTryBegin(scope)){assert(!xrtDeadlineExpired(due));xrtThreadYield();}}
static void futureEdge(const xvalue* q,xfuture* input)
{
    if(!input)return;
    xrtownershipscope scope={0};freeze(&scope);
    xrtownershipref ref=xrtValueOwnership(q);Edge e={xrtFutureOwnership(input),0};
    assert(ref.Ops&&ref.Ops->Trace(ref.Data,channelEdge,&e)&&e.matches==1);
    assert(xrtOwnershipScopeEnd(&scope));
}
static void equal(const Message* m,const Slot* copy)
{
    if(isFuture(m->kind))assert(copy->future==m->input.future);
    else if(m->kind==1)assert(xl6SlotMapKeyEqualV1(copy->key,m->input.key));
    else assert(copy->optional.hasValue==m->input.optional.hasValue&&
        xl6SlotMapKeyEqualV1(copy->optional.value,m->input.optional.value));
}
int main(void)
{
    assert(xrtMemDebugEnable(true));api=xl6ChannelDirectFutureRuntimeV1Get();assert(api);
    for(unsigned kind=0;kind<5;++kind){
        xmemdebugsnapshot before={0};xrtMemDebugSnapshot(&before);Message m=message(kind);
        XL6_ContinuationExecutor* executor=xl6ContinuationExecutorCreateFixedV1(1,1);assert(executor);
        xvalue* q=channel(&m);send(&m,q);if(isFuture(kind))futureEdge(q,m.input.future);
        XL6_SequenceLayoutV1 wrong=m.layout;wrong.transfer=3;assert(!api->PrepareReceive(wrong,q));xrtClearError();
        XL6_ChannelFutureFrameV1* frame=api->PrepareReceive(m.layout,q);assert(frame);
        xfuture* future=NULL;XL6_ChannelReceiveResultV1* result=receive(executor,frame,&future);
        xrtValueRelease(q); /* Result owns its slot policy, independently of Channel. */
        if(kind==0){
            idle(executor);xfutureresult outcome={0};assert(xrtFutureResult(future,&outcome));
            xrtownershipscope scope={0};freeze(&scope);Edge e={xrtFutureOwnership(m.input.future),0};
            assert(xl6ChannelReceiveResultHandlePolicyV1Get()->Trace(*(xvalue**)outcome.Value,resultEdge,&e)&&e.matches==1);
            assert(xrtOwnershipScopeEnd(&scope));
        }
        int64_t status=-1;assert(xl6ChannelReceiveResultInfoV1(result,&status)&&status==XL6_CHANNEL_OK_V1);
        Slot first,second,empty;memset(&first,0,sizeof(first));memset(&second,0,sizeof(second));memset(&empty,0,sizeof(empty));
        XL6_FutureValueProfileV1 bad=m.profile;bad.type^=UINT64_C(1);
        assert(!xl6ChannelReceiveResultReadV1(bad,result,&first)&&!memcmp(&first,&empty,m.layout.size));xrtClearError();
        assert(xl6ChannelReceiveResultReadV1(m.profile,result,&first));equal(&m,&first);
        assert(xl6ChannelReceiveResultReadV1(m.profile,result,&second));equal(&m,&second);
        if(kind<3){Slot saved;memcpy(&saved,&first,sizeof(saved));assert(!xl6ChannelReceiveResultReadV1(m.profile,result,&first));
            assert(!memcmp(&first,&saved,m.layout.size));xrtClearError();}
        xrtTypeDropValue(m.layout.type,&first);xrtTypeDropValue(m.layout.type,&second);
        xrtTypeDropValue(m.layout.type,&m.input);xrtFutureDestroy(future);finish(executor);balanced(&before);
    }
    unsigned prefixes[3][3]={{0}};
    for(unsigned kind=0;kind<3;++kind)for(unsigned phase=0;phase<3;++phase)for(size_t point=0;point<128;++point){
        xmemdebugsnapshot before={0};xrtMemDebugSnapshot(&before);Message m=message(kind);
        XL6_ContinuationExecutor* executor=xl6ContinuationExecutorCreateFixedV1(1,1);assert(executor);
        xvalue* q=channel(&m);if(phase)send(&m,q);
        XL6_ChannelFutureFrameV1* frame=NULL;xfuture* future=NULL;
        if(phase==2){frame=api->PrepareReceive(m.layout,q);assert(frame);}
        assert(xrtMemDebugFailAfter(point));
        if(phase<2)frame=phase?api->PrepareReceive(m.layout,q):api->PrepareSend(m.layout,q,&m.input);
        else future=api->StartReceive(executor,frame,api->ResultHandle(frame),NULL,0,NULL);
        bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();xrtClearError();
        if(future){assert(xrtFutureWaitFor(future,10000000)==XWAIT_OK);xrtFutureDestroy(future);}
        else if(frame)api->Discard(frame);
        xrtValueRelease(q);xrtTypeDropValue(m.layout.type,&m.input);finish(executor);balanced(&before);
        if(!hit){prefixes[kind][phase]=(unsigned)point;break;}assert(point<127);
    }
    {
        xmemdebugsnapshot before={0};xrtMemDebugSnapshot(&before);Message m=message(1);
        xvalue* schema=xl6SequenceSlotMapKeyCreateV1(1,123);assert(schema);
        xvalue* q=xl6WaitQueueCreateTakeV1(OWNER,schema,2);assert(q);
        assert(xl6WaitQueuePushV1(q,OWNER,m.layout.elementIdentity,m.layout.size,m.layout.type,&m.input)==XQUEUE_OK);
        XL6_SlotMapKeyV1 occupied={0};assert(xl6SlotMapKeyCopyValueV1(m.input.key,&occupied));
        XL6_ChannelStatusV1 status=XL6_CHANNEL_TIMEOUT_V1;
        assert(!xl6WaitQueueReceiveV1(q,OWNER,m.layout.elementIdentity,m.layout.type,
            (XL6_ChannelSlotV1){&occupied,sizeof(occupied)},XL6_CHANNEL_TRY_V1,0,&status));
        assert(status==XL6_CHANNEL_TIMEOUT_V1&&xl6SlotMapKeyEqualV1(occupied,m.input.key));xrtClearError();
        assert(xl6WaitQueueQueryV1(q,OWNER,XL6_QUEUE_QUERY_COUNT_V1)==1);xl6SlotMapKeyDropValueV1(occupied);
        XL6_SlotMapKeyV1 copy={0};assert(xl6WaitQueueReceiveV1(q,OWNER,m.layout.elementIdentity,m.layout.type,
            (XL6_ChannelSlotV1){&copy,sizeof(copy)},XL6_CHANNEL_TRY_V1,0,&status));
        assert(status==XL6_CHANNEL_OK_V1&&xl6SlotMapKeyEqualV1(copy,m.input.key));xl6SlotMapKeyDropValueV1(copy);
        xrtValueRelease(q);xrtTypeDropValue(m.layout.type,&m.input);balanced(&before);
    }
    {
        /* Trusted native publishers may have a checked Copy failure. This
         * permanent test descriptor adds no graph/callback provider and no
         * implementation override. False must not become a successful send
         * merely because no error object was allocated. */
        xmemdebugsnapshot before={0};xrtMemDebugSnapshot(&before);Message m=message(1);
        static XL6_NativeCollectionValueProfileV1 fault;
        fault=*xl6SlotMapKeyCollectionProfileV1Get();fault.Copy=refuseCopy;
        xvalue* schema=xl6SequenceNativeValueCreateV1(1,123,&fault);assert(schema);
        xvalue* q=xl6ChannelCreateTakeV1(OWNER,schema,1);assert(q);
        for(unsigned path=0;path<3;++path){
            xl6_error_scope scope={0};assert(xl6ErrorScopeBegin(&scope));
            XL6_ChannelStatusV1 status=XL6_CHANNEL_TIMEOUT_V1;
            if(!path)assert(!xl6ChannelSendV1(q,OWNER,m.layout.elementIdentity,m.layout.type,
                (XL6_ChannelSlotV1){&m.input,m.layout.size},XL6_CHANNEL_TRY_V1,0,&status)&&status==XL6_CHANNEL_TIMEOUT_V1);
            else if(path==1)assert(!api->PrepareSend(m.layout,q,&m.input));
            else assert(!xl6ChannelCaseCreateSchemaV1(m.layout,q,&m.input,true));
            xl6_error_result outcome=xl6ErrorScopeEnd(&scope);assert(!outcome.ok&&!outcome.error&&!xrtGetError());
            assert(xl6ChannelQueryV1(q,OWNER,XL6_CHANNEL_QUERY_COUNT_V1)==0);
        }
        xrtValueRelease(q);xrtTypeDropValue(m.layout.type,&m.input);balanced(&before);
    }
    printf("owned Channel native: real Future edges, detached policy/results, native keys, nullable null, exact profile and occupied-output refusal; allocation prefixes");
    for(unsigned kind=0;kind<3;++kind)printf(" %u/%u/%u",prefixes[kind][0],prefixes[kind][1],prefixes[kind][2]);
    puts("; checked false/NULL Copy on sync/case/async preparation; balanced");return 0;
}
