#include "release/include/xl_clock.h"
#include "src/runtime/xl6_channel_internal.h"
#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_ownership_plan.h"
#include "src/api/xl_foreign_error.h"
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define ID UINT64_C(0xCC61)
#define ELEMENT UINT64_C(0xCC62)
#define SLOT(Value) (XL6_ChannelSlotV1){&(Value),sizeof(Value)}
static XL6_ChannelWaitOptionsV2 options(unsigned mode,int64_t ms,uint64_t deadline)
{ return (XL6_ChannelWaitOptionsV2){sizeof(XL6_ChannelWaitOptionsV2),mode,ms,deadline}; }
static xvalue* create(unsigned kind,int64_t capacity)
{
    xvalue* seed=kind==3 ? xl6SequenceReferenceCreateV1(XL6_SEQUENCE_ARRAY_V1,123,ELEMENT,XL6_VALUE_ADMIT_ANY,true,ELEMENT) :
        xl6SequencePrimitiveCreateV1(XL6_SEQUENCE_ARRAY_V1,123,ELEMENT,kind,
            kind==2 ? sizeof(xstrview) : sizeof(int64_t),kind==2 ? xl6StringTypeV1() : xrtTypeInt64(),
            kind==2 ? XL6_VALUE_ADMIT_STRING : XL6_VALUE_ADMIT_I64,false,ELEMENT);
    xvalue* channel=xl6ChannelCreateTakeV1(ID,seed,capacity); assert(channel); return channel;
}
static bool send(xvalue* q,const xrttype* type,XL6_ChannelSlotV1 slot)
{
    XL6_ChannelStatusV1 status;
    assert(xl6ChannelSendV1(q,ID,ELEMENT,type,slot,XL6_CHANNEL_TRY_V1,0,&status)); return status==XL6_CHANNEL_OK_V1;
}
static XL6_ChannelStatusV1 receive(xvalue* q,const xrttype* type,XL6_ChannelSlotV1 slot)
{
    XL6_ChannelStatusV1 status;
    assert(xl6ChannelReceiveV1(q,ID,ELEMENT,type,slot,XL6_CHANNEL_TRY_V1,0,&status)); return status;
}
static XL6_ChannelCaseV1 item(xvalue* q,const xrttype* type,XL6_ChannelSlotV1 slot,bool sending)
{ return (XL6_ChannelCaseV1){q,ID,ELEMENT,type,slot,sending ? XL6_CHANNEL_CASE_SEND_V1 : XL6_CHANNEL_CASE_RECEIVE_V1}; }
static void error(void) { assert(xrtGetError()); xrtClearError(); }
static bool failCopy,failSelect,failClose;
bool xl6ChannelSelectProbeCopy(const xrttype* type,void* target,const void* source)
{ return failCopy ? false : xrtTypeCopyValue(type,target,source); }
xchannelselectresult xl6ChannelSelectProbeTry(const xchannelcase* cases,size_t count)
{ return failSelect ? (xchannelselectresult){XWAIT_ERROR,SIZE_MAX,XCHANNEL_ERROR} : xrtChannelSelectTry(cases,count); }
void xl6ChannelSelectProbeClose(XL6_ChannelOperationV1* operation)
{ xl6ChannelOperationCloseV1(operation); if(failClose) xl6ErrorScopePropagateFailure((xl6_error_result){false,NULL}); }
static void logicalFailures(void)
{
    for(unsigned mode=0;mode<3;++mode) {
        xvalue* q=create(2,1); xstrview source=XRT_STR_LITERAL("logical\0你"),target={0};
        bool receiving=mode==2;
        if(receiving) assert(send(q,xl6StringTypeV1(),SLOT(source)));
        XL6_ChannelCaseV1 cases[]={item(q,xl6StringTypeV1(),receiving ? SLOT(target) : SLOT(source),!receiving)};
        XL6_ChannelSelectionV1 selected={93,XL6_CHANNEL_FULL_V1};
        xl6_error_scope scope={0}; assert(xl6ErrorScopeBegin(&scope));
        failCopy=mode==0; failSelect=mode==1; failClose=mode==2;
        assert(!xl6ChannelSelectV1(cases,1,options(XL6_CHANNEL_TRY_V1,0,0),&selected));
        failCopy=failSelect=failClose=false;
        xl6_error_result result=xl6ErrorScopeEnd(&scope);
        assert(!result.ok && !result.error && !xrtGetError());
        assert(selected.index==93 && selected.status==XL6_CHANNEL_FULL_V1 && xl6ChannelQueryV1(q,ID,0)==0);
        assert(receiving ? target.Data && target.Size==source.Size && !memcmp(target.Data,source.Data,source.Size) : !target.Data);
        xrtTypeDropValue(xl6StringTypeV1(),&target); xrtValueRelease(q);
    }
}
static void vectors(void)
{
    xvalue* q=create(1,1); int64_t value=7,output=99; XL6_ChannelStatusV1 status=XL6_CHANNEL_FULL_V1;
    assert(xl6ChannelSendWaitV2(q,ID,ELEMENT,xrtTypeInt64(),SLOT(value),options(XL6_CHANNEL_UNTIL_V2,0,0),&status));
    assert(status==XL6_CHANNEL_OK_V1);
    assert(xl6ChannelSendWaitV2(q,ID,ELEMENT,xrtTypeInt64(),SLOT(value),options(XL6_CHANNEL_UNTIL_V2,0,0),&status));
    assert(status==XL6_CHANNEL_TIMEOUT_V1);
    xcancel* token=xrtCancelCreate(); assert(token && xrtCancelRequest(token));
    assert(xl6ChannelReceiveCancelV2(q,ID,ELEMENT,xrtTypeInt64(),SLOT(output),options(XL6_CHANNEL_WAIT_V1,0,0),token,&status));
    assert(status==XL6_CHANNEL_OK_V1 && output==7); output=99;
    assert(xl6ChannelReceiveCancelV2(q,ID,ELEMENT,xrtTypeInt64(),SLOT(output),options(XL6_CHANNEL_WAIT_V1,0,0),token,&status));
    assert(status==XL6_CHANNEL_CANCELLED_V1 && output==99);
    assert(xl6ChannelReceiveCancelV2(q,ID,ELEMENT,xrtTypeInt64(),SLOT(output),options(XL6_CHANNEL_TRY_V1,0,0),NULL,&status));
    assert(status==XL6_CHANNEL_EMPTY_V1 && output==99);
    XL6_ChannelWaitOptionsV2 bad[]={options(XL6_CHANNEL_FOR_V1,-1,0),options(XL6_CHANNEL_FOR_V1,INT64_MAX,0),
        options(XL6_CHANNEL_UNTIL_V2,0,UINT64_MAX),options(XL6_CHANNEL_WAIT_V1,1,0),options(XL6_CHANNEL_FOR_V1,0,1),options(4,0,0)};
    for(size_t i=0;i<sizeof(bad)/sizeof(*bad);++i) {
        status=XL6_CHANNEL_FULL_V1;
        assert(!xl6ChannelReceiveWaitV2(q,ID,ELEMENT,xrtTypeInt64(),SLOT(output),bad[i],&status)); error();
        assert(status==XL6_CHANNEL_FULL_V1 && output==99 && xl6ChannelQueryV1(q,ID,0)==0);
    }
    xrtCancelDestroy(token); xrtValueRelease(q);

    for(unsigned ready=0;ready<3;++ready) {
        xvalue* channels[3]={create(1,1),create(2,1),create(3,1)};
        xstrview text=XRT_STR_LITERAL("a\0你🙂"),gotText={0};
        xvalue* binary=xrtValueBytes((xbytesview){(cbytes)text.Data,text.Size}),*gotBytes=NULL; assert(binary);
        int64_t got=0;
        assert(ready==0 ? send(channels[0],xrtTypeInt64(),SLOT(value)) : ready==1 ?
            send(channels[1],xl6StringTypeV1(),SLOT(text)) : send(channels[2],xrtTypeValue(),SLOT(binary)));
        XL6_ChannelCaseV1 cases[]={item(channels[0],xrtTypeInt64(),SLOT(got),false),
            item(channels[1],xl6StringTypeV1(),SLOT(gotText),false),item(channels[2],xrtTypeValue(),SLOT(gotBytes),false)};
        XL6_ChannelSelectionV1 selected={SIZE_MAX,XL6_CHANNEL_FULL_V1};
        assert(xrtMemDebugFailAfter(0));
        assert(xl6ChannelSelectV1(cases,3,options(XL6_CHANNEL_TRY_V1,0,0),&selected));
        assert(!xrtMemDebugFailTriggered()); xrtMemDebugFailClear();
        assert(selected.status==XL6_CHANNEL_OK_V1 && selected.index==ready);
        assert(ready==0 ? got==value && !gotText.Data && !gotBytes : ready==1 ?
            !got && !gotBytes && gotText.Size==text.Size && !memcmp(gotText.Data,text.Data,text.Size) : !got && !gotText.Data && gotBytes==binary);
        xrtTypeDropValue(xl6StringTypeV1(),&gotText); xrtValueRelease(gotBytes); xrtValueRelease(binary);
        for(unsigned i=0;i<3;++i) xrtValueRelease(channels[i]);
    }
    q=create(3,1); xvalue* none=NULL,*nullResult=NULL;
    assert(send(q,xrtTypeValue(),SLOT(none)));
    XL6_ChannelCaseV1 nullCase=item(q,xrtTypeValue(),SLOT(nullResult),false);
    XL6_ChannelSelectionV1 nullSelection;
    token=xrtCancelCreate(); assert(token && xrtCancelRequest(token));
    assert(xl6ChannelSelectCancelV1(&nullCase,1,options(XL6_CHANNEL_TRY_V1,0,0),token,&nullSelection));
    assert(nullSelection.index==0 && nullSelection.status==XL6_CHANNEL_OK_V1 && !nullResult);
    assert(xl6ChannelSelectV1(&nullCase,1,options(XL6_CHANNEL_TRY_V1,0,0),&nullSelection));
    assert(nullSelection.index==SIZE_MAX && nullSelection.status==XL6_CHANNEL_TIMEOUT_V1 && !nullResult);
    xrtCancelDestroy(token); xrtValueRelease(q);

    q=create(1,1); int64_t left=0,right=0; unsigned winners=0;
    XL6_ChannelCaseV1 duplicate[]={item(q,xrtTypeInt64(),SLOT(left),false),item(q,xrtTypeInt64(),SLOT(right),false)};
    for(unsigned i=0;i<32;++i) {
        value=(int64_t)i+1; left=right=0; assert(send(q,xrtTypeInt64(),SLOT(value)));
        XL6_ChannelSelectionV1 chosen;
        assert(xl6ChannelSelectV1(duplicate,2,options(XL6_CHANNEL_TRY_V1,0,0),&chosen));
        assert(chosen.status==XL6_CHANNEL_OK_V1 && chosen.index<2 && (chosen.index==0 ? left==value && !right : right==value && !left));
        winners|=1u<<chosen.index;
    }
    assert(winners==3); assert(xl6ChannelCloseV1(q,ID));
    XL6_ChannelSelectionV1 closed;
    assert(xl6ChannelSelectV1(duplicate,2,options(XL6_CHANNEL_TRY_V1,0,0),&closed));
    assert(closed.status==XL6_CHANNEL_CLOSED_V1 && closed.index<2 && xl6ChannelQueryV1(q,ID,0)==0);
    xrtValueRelease(q);
}
static void selectionValidation(void)
{
    xvalue* q=create(1,1); int64_t first=0,second=0;
    XL6_ChannelCaseV1 cases[]={item(q,xrtTypeInt64(),SLOT(first),false),item(q,xrtTypeInt64(),SLOT(second),false)};
    XL6_ChannelSelectionV1 selected={71,XL6_CHANNEL_FULL_V1};
    assert(!xl6ChannelSelectV1(cases,0,options(XL6_CHANNEL_TRY_V1,0,0),&selected)); error();
    cases[1].slot=cases[0].slot;
    assert(!xl6ChannelSelectV1(cases,2,options(XL6_CHANNEL_TRY_V1,0,0),&selected)); error();
    cases[1].slot=SLOT(second); cases[1].element++;
    assert(!xl6ChannelSelectV1(cases,2,options(XL6_CHANNEL_TRY_V1,0,0),&selected)); error();
    assert(selected.index==71 && selected.status==XL6_CHANNEL_FULL_V1 && !first && !second);
    cases[1].element--;
    xcancel* token=xrtCancelCreate(); assert(token && xrtCancelRequest(token));
    assert(xl6ChannelSelectCancelV1(cases,2,options(XL6_CHANNEL_WAIT_V1,0,0),token,&selected));
    assert(selected.status==XL6_CHANNEL_CANCELLED_V1 && selected.index==SIZE_MAX);
    xrtCancelDestroy(token); xrtValueRelease(q);
}
static const xrtownershipadapterv1* resolve(xrtownershipref ref,void* context)
{
    (void)context; const xrtownershipadapterv1* adapter=xl6ChannelOwnershipAdapterV1(ref);
    if(!adapter) adapter=xl6SequenceOwnershipAdapterV1(ref);
    if(!adapter) adapter=xrtValueOwnershipAdapterV1(ref);
    return adapter;
}
typedef struct Actor { xvalue* q; xvalue* other; xcancel* token; xatomic32 started; bool send,select,ok; XL6_ChannelStatusV1 status; } Actor;
static int32 actor(void* raw)
{
    Actor* state=raw; int64_t value=47,output=0; xrtAtomic32Store(&state->started,1,XMEMORY_RELEASE);
    XL6_ChannelWaitOptionsV2 wait=options(XL6_CHANNEL_WAIT_V1,0,0);
    if(state->select) {
        int64_t other=0; XL6_ChannelCaseV1 cases[]={item(state->q,xrtTypeInt64(),SLOT(output),false),
            item(state->other,xrtTypeInt64(),SLOT(other),false)}; XL6_ChannelSelectionV1 selected;
        state->ok=xl6ChannelSelectCancelV1(cases,2,wait,state->token,&selected); state->status=selected.status;
    } else state->ok=state->send ? xl6ChannelSendCancelV2(state->q,ID,ELEMENT,xrtTypeInt64(),SLOT(value),wait,state->token,&state->status) :
        xl6ChannelReceiveCancelV2(state->q,ID,ELEMENT,xrtTypeInt64(),SLOT(output),wait,state->token,&state->status);
    assert(!xrtGetError()); return 0;
}
static void threads(void)
{
    for(unsigned kind=0;kind<3;++kind) {
        Actor state={.q=create(1,1),.other=create(1,1),.token=xrtCancelCreate(),.send=kind==1,.select=kind==2};
        assert(state.token); xrtAtomic32Init(&state.started,0); int64_t value=31;
        if(state.send) assert(send(state.q,xrtTypeInt64(),SLOT(value)));
        xthread* worker=xrtThreadCreate(actor,&state,0); assert(worker);
        XL6_DeadlineV1 limit=xl6DeadlineAfterMillisecondsV1(5000); size_t refs=0;
        xrtownershipref native=xrtCancelOwnership(state.token);
        while(!xrtAtomic32Load(&state.started,XMEMORY_ACQUIRE) || !native.Ops->Count(native.Data,&refs) || refs<3) {
            assert(xl6MonotonicMillisecondsV1()<limit); xrtThreadYield();
        }
        for(unsigned root=0;root<(state.select ? 2u : 1u);++root) {
            /* Legacy V1 admission refuses an active owner with ERROR. A
             * contended world-freeze can first return BUSY, without a plan. */
            for(unsigned attempt=0;;++attempt) {
                assert(attempt<1000000); XL6_OwnershipPlan* graph=NULL;
                XL6_OwnershipPlanResult result=xl6OwnershipPlanTryCreate(xrtValueOwnership(root ? state.other : state.q),resolve,NULL,&graph);
                assert(!graph);
                if(result==XL6_OWNERSHIP_PLAN_BUSY) { xrtThreadYield(); continue; }
                assert(result==XL6_OWNERSHIP_PLAN_ERROR); xrtClearError(); break;
            }
        }
        assert(xrtCancelRequest(state.token));
        assert(xrtThreadWaitFor(worker,UINT64_C(5000000))==XWAIT_OK && state.ok && state.status==XL6_CHANNEL_CANCELLED_V1);
        xrtThreadDestroy(worker);
        assert(xl6ChannelQueryV1(state.q,ID,0)==(state.send ? 1 : 0));
        xrtCancelDestroy(state.token); xrtValueRelease(state.other); xrtValueRelease(state.q);
    }
}
typedef struct Rendezvous { xvalue* q; xvalue* other; bool ok; XL6_ChannelSelectionV1 chosen; } Rendezvous;
static int32 rendezvousActor(void* raw)
{
    Rendezvous* state=raw; int64_t value=68,output=0;
    XL6_ChannelCaseV1 cases[]={item(state->q,xrtTypeInt64(),SLOT(value),true),item(state->other,xrtTypeInt64(),SLOT(output),false)};
    state->ok=xl6ChannelSelectV1(cases,2,options(XL6_CHANNEL_FOR_V1,5000,0),&state->chosen);
    assert(!xrtGetError()); return 0;
}
static void rendezvous(void)
{
    Rendezvous state={.q=create(1,0),.other=create(1,1)};
    xthread* worker=xrtThreadCreate(rendezvousActor,&state,0); assert(worker);
    int64_t output=0; XL6_ChannelStatusV1 status;
    assert(xl6ChannelReceiveV1(state.q,ID,ELEMENT,xrtTypeInt64(),SLOT(output),XL6_CHANNEL_FOR_V1,5000,&status));
    assert(status==XL6_CHANNEL_OK_V1 && output==68);
    assert(xrtThreadWaitFor(worker,UINT64_C(5000000))==XWAIT_OK && state.ok && state.chosen.index==0 && state.chosen.status==XL6_CHANNEL_OK_V1);
    xrtThreadDestroy(worker); xrtValueRelease(state.other); xrtValueRelease(state.q);
}
static void prefix(const char* operation,size_t point)
{
    bool large=!strcmp(operation,"large"),cancel=!strcmp(operation,"cancel"); size_t count=large ? 10u : 3u;
    xvalue* channels[10]={0}; xstrview texts[10]; XL6_ChannelCaseV1 cases[10];
    for(size_t i=0;i<count;++i) { channels[i]=create(2,1); texts[i]=XRT_STR_LITERAL("prefix\0你"); cases[i]=item(channels[i],xl6StringTypeV1(),SLOT(texts[i]),true); }
    xcancel* token=cancel ? xrtCancelCreate() : NULL; assert(!cancel || token);
    xl6_error_scope scope={0}; assert(xl6ErrorScopeBegin(&scope)); assert(xrtMemDebugFailAfter(point));
    XL6_ChannelSelectionV1 selected={51,XL6_CHANNEL_FULL_V1};
    bool ok=cancel ? xl6ChannelSelectCancelV1(cases,count,options(XL6_CHANNEL_TRY_V1,0,0),token,&selected) :
        xl6ChannelSelectV1(cases,count,options(XL6_CHANNEL_TRY_V1,0,0),&selected);
    bool hit=xl6ProductMemoryEnd(); xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    assert(ok==outcome.ok && ok==!hit); xrtErrorFree(outcome.error); assert(!xrtGetError());
    size_t published=0;
    for(size_t i=0;i<count;++i) {
        int64_t queued=xl6ChannelQueryV1(channels[i],ID,0); assert(queued==0 || queued==1); published+=(size_t)queued;
        xstrview text={0}; XL6_ChannelStatusV1 status=receive(channels[i],xl6StringTypeV1(),SLOT(text));
        assert(status==(queued ? XL6_CHANNEL_OK_V1 : XL6_CHANNEL_EMPTY_V1));
        if(queued) assert(selected.index==i && text.Size==texts[i].Size && !memcmp(text.Data,texts[i].Data,text.Size));
        xrtTypeDropValue(xl6StringTypeV1(),&text); xrtValueRelease(channels[i]);
    }
    assert(published==(ok ? 1u : 0u));
    if(!ok) assert(selected.index==51 && selected.status==XL6_CHANNEL_FULL_V1);
    xrtCancelDestroy(token); xl6ProductMemoryVerify(); printf("hit=%u complete-prefix balanced\n",hit ? 1u : 0u);
}
int main(int argc,char** argv)
{
    xl6ProductMemoryBegin(SIZE_MAX);
    if(argc==3) { prefix(argv[1],(size_t)strtoull(argv[2],NULL,10)); return 0; }
    assert(argc==1 || argc==2);
    if(argc==2 && !strcmp(argv[1],"logical")) logicalFailures();
    else if(argc==2) for(unsigned i=0;i<1000;++i) { vectors(); selectionValidation(); }
    else { vectors(); selectionValidation(); threads(); rendezvous(); }
    assert(!xrtGetError() && !xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
    puts("Channel select foundation: heterogeneous atomic cases, exact statuses, cancellation and ownership balanced"); return 0;
}
