/* Private heap-frame prerequisite for asynchronous Channel. This is NOT a
 * Future API: only real native registration, typed leases and ownership moves. */
#include "src/runtime/xl6_channel_case_internal.h"
#include "src/runtime/xl6_channel_internal.h"
#include "src/runtime/xl6_array_admission.h"
#include "src/api/xl_foreign_error.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define ID UINT64_C(0xCA7101)
#define ELEMENT UINT64_C(0xCA7102)
#define SLOT(Value) (XL6_ChannelSlotV1){&(Value),sizeof(Value)}
typedef struct Frame {
    XL6_ChannelOwnedSelectFrameV1 owned;
    XL6_ChannelSelectFrameV1 raw;
    xchannelsubscription* subscription;
    xchannelselectresult native;
    void* received;
    xvalue* resultHandle;
    size_t refs;
    bool ready,active;
} Frame;
static size_t drops;
static bool frame_count(const void* data,size_t* output)
{
    const Frame* frame=data;
    if(!frame || !output || !frame->refs || frame->active) return false;
    *output=frame->refs;return true;
}
static bool frame_trace(const void* data,xrtownershipvisitor visit,ptr context)
{
    const Frame* frame=data;
    return !frame->active && (!frame->subscription || visit(xrtChannelSubscribeOwnership(frame->subscription),context)) &&
        xl6ChannelSelectTraceV1(&frame->raw,visit,context) && xl6ChannelOwnedSelectTraceV1(&frame->owned,visit,context) &&
        (!frame->resultHandle || visit(xrtValueOwnership(frame->resultHandle),context));
}
static const xrtownershipops frame_ops={frame_count,frame_trace};
static void notify(xchannelsubscription* subscription,ptr data)
{
    Frame* frame=data;xrtownershipscope scope={0};
    assert(!frame->ready && xrtOwnershipMutationBegin(&scope));
    assert(xrtChannelSubscribeResult(subscription,&frame->native,&frame->received));
    frame->ready=true;
    assert(xrtOwnershipScopeEnd(&scope));
}
static void drop(const void* data)
{
    Frame* frame=(Frame*)data;
    assert(frame->refs==1 && !frame->subscription && !frame->raw.count && !frame->owned.count && !frame->resultHandle);
    frame->refs=0;++drops;xrtFree(frame);
}
static const xchannelsubscribeownershipv1 policy={sizeof(policy),notify,drop,&frame_ops};
static xvalue* channel(unsigned kind,size_t capacity)
{
    xvalue* schema=kind==3 ? xl6SequenceReferenceCreateV1(1,123,ELEMENT,XL6_VALUE_ADMIT_ANY,true,ELEMENT) :
        xl6SequencePrimitiveCreateV1(1,123,ELEMENT,kind,kind==2 ? sizeof(xstrview) : sizeof(int64_t),
            kind==2 ? xl6StringTypeV1() : xrtTypeInt64(),kind==2 ? XL6_VALUE_ADMIT_STRING : XL6_VALUE_ADMIT_I64,false,ELEMENT);
    return xl6ChannelCreateTakeV1(ID,schema,(int64_t)capacity);
}
static size_t edges(xrtownershipref ref,ptr data)
{ (void)ref;return ++*(size_t*)data; }
static bool visit(xrtownershipref ref,ptr data)
{ return edges(ref,data)>0; }
static void inspect(Frame* frame,bool registered)
{
    xrtownershipscope freeze={0};xrtownershipsnapshot* snapshot=NULL;
    xrtownershipref anchor={frame,&frame_ops};size_t ownerCount=0,caseCount=0,rawEdges=0,ownedEdges=0;
    assert(xrtOwnershipFreezeTryBegin(&freeze));
    xrtownershipref owner=xl6ChannelOperationOwnershipV1(frame->raw.operations);
    assert(owner.Ops->Count(owner.Data,&ownerCount) && ownerCount==frame->raw.count+1u);
    xrtownershipref item=xl6ChannelCaseOwnershipV1(frame->owned.pins[0]);
    assert(item.Ops->Count(item.Data,&caseCount) && caseCount==frame->owned.count);
    assert(xl6ChannelSelectTraceV1(&frame->raw,visit,&rawEdges) && rawEdges==frame->raw.count);
    assert(xl6ChannelOwnedSelectTraceV1(&frame->owned,visit,&ownedEdges) &&
        ownedEdges==frame->owned.count+(frame->owned.resultBorrowed ? 0u : 1u));
    /* All objects here are trusted resident fixtures/builtin values. Read-only
     * snapshot, not certification of unknown policies for graph collection. */
    assert(xrtOwnershipSnapshotCreate(&anchor,1,NULL,0,NULL,NULL,&snapshot));
    bool found=false;
    for(size_t i=0;i<xrtOwnershipSnapshotNodeCount(snapshot);++i) {
        xrtownershipnode node;assert(xrtOwnershipSnapshotNode(snapshot,i,&node));
        if(node.Reference.Data==frame->subscription) {
            assert(node.InternalCount==1 && node.StrongCount==(registered ? 2u : 1u) && node.Reachable==registered);
            found=true;
        }
        if(node.Reference.Data==owner.Data) assert(node.InternalCount==ownerCount);
    }
    assert(found);xrtOwnershipSnapshotDestroy(snapshot);assert(xrtOwnershipScopeEnd(&freeze));
}
static Frame* prepare(xvalue* q,const xrttype* type,size_t bytes,size_t count,bool verifyNoAllocation)
{
    Frame* frame=xrtCalloc(1,sizeof(*frame));
    XL6_ChannelCaseOwnerV1* item=NULL;
    if(!frame)return NULL;
    frame->refs=1;
    item=xl6ChannelReceiveCaseCreateV1(q,ID,ELEMENT,type,bytes);
    if(!item)goto failed;
    XL6_ChannelCaseOwnerV1* inputs[9];assert(count<=9);
    for(size_t i=0;i<count;++i)inputs[i]=item;
    XL6_ChannelWaitOptionsV2 options={sizeof(options),XL6_CHANNEL_WAIT_V1,0,0};
    XL6_ChannelSelectionV1 info={0};
    if(!xl6ChannelOwnedSelectPrepareV1(inputs,count,&options,&frame->owned) ||
        !xl6ChannelSelectPrepareV1(frame->owned.cases,count,&info,&frame->raw))goto failed;
    if(count==9) {
        frame->resultHandle=xl6ChannelOwnedSelectResultHandleV1(&frame->owned);
        if(!frame->resultHandle)goto failed;
    }
    if(verifyNoAllocation)assert(xrtMemDebugFailAfter(0));
    assert(xl6ChannelOwnedSelectParkV1(&frame->owned) && xl6ChannelSelectParkV1(&frame->raw));
    if(verifyNoAllocation) { assert(!xrtMemDebugFailTriggered());xrtMemDebugFailClear(); }
    xchannelsubscribecase native[9];
    for(size_t i=0;i<count;++i)native[i]=(xchannelsubscribecase){frame->raw.native[i].Channel,XCHANNEL_OP_RECV,NULL};
    frame->subscription=xrtChannelSubscribeCreateOwnedV1(native,count,frame,&policy);
    if(!frame->subscription)goto failed;
    /* Creator goes away; every remaining case reference is an actual frame pin. */
    xl6ChannelCaseDestroyV1(item);return frame;
failed: {
    xerror* error=xrtTakeError();
    xl6ChannelSelectDiscardV1(&frame->raw);(void)xl6ChannelOwnedSelectDiscardV1(&frame->owned,false);
    xrtValueRelease(frame->resultHandle);xl6ChannelCaseDestroyV1(item);
    xrtFree(frame);xrtClearError();xrtSetErrorTake(error);return NULL;
    }
}
typedef struct Result { XL6_ChannelSelectionOwnerV1* selection;xvalue* handle; } Result;
static Result drain(Frame* frame)
{
    assert(frame->ready);frame->active=true;
    if(frame->native.Wait==XWAIT_OK && frame->native.Result==XCHANNEL_OK)
        frame->raw.operations[frame->native.Index].received=frame->received;
    XL6_ChannelSelectionV1 info={0};
    assert(xrtMemDebugFailAfter(0));
    assert(xl6ChannelSelectCompleteV1(&frame->raw,frame->native,&info));
    assert(xl6ChannelOwnedSelectCompleteV1(&frame->owned,info));
    xl6ChannelSelectDiscardV1(&frame->raw);
    XL6_ChannelSelectionOwnerV1* result=xl6ChannelOwnedSelectDiscardV1(&frame->owned,true);
    xvalue* resultHandle=frame->resultHandle;frame->resultHandle=NULL;
    if(resultHandle) {
        void* pointer=NULL;void* user=NULL;const xvaluehandleops* ops=NULL;
        assert(!result && xrtValueGetHandle(resultHandle,&pointer,&ops,&user) && !user &&
            ops==xl6ChannelSelectionHandlePolicyV1Get()->Ops);
        result=pointer;
    }
    assert(result && !xrtMemDebugFailTriggered());xrtMemDebugFailClear();frame->active=false;
    xrtownershipscope mutation={0};assert(xrtOwnershipMutationBegin(&mutation));
    xchannelsubscription* subscription=frame->subscription;frame->subscription=NULL;
    assert(xrtOwnershipScopeEnd(&mutation));xrtChannelSubscribeDestroy(subscription);return (Result){result,resultHandle};
}
static void receive(unsigned kind,size_t count,bool stop)
{
    xvalue* q=channel(kind,0);assert(q);
    const xrttype* type=kind==1 ? xrtTypeInt64() : kind==2 ? xl6StringTypeV1() : xrtTypeValue();
    size_t bytes=kind==1 ? sizeof(int64_t) : kind==2 ? sizeof(xstrview) : sizeof(xvalue*);
    Frame* frame=prepare(q,type,bytes,count,true);assert(frame);inspect(frame,false);
    XL6_ChannelCaseOwnerV1* expected=frame->owned.pins[0];
    assert(xrtChannelSubscribeStart(frame->subscription));inspect(frame,true);
    int64_t number=41; xstrview text=XRT_STR_LITERAL("a\0你🙂");
    xvalue* binary=kind==3 && count!=1 ? xrtValueBytes((xbytesview){(cbytes)text.Data,text.Size}) : NULL;
    assert(kind!=3 || count==1 || binary);
    XL6_ChannelSlotV1 input=kind==1 ? SLOT(number) : kind==2 ? SLOT(text) : SLOT(binary);
    if(stop)assert(xrtChannelSubscribeStop(frame->subscription,XWAIT_CANCELLED));
    else {
        XL6_ChannelOperationV1 sending={0};XL6_ChannelStatusV1 status;
        assert(xl6ChannelOperationOpenV1(q,ID,ELEMENT,type,input,true,&status,&sending));
        assert(xrtMemDebugFailAfter(0));
        assert(xrtChannelTrySend(sending.native,sending.message)==XCHANNEL_OK);
        assert(xl6ChannelOperationCommitV1(&sending));xl6ChannelOperationCloseV1(&sending);
        assert(!xrtMemDebugFailTriggered());xrtMemDebugFailClear();
    }
    inspect(frame,false); /* Obtained box still belongs to the Channel owner. */
    Result completed=drain(frame);XL6_ChannelSelectionOwnerV1* result=completed.selection;XL6_ChannelSelectionV1 info={0};
    assert(xl6ChannelSelectionInfoV1(result,&info));
    if(stop)assert(info.index==SIZE_MAX && info.status==XL6_CHANNEL_CANCELLED_V1);
    else {
        assert(info.index<count && info.status==XL6_CHANNEL_OK_V1);
        int64_t got=0;xstrview gotText={0};xvalue* gotValue=NULL;
        XL6_ChannelSlotV1 output=kind==1 ? SLOT(got) : kind==2 ? SLOT(gotText) : SLOT(gotValue);
        assert(xl6ChannelSelectionValueV1(result,expected,ELEMENT,type,output));
        assert(kind==1 ? got==number : kind==2 ? gotText.Size==text.Size && !memcmp(gotText.Data,text.Data,text.Size) :
            binary ? xrtValueEqual(gotValue,binary) : !gotValue);
        xrtTypeDropValue(xl6StringTypeV1(),&gotText);xrtValueRelease(gotValue);
    }
    if(completed.handle)xrtValueRelease(completed.handle);else xl6ChannelSelectionDestroyV1(result);
    xrtValueRelease(binary);xrtValueRelease(q);assert(!xrtGetError());
}
static void parked_send(bool commit)
{
    xvalue* q=channel(2,1);assert(q);xstrview text=XRT_STR_LITERAL("send\0你");
    XL6_ChannelOperationV1 operation={0};XL6_ChannelStatusV1 status;
    assert(xl6ChannelOperationOpenV1(q,ID,ELEMENT,xl6StringTypeV1(),SLOT(text),true,&status,&operation));
    xrtownershipref owner=xl6ChannelOperationOwnershipV1(&operation);size_t count=0;
    xrtownershipscope freeze={0};assert(xrtOwnershipFreezeTryBegin(&freeze));
    assert(!owner.Ops->Count(owner.Data,&count));assert(xrtOwnershipScopeEnd(&freeze));
    assert(xl6ChannelOperationParkV1(&operation));assert(xrtOwnershipFreezeTryBegin(&freeze));
    assert(owner.Ops->Count(owner.Data,&count) && count==2);assert(xrtOwnershipScopeEnd(&freeze));
    if(commit) {
        assert(xrtChannelTrySend(operation.native,operation.message)==XCHANNEL_OK);
        assert(xl6ChannelOperationCommitV1(&operation));
    }
    xl6ChannelOperationCloseV1(&operation);xrtValueRelease(q);assert(!xrtGetError());
}
static void prefix(size_t count)
{
    xvalue* q=channel(2,0);assert(q);size_t point;
    for(point=0;point<64;++point) {
        xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);
        assert(xrtMemDebugFailAfter(point));
        Frame* frame=prepare(q,xl6StringTypeV1(),sizeof(xstrview),count,false);
        bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();xrtClearError();
        assert((frame!=NULL)==!hit);
        if(frame) {
            assert(xrtChannelSubscribeStart(frame->subscription) && xrtChannelSubscribeStop(frame->subscription,XWAIT_CANCELLED));
            Result result=drain(frame);
            if(result.handle)xrtValueRelease(result.handle);else xl6ChannelSelectionDestroyV1(result.selection);
        }
        assert(!xrtGetError());xrtMemDebugSnapshot(&after);
        assert(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes &&
            before.InvalidFreeCount==after.InvalidFreeCount && before.DoubleFreeCount==after.DoubleFreeCount);
        if(!hit)break;
    }
    assert(point<64);xrtValueRelease(q);printf("lease-count=%zu complete-construction-prefix=%zu\n",count,point);
}
int main(void)
{
    xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);
    for(unsigned kind=1;kind<=3;++kind)for(size_t count=1;count<=9;count+=4) {
        receive(kind,count,false);receive(kind,count,true);
    }
    parked_send(false);parked_send(true);prefix(1);prefix(9);
    xrtMemDebugSnapshot(&after);
    assert(drops==20 && before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes &&
        before.InvalidFreeCount==after.InvalidFreeCount && before.DoubleFreeCount==after.DoubleFreeCount &&
        before.UseAfterFreeCount==after.UseAfterFreeCount);
    puts("parked owned leases, native execution roots, typed moves and no late allocation passed");return 0;
}
