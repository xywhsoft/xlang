#include "xl6_text_value.h"
#include "xl6_channel_future_core.h"
#include "xl6_channel_direct_future.h"
#include "xl6_channel_internal.h"
#include "xl6_native_handle.h"
#include "src/api/xl_foreign_error.h"
#include <string.h>
#include <stdlib.h>

typedef struct XL6_ChannelReceiveResultV1 Result;
struct XL6_ChannelReceiveResultV1 {
    size_t refs,active,allocation;
    const void* claim;
    bool creator,cleared,dropping,dropped,ready,initialized;
    XL6_SequenceLayoutV1 layout;
    XL6_ChannelStatusV1 status;
    _Alignas(max_align_t) unsigned char value[];
};
static xatomic64 resultGate;
static const xrtownershipops resultOps;
static void enter(xrtownershipscope* scope)
{
    if(!xrtOwnershipMutationBegin(scope))abort();
    for(;;){uint64 expected=0;
        if(xrtAtomic64CompareExchange(&resultGate,&expected,1,XMEMORY_ACQUIRE,XMEMORY_RELAXED))return;
        xrtThreadYield();}
}
static void leave(xrtownershipscope* scope)
{xrtAtomic64Store(&resultGate,0,XMEMORY_RELEASE);if(!xrtOwnershipScopeEnd(scope))abort();}
static void capture(xl6_error_scope* scope)
{if(xrtGetError()||scope->failed)xl6ErrorScopeCaptureFailure(scope);}
static bool dispose(Result* result)
{
    xrtownershipscope mutation={0};enter(&mutation);
    if(result->dropped){leave(&mutation);return true;}
    if(result->active||result->dropping)abort();
    result->dropped=true;result->dropping=true;
    bool initialized=result->initialized;result->initialized=false;
    XL6_SequenceLayoutV1 layout=result->layout;leave(&mutation);
    xl6_error_scope scope={0};if(!xl6ErrorScopeBeginCleanup(&scope,false))abort();
    if(initialized)xrtTypeDropValue(xl6SequenceStorageTypeV1(layout.transfer,layout.type),result->value);
    capture(&scope);enter(&mutation);result->dropping=false;leave(&mutation);
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    if(!outcome.ok)xl6ErrorScopePropagateFailure(outcome);else xrtErrorFree(outcome.error);
    return outcome.ok;
}
static void release(const void* data)
{
    Result* result=(Result*)data;if(!result)return;
    xrtownershipscope mutation={0};enter(&mutation);
    if(!result->refs)abort();
    bool last=--result->refs==0;
    if(last){if(result->creator||result->active||result->dropping||(result->claim&&!result->cleared))abort();result->cleared=true;}
    leave(&mutation);if(last){(void)dispose(result);xrtFree(result);}
}
static Result* begin(const Result* data)
{
    Result* result=(Result*)data;xrtownershipscope mutation={0};enter(&mutation);
    bool ok=result&&result->creator&&result->refs&&result->refs<SIZE_MAX&&!result->cleared&&
        !result->claim&&!result->dropping&&!result->dropped&&result->active<SIZE_MAX;
    if(ok){++result->refs;++result->active;}leave(&mutation);
    if(!ok)xrtSetErrorKind(XERR_STATE);
    return ok?result:NULL;
}
static void end(Result* result)
{xrtownershipscope mutation={0};enter(&mutation);if(!result->active)abort();--result->active;leave(&mutation);release(result);}
void xl6ChannelReceiveResultDestroyV1(Result* result)
{
    if(!result)return;
    xrtownershipscope mutation={0};enter(&mutation);
    if(!result->creator)abort();
    result->creator=false;leave(&mutation);release(result);
}
static bool result_count(const void* data,size_t* count)
{
    const Result* result=data;
    if(!result||!count||!result->refs||result->active||result->dropping||result->cleared)return false;
    *count=result->refs;return true;
}
static bool result_trace(const void* data,xrtownershipvisitor visit,void* context)
{
    const Result* result=data;size_t count;
    return visit&&result_count(data,&count)&&(!result->initialized||result->layout.transfer!=3||
        visit(xrtValueOwnership(*(xvalue* const*)result->value),context));
}
static const xrtownershipops resultOps={result_count,result_trace};
static xrtownershipref result_ownership(const Result* result)
{return (xrtownershipref){result,result?&resultOps:NULL};}
static bool handle_trace(const xvalue* value,xrtownershipvisitor visit,void* context)
{
    void* raw=NULL;void* user=NULL;const xvaluehandleops* ops=NULL;
    return visit&&xrtValueGetHandle(value,&raw,&ops,&user)&&raw&&!user&&
        ops==xl6ChannelReceiveResultHandlePolicyV1Get()->Ops&&visit(result_ownership(raw),context);
}
XL6_DEFINE_NATIVE_HANDLE_POLICY(xl6ChannelReceiveResultHandlePolicyV1Get,xl6ChannelReceiveResultDestroyV1,handle_trace)
static bool hold(const void* data)
{
    Result* result=(Result*)data;
    if(!result->refs||result->refs==SIZE_MAX||result->cleared)return false;
    ++result->refs;return true;
}
static bool claim(const void* data,const void* token)
{
    Result* result=(Result*)data;
    if(!token||result->cleared||result->active||result->dropping||(result->claim&&result->claim!=token))return false;
    result->claim=token;return true;
}
static void restore(const void* data,const void* token)
{Result* result=(Result*)data;if(result->claim!=token||result->cleared)abort();result->claim=NULL;}
static void clear(const void* data,const void* token)
{Result* result=(Result*)data;if(result->claim!=token||result->cleared||result->active||result->dropping)abort();result->cleared=true;}
static bool finish(const void* data,const void* token)
{Result* result=(Result*)data;if(result->claim!=token||!result->cleared)abort();return dispose(result);}
static bool busy(const void* data)
{const Result* result=data;return result&&result->refs&&(result->active||result->dropping);}
const xrtownershipquiescencev1* xl6ChannelReceiveResultQuiescenceV1Get(void)
{static const xrtownershipquiescencev1 policy={sizeof(policy),&resultOps,busy};return &policy;}
static xvalue* create_result(XL6_SequenceLayoutV1 layout,Result** output)
{
    if(!output||!layout.type||!layout.elementIdentity||!layout.size||layout.size!=layout.type->Size||
        !layout.type->Align||layout.type->Align>_Alignof(max_align_t)||
        (layout.type->Align&(layout.type->Align-1))||layout.size>SIZE_MAX-sizeof(Result)||
        layout.transfer<1||layout.transfer>3){xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    Result* result=xrtCalloc(1,sizeof(*result)+layout.size);if(!result)return NULL;
    result->refs=1;result->creator=true;result->allocation=sizeof(*result)+layout.size;result->layout=layout;
    void* raw=result;xvalue* handle=xl6NativeHandleTakeV1(&raw,xl6ChannelReceiveResultHandlePolicyV1Get());
    if(!handle)return NULL;
    *output=result;return handle;
}
bool xl6ChannelReceiveResultInfoV1(const Result* source,int64_t* output)
{
    xl6_error_scope scope={0};if(!xl6ErrorScopeBegin(&scope))return false;
    Result* result=NULL;int64_t status=0;
    bool ok=output&&(uintptr_t)output%_Alignof(int64_t)==0&&(uintptr_t)output<=UINTPTR_MAX-sizeof(*output)&&
        (!source||!((uintptr_t)output<(uintptr_t)source+source->allocation&&
                     (uintptr_t)source<(uintptr_t)output+sizeof(*output)));
    if(!ok)xrtSetErrorKind(XERR_ARGUMENT);
    if(ok){result=begin(source);ok=result!=NULL;}
    if(ok){if(!result->ready){xrtSetErrorKind(XERR_STATE);ok=false;}else status=(int64_t)result->status;}
    if(!ok)xl6ErrorScopeCaptureFailure(&scope);
    if(result)end(result);
    capture(&scope);
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    if(!outcome.ok){xl6ErrorScopePropagateFailure(outcome);return false;}
    xrtErrorFree(outcome.error);*output=status;return true;
}
bool xl6ChannelReceiveResultReadV1(XL6_FutureValueProfileV1 profile,const Result* source,void* output)
{
    xl6_error_scope scope={0};if(!xl6ErrorScopeBegin(&scope))return false;
    Result* result=begin(source);XL6_SequenceLayoutV1 layout={0};bool ok=false;
    _Alignas(max_align_t) unsigned char local[64]={0};void* copy=NULL;
    if(result){layout=result->layout;
        if(!result->ready||result->status!=XL6_CHANNEL_OK_V1||!result->initialized)xrtSetErrorKind(XERR_STATE);
        else if(profile.size!=sizeof(profile)||profile.native||profile.type!=layout.elementIdentity||profile.valueSize!=layout.size||
            profile.kind!=(layout.transfer==1?XL6_FUTURE_VALUE_TRIVIAL:layout.transfer==2?XL6_FUTURE_VALUE_STRING:XL6_FUTURE_VALUE_XVALUE))xrtSetErrorKind(XERR_TYPE);
        else if(!output||(uintptr_t)output%layout.type->Align||(uintptr_t)output>UINTPTR_MAX-layout.size||
            ((uintptr_t)output<(uintptr_t)result+result->allocation&&(uintptr_t)result<(uintptr_t)output+layout.size)||
            (layout.transfer==3&&*(xvalue**)output)||(layout.transfer==2&&(((xstrview*)output)->Data||((xstrview*)output)->Size)))xrtSetErrorKind(XERR_ARGUMENT);
        else {copy=layout.size<=sizeof(local)?local:xrtCalloc(1,layout.size);
            if(copy)ok=xrtTypeCopyValue(xl6SequenceStorageTypeV1(layout.transfer,layout.type),copy,result->value);}
    }
    if(!ok)xl6ErrorScopeCaptureFailure(&scope);
    if(result)end(result);
    capture(&scope);
    if(copy&&scope.failed){xrtTypeDropValue(xl6SequenceStorageTypeV1(layout.transfer,layout.type),copy);capture(&scope);memset(copy,0,layout.size);}
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    if(outcome.ok&&copy){memcpy(output,copy,layout.size);memset(copy,0,layout.size);}
    if(copy&&copy!=local)xrtFree(copy);
    if(!outcome.ok){xl6ErrorScopePropagateFailure(outcome);return false;}
    xrtErrorFree(outcome.error);return true;
}

typedef XL6_ChannelFutureFrameV1 Frame;
typedef struct DirectState {XL6_ChannelOperationV1 operation;Result* result;} DirectState;
static const XL6_ChannelFutureProviderV1 sendProvider,receiveProvider;
static const XL6_ChannelFutureProviderV1* accepted(const Frame* frame)
{
    const XL6_ChannelFutureProviderV1* provider=xl6ChannelFutureCoreProviderV1(frame);
    return provider==&sendProvider||provider==&receiveProvider?provider:NULL;
}
static bool count_frame(const void* frame,size_t* count)
{return xl6ChannelFutureCoreCountV1(frame,accepted(frame),count);}
static bool trace_frame(const void* frame,xrtownershipvisitor visit,void* context)
{return xl6ChannelFutureCoreTraceV1(frame,accepted(frame),visit,context);}
static const xrtownershipops frameOps={count_frame,trace_frame};
static const xchannelsubscribeownershipv1 subscriptionPolicy={sizeof(subscriptionPolicy),
    xl6ChannelFutureCoreNotifyV1,xl6ChannelFutureCoreDataDropV1,&frameOps};
static const xcancelwatchownershipv1 cancelPolicy={sizeof(cancelPolicy),
    xl6ChannelFutureCoreCancelV1,xl6ChannelFutureCoreCancelDropV1,&frameOps};
static void notified(void* data,xchannelselectresult native,void* received)
{
    DirectState* state=data;
    if(native.Wait==XWAIT_OK){if(native.Index!=0)abort();
        if(native.Result==XCHANNEL_OK&&!state->operation.sending)state->operation.received=received;}
}
static bool complete(void* data,xchannelselectresult native,int64_t* scalar)
{
    DirectState* state=data;XL6_ChannelStatusV1 status;
    switch(native.Wait){
    case XWAIT_OK:
        if(native.Index!=0)abort();
        if(native.Result==XCHANNEL_OK){status=XL6_CHANNEL_OK_V1;if(!xl6ChannelOperationCommitV1(&state->operation))return false;}
        else if(native.Result==XCHANNEL_CLOSED)status=XL6_CHANNEL_CLOSED_V1;
        else {xrtSetErrorKind(XERR_STATE);return false;}break;
    case XWAIT_CANCELLED:status=XL6_CHANNEL_CANCELLED_V1;break;
    case XWAIT_TIMEOUT:status=XL6_CHANNEL_TIMEOUT_V1;break;
    default:return false;
    }
    if(state->operation.sending)*scalar=(int64_t)status;
    else {Result* result=state->result;xrtownershipscope mutation={0};enter(&mutation);
        if(!result||!result->creator||result->cleared||result->ready||result->active)abort();
        result->status=status;result->initialized=status==XL6_CHANNEL_OK_V1;result->ready=true;leave(&mutation);}
    return true;
}
static void cleanup(void* data)
{DirectState* state=data;state->result=NULL;xl6ChannelOperationCloseV1(&state->operation);}
static bool trace_state(const void* data,xrtownershipvisitor visit,void* context)
{
    const DirectState* state=data;
    return !state->operation.owner||(state->operation.parked&&visit(xl6ChannelOperationOwnershipV1(&state->operation),context));
}
static bool empty(const void* data)
{const DirectState* state=data;return !state->operation.owner&&!state->result;}
static const XL6_ChannelFutureProviderV1 sendProvider={sizeof(sendProvider),sizeof(DirectState),
    XL6_FUTURE_PAYLOAD_VALUE,&frameOps,&subscriptionPolicy,&cancelPolicy,notified,complete,cleanup,trace_state,empty};
static const XL6_ChannelFutureProviderV1 receiveProvider={sizeof(receiveProvider),sizeof(DirectState),
    XL6_FUTURE_PAYLOAD_XVALUE,&frameOps,&subscriptionPolicy,&cancelPolicy,notified,complete,cleanup,trace_state,empty};
static Frame* prepare(XL6_SequenceLayoutV1 layout,const xvalue* channel,const void* value,bool sending)
{
    xl6_error_scope scope={0};if(!xl6ErrorScopeBegin(&scope))return NULL;
    Frame* frame=xl6ChannelFutureCoreCreateV1(sending?&sendProvider:&receiveProvider);xvalue* handle=NULL;bool ok=false;
    if(frame){DirectState* state=xl6ChannelFutureCoreStateV1(frame);
        XL6_ChannelSlotV1 slot={(void*)value,layout.size};
        if(!sending){handle=create_result(layout,&state->result);if(!handle)goto done;
            slot.data=state->result->value;if(!xl6ChannelFutureCoreHandleTakeV1(frame,&handle))goto done;}
        XL6_ChannelStatusV1 status;
        if(!xl6ChannelOperationOpenSchemaV1(layout,channel,slot,sending,&status,&state->operation))goto done;
        if(!xl6ChannelOperationParkV1(&state->operation))goto done;
        xchannelsubscribecase native={state->operation.native,sending?XCHANNEL_OP_SEND:XCHANNEL_OP_RECV,state->operation.message};
        ok=xl6ChannelFutureCoreSubscribeV1(frame,&native,1);
    }
done:
    if(!ok||xrtGetError()||scope.failed)xl6ErrorScopeCaptureFailure(&scope);
    if(scope.failed){xl6ChannelFutureCoreDiscardV1(frame);frame=NULL;capture(&scope);}
    xrtValueRelease(handle);capture(&scope);xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    if(!outcome.ok){xl6ErrorScopePropagateFailure(outcome);return NULL;}
    xrtErrorFree(outcome.error);return frame;
}
static Frame* prepare_send(XL6_SequenceLayoutV1 layout,const xvalue* channel,const void* value)
{return prepare(layout,channel,value,true);}
static Frame* prepare_receive(XL6_SequenceLayoutV1 layout,const xvalue* channel)
{return prepare(layout,channel,NULL,false);}
static xfuture* start_send(XL6_ContinuationExecutor* executor,Frame* frame,xcancel* parent)
{
    if(xl6ChannelFutureCoreProviderV1(frame)!=&sendProvider){xrtSetErrorKind(XERR_TYPE);return NULL;}
    return xl6ChannelFutureCoreStartV1(executor,frame,NULL,NULL,0,parent);
}
static xfuture* start_receive(XL6_ContinuationExecutor* executor,Frame* frame,const xvalue* owner,const char* field,size_t size,xcancel* parent)
{
    if(xl6ChannelFutureCoreProviderV1(frame)!=&receiveProvider){xrtSetErrorKind(XERR_TYPE);return NULL;}
    return xl6ChannelFutureCoreStartV1(executor,frame,owner,field,size,parent);
}
const XL6_ChannelDirectFutureRuntimeV1* xl6ChannelDirectFutureRuntimeV1Get(void)
{
    static const XL6_ChannelDirectFutureRuntimeV1 api={prepare_send,prepare_receive,xl6ChannelFutureCoreResultHandleV1,
        start_send,start_receive,xl6ChannelFutureCoreDiscardV1,xl6ChannelFutureCoreOwnershipV1};return &api;
}
const xchannelsubscribeownershipv1* xl6ChannelDirectFutureSubscriptionPolicyV1Get(void) {return &subscriptionPolicy;}
const xcancelwatchownershipv1* xl6ChannelDirectFutureCancelPolicyV1Get(void) {return &cancelPolicy;}
const xrtownershipadapterv1* xl6ChannelDirectFutureOwnershipAdapterV1(xrtownershipref ref,const xrtownershippreparationv1** preparation)
{
    if(ref.Ops==&frameOps)return xl6ChannelFutureCoreAdapterV1(ref,accepted(ref.Data),preparation);
    static const xrtownershipadapterv1 adapter={sizeof(adapter),hold,release,claim,restore,NULL,clear,finish};size_t count;
    return ref.Ops==&resultOps&&result_count(ref.Data,&count)?&adapter:NULL;
}
