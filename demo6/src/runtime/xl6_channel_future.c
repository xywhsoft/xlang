#include "xl6_text_value.h"
#include "xl6_channel_future_core.h"
#include "xl6_channel_case_internal.h"
#include "xl6_channel_internal.h"
#include "src/api/xl_foreign_error.h"

typedef XL6_ChannelFutureFrameV1 Frame;
typedef struct SelectState {
    XL6_ChannelOwnedSelectFrameV1 owned;
    XL6_ChannelSelectFrameV1 operations;
    XL6_ChannelSelectionV1 selected;
} SelectState;
static const XL6_ChannelFutureProviderV1 provider;
static bool count_frame(const void* frame,size_t* count)
{return xl6ChannelFutureCoreCountV1(frame,&provider,count);}
static bool trace_frame(const void* frame,xrtownershipvisitor visit,void* context)
{return xl6ChannelFutureCoreTraceV1(frame,&provider,visit,context);}
static const xrtownershipops frameOps={count_frame,trace_frame};
static const xchannelsubscribeownershipv1 subscriptionPolicy={sizeof(subscriptionPolicy),
    xl6ChannelFutureCoreNotifyV1,xl6ChannelFutureCoreDataDropV1,&frameOps};
static const xcancelwatchownershipv1 cancelPolicy={sizeof(cancelPolicy),
    xl6ChannelFutureCoreCancelV1,xl6ChannelFutureCoreCancelDropV1,&frameOps};
static void notified(void* data,xchannelselectresult result,void* received)
{
    SelectState* state=data;
    if(result.Wait==XWAIT_OK&&result.Result==XCHANNEL_OK){
        if(result.Index>=state->operations.count)abort();
        if(!state->operations.operations[result.Index].sending)
            state->operations.operations[result.Index].received=received;
    }
}
static bool complete(void* data,xchannelselectresult result,int64_t* unused)
{
    SelectState* state=data;(void)unused;
    return xl6ChannelSelectCompleteV1(&state->operations,result,&state->selected)&&
        xl6ChannelOwnedSelectCompleteV1(&state->owned,state->selected);
}
static void cleanup(void* data)
{
    SelectState* state=data;xl6_error_scope scope={0};if(!xl6ErrorScopeBeginCleanup(&scope,false))abort();
    xl6ChannelSelectDiscardV1(&state->operations);
    if(xrtGetError()||scope.failed)xl6ErrorScopeCaptureFailure(&scope);
    (void)xl6ChannelOwnedSelectDiscardV1(&state->owned,false);
    if(xrtGetError()||scope.failed)xl6ErrorScopeCaptureFailure(&scope);
    xl6_error_result result=xl6ErrorScopeEnd(&scope);
    if(!result.ok)xl6ErrorScopePropagateFailure(result);else xrtErrorFree(result.error);
}
static bool trace_state(const void* data,xrtownershipvisitor visit,void* context)
{
    const SelectState* state=data;
    return xl6ChannelOwnedSelectTraceV1(&state->owned,visit,context)&&
        xl6ChannelSelectTraceV1(&state->operations,visit,context);
}
static bool empty(const void* data)
{
    const SelectState* state=data;
    return !state->owned.count&&!state->owned.result&&!state->owned.cases&&!state->owned.pins&&
        !state->owned.slots&&!state->operations.count&&!state->operations.operations&&!state->operations.native;
}
static const XL6_ChannelFutureProviderV1 provider={sizeof(provider),sizeof(SelectState),
    XL6_FUTURE_PAYLOAD_XVALUE,&frameOps,&subscriptionPolicy,&cancelPolicy,
    notified,complete,cleanup,trace_state,empty};
static Frame* prepare(XL6_ChannelCaseOwnerV1* const* cases,size_t count)
{
    xl6_error_scope scope={0};if(!xl6ErrorScopeBegin(&scope))return NULL;
    Frame* frame=xl6ChannelFutureCoreCreateV1(&provider);
    xchannelsubscribecase local[8],*native=local;xvalue* handle=NULL;bool ok=false;
    if(!frame)goto done;
    SelectState* state=xl6ChannelFutureCoreStateV1(frame);
    XL6_ChannelWaitOptionsV2 options={sizeof(options),XL6_CHANNEL_WAIT_V1,0,0};
    if(!xl6ChannelOwnedSelectPrepareV1(cases,count,&options,&state->owned))goto done;
    handle=xl6ChannelOwnedSelectResultHandleV1(&state->owned);
    if(!handle||!xl6ChannelFutureCoreHandleTakeV1(frame,&handle))goto done;
    if(!xl6ChannelSelectPrepareV1(state->owned.cases,count,&state->selected,&state->operations))goto done;
    if(!xl6ChannelOwnedSelectParkV1(&state->owned)||!xl6ChannelSelectParkV1(&state->operations))goto done;
    if(count>SIZE_MAX/sizeof(*native)){xrtSetErrorKind(XERR_RANGE);goto done;}
    if(count>8){native=xrtCalloc(count,sizeof(*native));if(!native)goto done;}
    for(size_t i=0;i<count;++i){XL6_ChannelOperationV1* item=state->operations.operations+i;
        native[i]=(xchannelsubscribecase){item->native,item->sending?XCHANNEL_OP_SEND:XCHANNEL_OP_RECV,item->message};
    }
    ok=xl6ChannelFutureCoreSubscribeV1(frame,native,count);
done:
    if(native!=local)xrtFree(native);
    if(!ok||xrtGetError()||scope.failed)xl6ErrorScopeCaptureFailure(&scope);
    if(scope.failed){xl6ChannelFutureCoreDiscardV1(frame);frame=NULL;
        if(xrtGetError()||scope.failed)xl6ErrorScopeCaptureFailure(&scope);}
    xrtValueRelease(handle);
    if(xrtGetError()||scope.failed)xl6ErrorScopeCaptureFailure(&scope);
    xl6_error_result result=xl6ErrorScopeEnd(&scope);
    if(!result.ok){xl6ErrorScopePropagateFailure(result);return NULL;}
    xrtErrorFree(result.error);return frame;
}
const XL6_ChannelFutureRuntimeV1* xl6ChannelFutureRuntimeV1Get(void)
{
    static const XL6_ChannelFutureRuntimeV1 api={prepare,xl6ChannelFutureCoreResultHandleV1,
        xl6ChannelFutureCoreStartV1,xl6ChannelFutureCoreDiscardV1,xl6ChannelFutureCoreOwnershipV1};return &api;
}
const xchannelsubscribeownershipv1* xl6ChannelFutureSubscriptionPolicyV1Get(void) {return &subscriptionPolicy;}
const xcancelwatchownershipv1* xl6ChannelFutureCancelPolicyV1Get(void) {return &cancelPolicy;}
const xfutureproducerownershipv1* xl6ChannelFutureProducerPolicyV1Get(void) {return xl6ChannelFutureCoreProducerPolicyV1Get();}
const XL6_ContinuationStorageV2* xl6ChannelFutureStorageV2Get(void) {return xl6ChannelFutureCoreStorageV2Get();}
const xrtownershipadapterv1* xl6ChannelFutureOwnershipAdapterV1(xrtownershipref ref,const xrtownershippreparationv1** plan)
{return xl6ChannelFutureCoreAdapterV1(ref,&provider,plan);}
