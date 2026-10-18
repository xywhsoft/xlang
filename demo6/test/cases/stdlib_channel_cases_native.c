#include "src/runtime/xl6_channel_case_internal.h"
#include "src/runtime/xl6_channel_internal.h"
#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_ownership_plan.h"
#include "src/runtime/xl6_ownership_domains.h"
#include "src/api/xl_foreign_error.h"
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define ID UINT64_C(0xC561)
#define ELEMENT UINT64_C(0xC562)
#define SLOT(Value) (XL6_ChannelSlotV1){&(Value),sizeof(Value)}
static XL6_ChannelWaitOptionsV2 options(unsigned mode,int64_t ms,uint64_t deadline)
{ return (XL6_ChannelWaitOptionsV2){sizeof(XL6_ChannelWaitOptionsV2),mode,ms,deadline}; }
static xvalue* channel(unsigned kind,int64_t capacity)
{
    xvalue* seed=kind==3 ? xl6SequenceReferenceCreateV1(1,123,ELEMENT,XL6_VALUE_ADMIT_ANY,true,ELEMENT) :
        xl6SequencePrimitiveCreateV1(1,123,ELEMENT,kind,kind==2 ? sizeof(xstrview) : sizeof(int64_t),
            kind==2 ? xl6StringTypeV1() : xrtTypeInt64(),kind==2 ? XL6_VALUE_ADMIT_STRING : XL6_VALUE_ADMIT_I64,false,ELEMENT);
    xvalue* result=xl6ChannelCreateTakeV1(ID,seed,capacity); assert(result); return result;
}
static void send(xvalue* q,const xrttype* type,XL6_ChannelSlotV1 value)
{
    XL6_ChannelStatusV1 status;
    assert(xl6ChannelSendV1(q,ID,ELEMENT,type,value,XL6_CHANNEL_TRY_V1,0,&status) && status==XL6_CHANNEL_OK_V1);
}
static XL6_ChannelSelectionV1 info(const XL6_ChannelSelectionOwnerV1* selected)
{ XL6_ChannelSelectionV1 result={0}; assert(selected && xl6ChannelSelectionInfoV1(selected,&result)); return result; }
static XL6_OwnershipCatalog* catalog;
static const xrtownershipadapterv1* resolve(xrtownershipref ref,void* context)
{
    (void)context;
    const xrtownershipadapterv1* adapter=NULL; const xrtownershippreparationv1* preparation=NULL;
    const xrtownershipfinishv1* completion=NULL;
    bool ok=xl6OwnershipCatalogResolveV1(ref,catalog,&adapter,&preparation,&completion);
    assert(!preparation && !completion); return ok ? adapter : NULL;
}
static void idle(XL6_ChannelCaseOwnerV1* item)
{
    XL6_OwnershipPlan* plan=NULL;
    assert(xl6OwnershipPlanTryCreate(xl6ChannelCaseOwnershipV1(item),resolve,NULL,&plan)==XL6_OWNERSHIP_PLAN_ROOTED);
    assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY);
}
static void vectors(void)
{
    xvalue* q=channel(2,2); char source[]="a\0你🙂"; xstrview text={source,sizeof(source)-1};
    XL6_ChannelCaseOwnerV1* sending=xl6ChannelSendCaseCreateV1(q,ID,ELEMENT,xl6StringTypeV1(),SLOT(text));
    XL6_ChannelCaseOwnerV1* receiving=xl6ChannelReceiveCaseCreateV1(q,ID,ELEMENT,xl6StringTypeV1(),sizeof(text));
    assert(sending && receiving); source[0]='z'; idle(sending); idle(receiving);
    XL6_ChannelSelectionOwnerV1* sent=xl6ChannelCasesSelectV1(&sending,1,options(XL6_CHANNEL_TRY_V1,0,0));
    assert(info(sent).status==XL6_CHANNEL_OK_V1 && info(sent).index==0);
    XL6_ChannelSelectionOwnerV1* first=xl6ChannelCasesSelectV1(&receiving,1,options(XL6_CHANNEL_TRY_V1,0,0));
    assert(info(first).status==XL6_CHANNEL_OK_V1);
    xstrview fresh=XRT_STR_LITERAL("second\0值"); send(q,xl6StringTypeV1(),SLOT(fresh));
    XL6_ChannelSelectionOwnerV1* second=xl6ChannelCasesSelectV1(&receiving,1,options(XL6_CHANNEL_TRY_V1,0,0));
    xstrview left={0},right={0};
    assert(xl6ChannelSelectionValueV1(first,receiving,ELEMENT,xl6StringTypeV1(),SLOT(left)));
    assert(xl6ChannelSelectionValueV1(second,receiving,ELEMENT,xl6StringTypeV1(),SLOT(right)));
    assert(left.Size==sizeof(source)-1 && left.Data[0]=='a' && left.Data[1]==0 && right.Size==fresh.Size &&
        !memcmp(right.Data,fresh.Data,right.Size));
    assert(left.Data!=right.Data);
    xrtTypeDropValue(xl6StringTypeV1(),&left); xrtTypeDropValue(xl6StringTypeV1(),&right);
    xl6ChannelSelectionDestroyV1(sent); xl6ChannelSelectionDestroyV1(first); xl6ChannelSelectionDestroyV1(second);
    xl6ChannelCaseDestroyV1(sending); xl6ChannelCaseDestroyV1(receiving); xrtValueRelease(q);

    for (unsigned ready=0;ready<3;++ready) {
        xvalue* queues[]={channel(1,1),channel(2,1),channel(3,1)};
        const xrttype* types[]={xrtTypeInt64(),xl6StringTypeV1(),xrtTypeValue()};
        size_t sizes[]={sizeof(int64_t),sizeof(xstrview),sizeof(xvalue*)};
        XL6_ChannelCaseOwnerV1* cases[3];
        int64_t number=41; text=XRT_STR_LITERAL("hetero\0你"); xvalue* binary=xrtValueBytes((xbytesview){(cbytes)text.Data,text.Size});
        XL6_ChannelSlotV1 slots[]={SLOT(number),SLOT(text),SLOT(binary)}; send(queues[ready],types[ready],slots[ready]);
        for (unsigned i=0;i<3;++i) { cases[i]=xl6ChannelReceiveCaseCreateV1(queues[i],ID,ELEMENT,types[i],sizes[i]); assert(cases[i]); }
        for (unsigned i=0;i<3;++i) xrtValueRelease(queues[i]);
        XL6_ChannelSelectionOwnerV1* selected=xl6ChannelCasesSelectV1(cases,3,options(XL6_CHANNEL_TRY_V1,0,0));
        assert(info(selected).index==ready && info(selected).status==XL6_CHANNEL_OK_V1);
        int64_t got=0; xstrview gotText={0}; xvalue* gotBytes=NULL;
        XL6_ChannelSlotV1 outputs[]={SLOT(got),SLOT(gotText),SLOT(gotBytes)};
        assert(xl6ChannelSelectionValueV1(selected,cases[ready],ELEMENT,types[ready],outputs[ready]));
        assert(ready==0 ? got==number : ready==1 ? gotText.Size==text.Size && !memcmp(gotText.Data,text.Data,text.Size) : xrtValueEqual(gotBytes,binary));
        xrtTypeDropValue(xl6StringTypeV1(),&gotText); xrtValueRelease(gotBytes); xrtValueRelease(binary);
        xl6ChannelSelectionDestroyV1(selected); for (unsigned i=0;i<3;++i) xl6ChannelCaseDestroyV1(cases[i]);
    }
}
static void states(void)
{
    xvalue* q=channel(3,1); xvalue* nothing=NULL;
    XL6_ChannelCaseOwnerV1* item=xl6ChannelReceiveCaseCreateV1(q,ID,ELEMENT,xrtTypeValue(),sizeof(xvalue*)); assert(item);
    xcancel* token=xrtCancelCreate(); assert(token && xrtCancelRequest(token));
    XL6_ChannelSelectionOwnerV1* cancelled=xl6ChannelCasesSelectCancelV1(&item,1,options(XL6_CHANNEL_WAIT_V1,0,0),token);
    assert(info(cancelled).status==XL6_CHANNEL_CANCELLED_V1 && info(cancelled).index==SIZE_MAX);
    xl6ChannelSelectionDestroyV1(cancelled); send(q,xrtTypeValue(),SLOT(nothing));
    XL6_ChannelSelectionOwnerV1* ready=xl6ChannelCasesSelectCancelV1(&item,1,options(XL6_CHANNEL_FOR_V1,0,0),token);
    assert(info(ready).status==XL6_CHANNEL_OK_V1 && info(ready).index==0);
    assert(xl6ChannelSelectionValueV1(ready,item,ELEMENT,xrtTypeValue(),SLOT(nothing)) && !nothing);
    xl6ChannelSelectionDestroyV1(ready);
    XL6_ChannelSelectionOwnerV1* absent=xl6ChannelCasesSelectV1(&item,1,options(XL6_CHANNEL_UNTIL_V2,0,0));
    assert(info(absent).status==XL6_CHANNEL_TIMEOUT_V1 && info(absent).index==SIZE_MAX);
    assert(xl6ChannelCloseV1(q,ID));
    XL6_ChannelSelectionOwnerV1* closed=xl6ChannelCasesSelectCancelV1(&item,1,options(XL6_CHANNEL_WAIT_V1,0,0),token);
    assert(info(closed).status==XL6_CHANNEL_CLOSED_V1 && info(closed).index==0);
    xl6ChannelSelectionDestroyV1(absent); xl6ChannelSelectionDestroyV1(closed);
    xl6ChannelCaseDestroyV1(item); xrtCancelDestroy(token); xrtValueRelease(q);
}
static void mixed(void)
{
    for (unsigned receiving=0;receiving<2;++receiving) {
        xvalue* in=channel(1,1),*out=channel(2,1); int64_t value=83,got=0;
        xstrview text=XRT_STR_LITERAL("send\0你"),blocked=XRT_STR_LITERAL("blocked"),result={0};
        if (receiving) { send(in,xrtTypeInt64(),SLOT(value)); send(out,xl6StringTypeV1(),SLOT(blocked)); }
        XL6_ChannelCaseOwnerV1* cases[]={xl6ChannelReceiveCaseCreateV1(in,ID,ELEMENT,xrtTypeInt64(),sizeof(value)),
            xl6ChannelSendCaseCreateV1(out,ID,ELEMENT,xl6StringTypeV1(),SLOT(text))};
        assert(cases[0] && cases[1]);
        XL6_ChannelSelectionOwnerV1* selected=xl6ChannelCasesSelectV1(cases,2,options(XL6_CHANNEL_TRY_V1,0,0));
        assert(info(selected).status==XL6_CHANNEL_OK_V1 && info(selected).index==(receiving ? 0u : 1u));
        if (receiving) assert(xl6ChannelSelectionValueV1(selected,cases[0],ELEMENT,xrtTypeInt64(),SLOT(got)) && got==value);
        else {
            XL6_ChannelStatusV1 status;
            assert(xl6ChannelReceiveV1(out,ID,ELEMENT,xl6StringTypeV1(),SLOT(result),XL6_CHANNEL_TRY_V1,0,&status));
            assert(status==XL6_CHANNEL_OK_V1 && result.Size==text.Size && !memcmp(result.Data,text.Data,text.Size));
            xrtTypeDropValue(xl6StringTypeV1(),&result);
        }
        xl6ChannelSelectionDestroyV1(selected);
        for (unsigned i=0;i<2;++i) xl6ChannelCaseDestroyV1(cases[i]);
        xrtValueRelease(in); xrtValueRelease(out);
    }
    xvalue* schema=xl6SequenceCScalarCreateV1(1,123,ELEMENT,xrtTypeFloat32(),ELEMENT);
    xvalue* q=xl6ChannelCreateTakeV1(ID,schema,1); assert(q);
    float value=1.25f,got=0; send(q,xrtTypeFloat32(),SLOT(value));
    XL6_ChannelCaseOwnerV1* item=xl6ChannelReceiveCaseCreateV1(q,ID,ELEMENT,xrtTypeFloat32(),sizeof(value)); assert(item);
    XL6_ChannelSelectionOwnerV1* selected=xl6ChannelCasesSelectV1(&item,1,options(XL6_CHANNEL_TRY_V1,0,0));
    assert(info(selected).status==XL6_CHANNEL_OK_V1);
    assert(xl6ChannelSelectionValueV1(selected,item,ELEMENT,xrtTypeFloat32(),SLOT(got)) && got==value);
    xl6ChannelSelectionDestroyV1(selected); xl6ChannelCaseDestroyV1(item); xrtValueRelease(q);
}
static void identities(void)
{
    xvalue* q=channel(1,1); int64_t value=17,output=73; send(q,xrtTypeInt64(),SLOT(value));
    XL6_ChannelCaseOwnerV1* cases[2];
    for (unsigned i=0;i<2;++i) { cases[i]=xl6ChannelReceiveCaseCreateV1(q,ID,ELEMENT,xrtTypeInt64(),sizeof(value)); assert(cases[i]); }
    XL6_ChannelSelectionOwnerV1* selected=xl6ChannelCasesSelectV1(cases,1,options(XL6_CHANNEL_TRY_V1,0,0)); assert(selected);
    assert(!xl6ChannelSelectionValueV1(selected,cases[1],ELEMENT,xrtTypeInt64(),SLOT(output)) && output==73); xrtClearError();
    assert(!xl6ChannelSelectionValueV1(selected,cases[0],ELEMENT+1,xrtTypeInt64(),SLOT(output)) && output==73); xrtClearError();
    assert(!xl6ChannelSelectionValueV1(selected,cases[0],ELEMENT,xrtTypeUInt64(),SLOT(output)) && output==73); xrtClearError();
    assert(xl6ChannelSelectionValueV1(selected,cases[0],ELEMENT,xrtTypeInt64(),SLOT(output)) && output==17);
    xl6ChannelSelectionDestroyV1(selected);
    XL6_ChannelWaitOptionsV2 bad[]={options(XL6_CHANNEL_FOR_V1,-1,0),options(XL6_CHANNEL_FOR_V1,INT64_MAX,0),
        options(XL6_CHANNEL_UNTIL_V2,0,UINT64_MAX),options(XL6_CHANNEL_WAIT_V1,1,0)};
    for (size_t i=0;i<sizeof(bad)/sizeof(*bad);++i) { assert(!xl6ChannelCasesSelectV1(cases,2,bad[i])); xrtClearError(); idle(cases[0]); }
    for (unsigned i=0;i<2;++i) xl6ChannelCaseDestroyV1(cases[i]);
    xrtValueRelease(q);
}
typedef struct Actor { XL6_ChannelCaseOwnerV1* item; XL6_ChannelSelectionOwnerV1* result; } Actor;
static int32 actor(void* raw)
{ Actor* value=raw; value->result=xl6ChannelCasesSelectV1(&value->item,1,options(XL6_CHANNEL_FOR_V1,5000,0)); assert(value->result); return 0; }
static void concurrentReuse(void)
{
    xvalue* q=channel(1,2); XL6_ChannelCaseOwnerV1* item=xl6ChannelReceiveCaseCreateV1(q,ID,ELEMENT,xrtTypeInt64(),sizeof(int64_t));
    assert(item); Actor left={item,NULL},right={item,NULL};
    xthread* a=xrtThreadCreate(actor,&left,0),*b=xrtThreadCreate(actor,&right,0); assert(a && b);
    int64_t value=31; send(q,xrtTypeInt64(),SLOT(value)); value=32; send(q,xrtTypeInt64(),SLOT(value));
    assert(xrtThreadWaitFor(a,UINT64_C(5000000))==XWAIT_OK && xrtThreadWaitFor(b,UINT64_C(5000000))==XWAIT_OK);
    xrtThreadDestroy(a); xrtThreadDestroy(b);
    int64_t first=0,second=0;
    assert(xl6ChannelSelectionValueV1(left.result,item,ELEMENT,xrtTypeInt64(),SLOT(first)));
    assert(xl6ChannelSelectionValueV1(right.result,item,ELEMENT,xrtTypeInt64(),SLOT(second)));
    assert((first==31 && second==32) || (first==32 && second==31)); idle(item);
    xl6ChannelSelectionDestroyV1(left.result); xl6ChannelSelectionDestroyV1(right.result);
    xl6ChannelCaseDestroyV1(item); xrtValueRelease(q);
}
static void rendezvous(void)
{
    xvalue* q=channel(2,0); xstrview text=XRT_STR_LITERAL("rendezvous\0你"),output={0};
    XL6_ChannelCaseOwnerV1* item=xl6ChannelSendCaseCreateV1(q,ID,ELEMENT,xl6StringTypeV1(),SLOT(text)); assert(item);
    Actor state={item,NULL}; xthread* worker=xrtThreadCreate(actor,&state,0); assert(worker);
    XL6_ChannelStatusV1 status;
    assert(xl6ChannelReceiveV1(q,ID,ELEMENT,xl6StringTypeV1(),SLOT(output),XL6_CHANNEL_FOR_V1,5000,&status));
    assert(status==XL6_CHANNEL_OK_V1 && output.Size==text.Size && !memcmp(output.Data,text.Data,text.Size));
    assert(xrtThreadWaitFor(worker,UINT64_C(5000000))==XWAIT_OK && info(state.result).status==XL6_CHANNEL_OK_V1);
    xrtThreadDestroy(worker); xrtTypeDropValue(xl6StringTypeV1(),&output);
    xl6ChannelSelectionDestroyV1(state.result); xl6ChannelCaseDestroyV1(item); xrtValueRelease(q);
}
static void cycles(bool selection)
{
    xvalue* q=channel(3,1),*graph=xrtValueObject(),*weak=xrtValueWeakRef(graph); assert(graph && weak);
    XL6_ChannelCaseOwnerV1* item=selection ? xl6ChannelReceiveCaseCreateV1(q,ID,ELEMENT,xrtTypeValue(),sizeof(graph)) :
        xl6ChannelSendCaseCreateV1(q,ID,ELEMENT,xrtTypeValue(),SLOT(graph)); assert(item);
    XL6_ChannelSelectionOwnerV1* result=NULL;
    if (selection) { send(q,xrtTypeValue(),SLOT(graph)); result=xl6ChannelCasesSelectV1(&item,1,options(XL6_CHANNEL_TRY_V1,0,0)); assert(result); }
    void* payload=selection ? (void*)result : (void*)item;
    xvalue* handle=xl6NativeHandleTakeV1(&payload,selection ? xl6ChannelSelectionHandlePolicyV1Get() : xl6ChannelCaseHandlePolicyV1Get());
    assert(handle && !payload && xrtValueObjectSet(graph,XRT_STR_LITERAL("owner"),handle));
    XL6_OwnershipPlan* plan=NULL;
    assert(xl6OwnershipPlanTryCreate(selection ? xl6ChannelSelectionOwnershipV1(result) : xl6ChannelCaseOwnershipV1(item),
        resolve,NULL,&plan)==XL6_OWNERSHIP_PLAN_ROOTED);
    xrtValueRelease(handle); xrtValueRelease(graph); xrtValueRelease(q);
    if (selection) xl6ChannelCaseDestroyV1(item);
    assert(xl6OwnershipPlanFinalize(plan) && xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_READY);
    assert(xl6OwnershipPlanFinish(plan) && xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY);
    assert(xrtValueWeakRefExpired(weak)); xrtValueRelease(weak);
}
#if defined(XL6_CHANNEL_CASE_SOURCE_PROBE)
static bool failClose,failDrop;
static XL6_ChannelSelectionOwnerV1* destroyOnCopy;
void xl6ChannelCasesProbeClose(XL6_ChannelOperationV1* operation)
{
    xl6ChannelOperationCloseV1(operation);
    if (failClose) xl6ErrorScopePropagateFailure((xl6_error_result){false,NULL});
}
bool xl6ChannelCasesProbeCopy(const xrttype* type,void* target,const void* source)
{
    bool ok=xrtTypeCopyValue(type,target,source);
    if (ok && destroyOnCopy) {
        XL6_ChannelSelectionOwnerV1* selection=destroyOnCopy; destroyOnCopy=NULL;
        xl6ChannelSelectionDestroyV1(selection); failDrop=true;
    }
    return ok;
}
void xl6ChannelCasesProbeDrop(const xrttype* type,void* data)
{
    xrtTypeDropValue(type,data);
    if (failDrop) { failDrop=false; xl6ErrorScopePropagateFailure((xl6_error_result){false,NULL}); }
}
static void logicalFailures(void)
{
    for (unsigned mode=0;mode<2;++mode) {
        xvalue* q=channel(2,1); xstrview text=XRT_STR_LITERAL("committed\0你"),output={0}; send(q,xl6StringTypeV1(),SLOT(text));
        XL6_ChannelCaseOwnerV1* item=xl6ChannelReceiveCaseCreateV1(q,ID,ELEMENT,xl6StringTypeV1(),sizeof(text)); assert(item);
        XL6_ChannelSelectionOwnerV1* selection=NULL;
        if (mode) { selection=xl6ChannelCasesSelectV1(&item,1,options(XL6_CHANNEL_TRY_V1,0,0)); assert(selection); }
        xl6_error_scope scope={0}; assert(xl6ErrorScopeBegin(&scope));
        if (!mode) {
            failClose=true; selection=xl6ChannelCasesSelectV1(&item,1,options(XL6_CHANNEL_TRY_V1,0,0));
            failClose=false; assert(!selection);
        } else {
            destroyOnCopy=selection;
            assert(!xl6ChannelSelectionValueV1(selection,item,ELEMENT,xl6StringTypeV1(),SLOT(output)));
            assert(!destroyOnCopy && !failDrop && !output.Data && !output.Size);
        }
        xl6_error_result result=xl6ErrorScopeEnd(&scope);
        assert(!result.ok && !result.error && !xrtGetError() && xl6ChannelQueryV1(q,ID,0)==0);
        idle(item); xl6ChannelCaseDestroyV1(item); xrtValueRelease(q);
    }
}
#endif
static void prefix(const char* operation,size_t point)
{
    bool construction=!strcmp(operation,"create"),read=!strcmp(operation,"read");
    bool large=!strcmp(operation,"large") || !strcmp(operation,"send-large");
    bool sending=!strcmp(operation,"send") || !strcmp(operation,"send-large"),cancel=!strcmp(operation,"cancel");
    size_t count=large ? 20u : 3u;
    xvalue* queues[20]={0}; XL6_ChannelCaseOwnerV1* cases[20]={0}; xstrview texts[20];
    XL6_ChannelSelectionOwnerV1* selected=NULL; xstrview output={0};
    xcancel* token=cancel ? xrtCancelCreate() : NULL; assert(!cancel || (token && xrtCancelRequest(token)));
    for (size_t i=0;i<count;++i) {
        queues[i]=channel(2,1); texts[i]=XRT_STR_LITERAL("prefix\0你");
        if (!construction) cases[i]=sending ? xl6ChannelSendCaseCreateV1(queues[i],ID,ELEMENT,xl6StringTypeV1(),SLOT(texts[i])) :
            xl6ChannelReceiveCaseCreateV1(queues[i],ID,ELEMENT,xl6StringTypeV1(),sizeof(texts[i]));
        assert(construction || cases[i]);
    }
    if (read || (!construction && !sending && !cancel)) send(queues[0],xl6StringTypeV1(),SLOT(texts[0]));
    if (read) { selected=xl6ChannelCasesSelectV1(cases,count,options(XL6_CHANNEL_TRY_V1,0,0)); assert(selected); }
    xl6_error_scope scope={0}; assert(xl6ErrorScopeBegin(&scope) && xrtMemDebugFailAfter(point));
    bool ok;
    if (construction) { cases[0]=xl6ChannelSendCaseCreateV1(queues[0],ID,ELEMENT,xl6StringTypeV1(),SLOT(texts[0])); ok=cases[0]!=NULL; }
    else if (read) ok=xl6ChannelSelectionValueV1(selected,cases[0],ELEMENT,xl6StringTypeV1(),SLOT(output));
    else { selected=cancel ? xl6ChannelCasesSelectCancelV1(cases,count,options(XL6_CHANNEL_TRY_V1,0,0),token) :
        xl6ChannelCasesSelectV1(cases,count,options(XL6_CHANNEL_TRY_V1,0,0)); ok=selected!=NULL; }
    bool hit=xl6ProductMemoryEnd(); xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    assert(ok==outcome.ok && ok==!hit); xrtErrorFree(outcome.error); assert(!xrtGetError());
    if (read) assert(ok ? output.Size==texts[0].Size : !output.Data && !output.Size);
    if (!read && !construction) {
        if (sending) {
            size_t messages=0; for (size_t i=0;i<count;++i) messages+=(size_t)xl6ChannelQueryV1(queues[i],ID,0);
            assert(messages==(ok ? 1u : 0u));
        } else assert(xl6ChannelQueryV1(queues[0],ID,0)==(cancel || ok ? 0 : 1));
        if (ok && cancel) assert(info(selected).status==XL6_CHANNEL_CANCELLED_V1);
    }
    xrtTypeDropValue(xl6StringTypeV1(),&output); xl6ChannelSelectionDestroyV1(selected);
    for (size_t i=0;i<count;++i) { xl6ChannelCaseDestroyV1(cases[i]); xrtValueRelease(queues[i]); }
    xrtCancelDestroy(token);
    xl6ProductMemoryVerify(); printf("hit=%u complete-prefix balanced\n",hit ? 1u : 0u);
}
int main(int argc,char** argv)
{
    xl6ProductMemoryBegin(SIZE_MAX);
    if (argc==3) { prefix(argv[1],(size_t)strtoull(argv[2],NULL,10)); return 0; }
    const XL6_OwnershipDomainV1* domains[]={xl6OwnershipDomainChannelCaseV1Get(),
        xl6OwnershipDomainChannelV1Get(),xl6OwnershipDomainSequenceV1Get()};
    catalog=xl6OwnershipCatalogCreateV1(domains,sizeof(domains)/sizeof(*domains)); assert(catalog);
    assert(argc==1); vectors(); states(); mixed(); identities(); concurrentReuse(); rendezvous(); cycles(false); cycles(true);
#if defined(XL6_CHANNEL_CASE_SOURCE_PROBE)
    logicalFailures();
#endif
    xl6OwnershipCatalogDestroyV1(catalog); catalog=NULL;
    assert(!xrtGetError() && !xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
    puts("Owned Channel cases: snapshots, heterogeneous selection, independent results, cancellation, cycles balanced"); return 0;
}
