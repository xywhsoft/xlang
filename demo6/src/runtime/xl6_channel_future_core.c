#include "xl6_text_value.h"
#include "xl6_channel_future_core.h"
#include "xl6_native_handle.h"
#include "xl6_future_payload.h"
#include "src/api/xl_foreign_error.h"
#include <string.h>
#include <stdlib.h>

typedef struct XL6_ChannelFutureFrameV1 Frame;
struct XL6_ChannelFutureFrameV1 {
    xatomic64 gate;
    size_t references,active;
    const void* claim;
    bool builderPublished,committed,publishing,bound,retiring,retired,cleared;
    XL6_ContinuationTicket ticket;
    const XL6_ChannelFutureProviderV1* provider;
    const XL6_ChannelFutureWakeV1* wake;
    void* wakeData;
    xchannelselectresult nativeResult;
    xchannelsubscription *subscription,*cancelSubscription;
    xcancelwatch* cancelWatch;
    xcancel* cancel;
    xpromise* promise;
    xvalue* handle;
    void* payload;
    _Alignas(max_align_t) unsigned char state[];
};
static const XL6_ContinuationStorageV2 storage;
static void enter(Frame* frame,xrtownershipscope* mutation)
{
    if(!xrtOwnershipMutationBegin(mutation))abort();
    for(;;){uint64 expected=0;
        if(xrtAtomic64CompareExchange(&frame->gate,&expected,1,XMEMORY_ACQUIRE,XMEMORY_RELAXED))return;
        xrtThreadYield();
    }
}
static void leave(Frame* frame,xrtownershipscope* mutation)
{xrtAtomic64Store(&frame->gate,0,XMEMORY_RELEASE);if(!xrtOwnershipScopeEnd(mutation))abort();}
static bool hold(const void* data)
{
    Frame* frame=(Frame*)data;xrtownershipscope mutation={0};bool ok=false;
    if(!frame)return false;
    enter(frame,&mutation);
    if(frame->references&&frame->references<SIZE_MAX&&!frame->cleared){++frame->references;ok=true;}
    leave(frame,&mutation);return ok;
}
static void release(const void* data)
{
    Frame* frame=(Frame*)data;xrtownershipscope mutation={0};enter(frame,&mutation);
    if(!frame->references)abort();
    bool last=--frame->references==0;
    if(last&&(!frame->retired||frame->active||frame->subscription||frame->cancelSubscription||
        frame->cancelWatch||frame->cancel||frame->promise||frame->handle||frame->payload||frame->ticket.queue||
        (frame->wake&&!frame->wake->Empty(frame->wakeData))||
        !frame->provider->Empty(frame->state)))abort();
    leave(frame,&mutation);
    if(last){if(frame->wake)frame->wake->Drop(frame->wakeData);xrtFree(frame);}
}
bool xl6ChannelFutureCoreRetainV1(Frame* frame) {return hold(frame);}
xrtownershipref xl6ChannelFutureCoreOwnershipV1(const Frame* frame)
{return (xrtownershipref){frame,frame?frame->provider->ops:NULL};}
static xrtownershipref storage_ownership(const void* data) {return xl6ChannelFutureCoreOwnershipV1(data);}
static void commit(const void* data)
{
    Frame* frame=(Frame*)data;xrtownershipscope mutation={0};enter(frame,&mutation);
    if(frame->committed||!frame->publishing||!frame->bound||!frame->references)abort();
    frame->committed=true;frame->publishing=false;leave(frame,&mutation);
}
static const XL6_ContinuationStorageV2 storage={hold,release,storage_ownership,NULL,commit,NULL,NULL,NULL};
const XL6_ContinuationStorageV2* xl6ChannelFutureCoreStorageV2Get(void) {return &storage;}
void xl6ChannelFutureCoreDataDropV1(const void* data)
{
    /* Inspection pins may preserve the native subscription long after its
     * operation completed. This returns only its physical Data reference;
     * logical delivery must NOT wait for physical object destruction. */
    release(data);
}
void xl6ChannelFutureCoreNotifyV1(xchannelsubscription* subscription,void* data)
{
    Frame* frame=data;xerror* previous=xrtTakeError();void* received=NULL;
    xrtownershipscope mutation={0};enter(frame,&mutation);++frame->active;leave(frame,&mutation);
    xchannelselectresult result={0};if(!xrtChannelSubscribeResult(subscription,&result,&received))abort();
    enter(frame,&mutation);
    if(!frame->bound||frame->subscription!=subscription)abort();
    frame->nativeResult=result;
    frame->provider->Notify(frame->state,result,received);
    xchannelsubscription* owner=frame->subscription;frame->subscription=NULL;
    xcancelwatch* watch=frame->cancelWatch;frame->cancelWatch=NULL;
    leave(frame,&mutation);
    if(frame->wake)frame->wake->Detach(frame->wakeData);
    xrtCancelUnwatch(watch);xrtChannelSubscribeDestroy(owner);
    enter(frame,&mutation);--frame->active;leave(frame,&mutation);
    xrtClearError();xrtSetErrorTake(previous);
    /* Native completion has already unlinked EVERY case. No code below may
     * access operations/payload or execute language CFG. The native Data
     * reference still pins this frame through Notify/Start/Stop/dispatch tails,
     * independently of queue retirement and any inspection pins. */
    xl6ContinuationReady(&frame->ticket);
    xl6ContinuationTransportReleased(&frame->ticket); /* final frame access */
}
static const xfutureproducerownershipv1 producerPolicy={sizeof(producerPolicy),release};
const xfutureproducerownershipv1* xl6ChannelFutureCoreProducerPolicyV1Get(void) {return &producerPolicy;}
void xl6ChannelFutureCoreCancelV1(void* data)
{
    Frame* frame=data;xerror* previous=xrtTakeError();xrtownershipscope mutation={0};
    enter(frame,&mutation);++frame->active;leave(frame,&mutation);
    /* This external reference belongs to the observer until its LAST Drop,
     * after this callback returns, even if Notify reentrantly unwatches it. */
    if(!frame->cancelSubscription)abort();
    (void)xrtChannelSubscribeStop(frame->cancelSubscription,XWAIT_CANCELLED);
    enter(frame,&mutation);--frame->active;leave(frame,&mutation);
    xrtClearError();xrtSetErrorTake(previous);
}
void xl6ChannelFutureCoreCancelDropV1(const void* data)
{
    Frame* frame=(Frame*)data;xrtownershipscope mutation={0};enter(frame,&mutation);
    xchannelsubscription* subscription=frame->cancelSubscription;frame->cancelSubscription=NULL;
    leave(frame,&mutation);xrtChannelSubscribeDestroy(subscription);release(frame);
}
static void capture_failure(xl6_error_scope* scope)
{if(xrtGetError()||scope->failed)xl6ErrorScopeCaptureFailure(scope);}
static void cleanup_values(Frame* frame,xl6_error_scope* scope)
{
    if(frame->wake){frame->wake->Cleanup(frame->wakeData);capture_failure(scope);}
    frame->provider->Cleanup(frame->state,frame->claim);capture_failure(scope);
    xvalue* handle=frame->handle;frame->handle=NULL;xrtValueRelease(handle);capture_failure(scope);
    xcancel* cancel=frame->cancel;frame->cancel=NULL;xrtCancelDestroy(cancel);capture_failure(scope);
}
static bool discard_contents(Frame* frame)
{
    if(frame->committed||frame->publishing||frame->bound||frame->ticket.queue)abort();
    if(frame->retired)return true;
    xl6_error_scope scope={0};if(!xl6ErrorScopeBeginCleanup(&scope,false))abort();
    xrtownershipscope mutation={0};enter(frame,&mutation);
    if(frame->retiring)abort();
    frame->retiring=true;
    xcancelwatch* watch=frame->cancelWatch;frame->cancelWatch=NULL;
    xchannelsubscription* subscription=frame->subscription;frame->subscription=NULL;
    leave(frame,&mutation);
    if(frame->wake)frame->wake->Detach(frame->wakeData);
    xrtCancelUnwatch(watch);capture_failure(&scope);
    xrtChannelSubscribeDestroy(subscription);capture_failure(&scope);
    cleanup_values(frame,&scope);
    void* payload=frame->payload;frame->payload=NULL;
    xl6PayloadDropGet(frame->provider->payloadKind)(payload,NULL);capture_failure(&scope);
    xpromise* promise=frame->promise;frame->promise=NULL;xrtPromiseDestroy(promise);capture_failure(&scope);
    enter(frame,&mutation);frame->retired=true;frame->retiring=false;leave(frame,&mutation);
    xl6_error_result result=xl6ErrorScopeEnd(&scope);
    if(!result.ok)xl6ErrorScopePropagateFailure(result);else xrtErrorFree(result.error);
    return result.ok;
}
void xl6ChannelFutureCoreDiscardV1(Frame* frame)
{
    if(!frame)return;
    (void)discard_contents(frame);release(frame);
}
bool xl6ChannelFutureCorePublishBuilderV1(Frame* frame)
{
    if(!frame){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    xrtownershipscope mutation={0};enter(frame,&mutation);
    bool ok=frame->references&&!frame->builderPublished&&!frame->claim&&!frame->committed&&
        !frame->publishing&&!frame->bound&&!frame->active&&!frame->retiring&&!frame->retired&&
        !frame->cleared&&!frame->ticket.queue&&frame->subscription&&!frame->cancelSubscription&&
        !frame->cancelWatch&&!frame->cancel&&!frame->promise&&!frame->payload;
    if(ok)frame->builderPublished=true;
    leave(frame,&mutation);if(!ok)xrtSetErrorKind(XERR_STATE);return ok;
}
Frame* xl6ChannelFutureCoreCreateV1(const XL6_ChannelFutureProviderV1* provider)
{
    if(!provider || provider->size!=sizeof(*provider) || !provider->stateSize ||
        provider->stateSize>SIZE_MAX-sizeof(Frame) || !provider->ops || !provider->subscription ||
        !provider->cancellation || !provider->Notify || !provider->Complete || !provider->Cleanup ||
        !provider->Trace || !provider->Empty ||
        (provider->payloadKind!=XL6_FUTURE_PAYLOAD_XVALUE&&provider->payloadKind!=XL6_FUTURE_PAYLOAD_VALUE)){
        xrtSetErrorKind(XERR_ARGUMENT);return NULL;
    }
    Frame* frame=xrtCalloc(1,sizeof(*frame)+provider->stateSize);if(!frame)return NULL;
    frame->references=1;frame->provider=provider;return frame;
}
void* xl6ChannelFutureCoreStateV1(const Frame* frame) {return frame?(void*)frame->state:NULL;}
const XL6_ChannelFutureProviderV1* xl6ChannelFutureCoreProviderV1(const Frame* frame)
{return frame?frame->provider:NULL;}
bool xl6ChannelFutureCoreWakeTakeV1(Frame* frame,const XL6_ChannelFutureWakeV1* wake,void** input)
{
    if(!frame||!wake||wake->size!=sizeof(*wake)||!wake->Arm||!wake->Detach||!wake->RecordFailure||
        !wake->TakeFailure||!wake->Cleanup||!wake->Trace||!wake->Empty||!wake->Drop||!input||!*input||
        frame->wake||frame->committed||frame->publishing||frame->bound||!frame->subscription){
        xrtSetErrorKind(XERR_ARGUMENT);return false;
    }
    frame->wake=wake;frame->wakeData=*input;*input=NULL;return true;
}
const XL6_ChannelFutureWakeV1* xl6ChannelFutureCoreWakePolicyV1(const Frame* frame)
{return frame?frame->wake:NULL;}
void* xl6ChannelFutureCoreWakeDataV1(const Frame* frame) {return frame?frame->wakeData:NULL;}
xchannelsubscription* xl6ChannelFutureCoreSubscriptionRefV1(Frame* frame)
{
    if(!frame){xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    xrtownershipscope mutation={0};enter(frame,&mutation);
    xchannelsubscription* result=frame->subscription?xrtChannelSubscribeRef(frame->subscription):NULL;
    leave(frame,&mutation);if(!result&&!xrtGetError())xrtSetErrorKind(XERR_STATE);return result;
}
void xl6ChannelFutureCoreStopV1(Frame* frame,xchannelsubscription* subscription,xwaitresult reason,
    bool failed,const xerror* error)
{
    if(!frame||!subscription||(reason!=XWAIT_TIMEOUT&&reason!=XWAIT_CANCELLED&&reason!=XWAIT_ERROR))abort();
    xerror* previous=xrtTakeError();xrtownershipscope mutation={0};enter(frame,&mutation);++frame->active;
    bool pending=frame->bound&&frame->subscription==subscription;
    if(pending&&failed){if(!frame->wake)abort();frame->wake->RecordFailure(frame->wakeData,error);}
    leave(frame,&mutation);
    if(pending)(void)xrtChannelSubscribeStop(subscription,reason);
    enter(frame,&mutation);--frame->active;leave(frame,&mutation);
    xrtClearError();xrtSetErrorTake(previous);
}
bool xl6ChannelFutureCoreHandleTakeV1(Frame* frame,xvalue** handle)
{
    if(!frame || !handle || !*handle || frame->handle || frame->subscription || frame->committed ||
        frame->bound || frame->publishing || frame->provider->payloadKind!=XL6_FUTURE_PAYLOAD_XVALUE){
        xrtSetErrorKind(XERR_STATE);return false;
    }
    frame->handle=*handle;*handle=NULL;return true;
}
bool xl6ChannelFutureCoreSubscribeV1(Frame* frame,const xchannelsubscribecase* native,size_t count)
{
    if(!frame || frame->subscription || frame->committed || frame->bound || frame->publishing){
        xrtSetErrorKind(XERR_STATE);return false;
    }
    if(!hold(frame))abort();
    frame->subscription=xrtChannelSubscribeCreateOwnedV1(native,count,frame,frame->provider->subscription);
    if(!frame->subscription){release(frame);return false;}return true;
}
const xvalue* xl6ChannelFutureCoreResultHandleV1(const Frame* frame)
{return frame&&!frame->bound&&!frame->committed&&!frame->publishing?frame->handle:NULL;}
static void fail(xpromise* promise,xerror* error)
{
    if(!error){xrtSetErrorKind(XERR_INTERNAL);error=xrtTakeError();}
    (void)xrtPromiseReject(promise,error);xrtErrorFree(error);xrtClearError();
}
static void run(void* data)
{
    Frame* frame=data;xerror* previous=xrtTakeError();xrtownershipscope mutation={0};
    enter(frame,&mutation);++frame->active;leave(frame,&mutation);
    xl6_error_scope scope={0};if(!xl6ErrorScopeBegin(&scope))abort();
    int64_t scalar=0;
    bool ok=!frame->wake||frame->wake->TakeFailure(frame->wakeData,frame->nativeResult);
    if(ok)ok=frame->provider->Complete(frame->state,frame->nativeResult,&scalar);
    if(ok&&frame->provider->payloadKind==XL6_FUTURE_PAYLOAD_VALUE)*(int64_t*)frame->payload=scalar;
    if(!ok)xl6ErrorScopeCaptureFailure(&scope);
    cleanup_values(frame,&scope);
    if(scope.failed){void* payload=frame->payload;frame->payload=NULL;
        xl6PayloadDropGet(frame->provider->payloadKind)(payload,NULL);capture_failure(&scope);}
    xl6_error_result result=xl6ErrorScopeEnd(&scope);
    if(!xl6ContinuationComplete(&frame->ticket))abort();
    if(!result.ok)fail(frame->promise,result.error);
    else {
        xrtErrorFree(result.error);void* payload=frame->payload;frame->payload=NULL;
        if(!xl6PayloadResolveOwned(frame->promise,payload,frame->provider->payloadKind)){
            xerror* error=xrtTakeError();xl6PayloadDropGet(frame->provider->payloadKind)(payload,NULL);
            xrtClearError();fail(frame->promise,error);
        }
    }
    enter(frame,&mutation);--frame->active;leave(frame,&mutation);
    xrtClearError();xrtSetErrorTake(previous);
}
static void retire(void* data)
{
    Frame* frame=data;xerror* previous=xrtTakeError();xrtownershipscope mutation={0};
    enter(frame,&mutation);frame->retiring=true;++frame->active;
    xpromise* promise=frame->promise;frame->promise=NULL;leave(frame,&mutation);
    xrtPromiseDestroy(promise);
    enter(frame,&mutation);frame->retired=true;frame->retiring=false;--frame->active;leave(frame,&mutation);
    xrtClearError();xrtSetErrorTake(previous);
}
static xfuture* start(XL6_ContinuationExecutor* executor,Frame* frame,const xvalue* owner,
    const char* field,size_t fieldSize,xcancel* parent,const void* wakeContext)
{
    if(!executor||!frame||((field==NULL)!=(fieldSize==0))){
        xrtSetErrorKind(XERR_ARGUMENT);return NULL;
    }
    xrtownershipscope mutation={0};enter(frame,&mutation);
    if(frame->claim||frame->retired||frame->active||frame->retiring||frame->committed||frame->publishing||
        frame->bound||!frame->subscription||frame->payload||frame->promise||
        ((frame->wake==NULL)!=(wakeContext==NULL))){
        leave(frame,&mutation);xrtSetErrorKind(XERR_STATE);return NULL;
    }
    /* Admission and the inspection barrier are one mutation, before argument
     * projection or fallible allocation. Claim cannot slip between them. */
    ++frame->active;leave(frame,&mutation);
    xfuture* future=NULL;
    bool boxed=frame->provider->payloadKind==XL6_FUTURE_PAYLOAD_XVALUE;
    if(boxed){
        const xvalue* handle=owner?(field?xrtValueObjectGet(owner,(xstrview){field,fieldSize}):owner):NULL;
        if(!frame->handle || handle!=frame->handle){xrtSetErrorKind(XERR_TYPE);goto failed;}
    }else if(owner||field||fieldSize||frame->handle){xrtSetErrorKind(XERR_ARGUMENT);goto failed;}
    frame->payload=xrtCalloc(1,boxed?sizeof(xvalue*):sizeof(int64_t));if(!frame->payload)goto failed;
    if(boxed){
        *(xvalue**)frame->payload=xrtValueRetain(owner);if(!*(xvalue**)frame->payload)goto failed;
        xvalue* initial=frame->handle;frame->handle=NULL;xrtValueRelease(initial);
    }
    frame->promise=xrtPromiseCreate(&future,parent);if(!frame->promise)goto failed;
    if(!hold(frame))abort();
    if(!xrtPromiseProducerBindTakeV1(frame->promise,xl6ChannelFutureCoreOwnershipV1(frame),&producerPolicy)){release(frame);goto failed;}
    frame->cancel=xrtPromiseCancelToken(frame->promise);if(!frame->cancel)goto failed;
    frame->cancelSubscription=xrtChannelSubscribeRef(frame->subscription);if(!frame->cancelSubscription)goto failed;
    if(!hold(frame))abort();
    frame->cancelWatch=xrtCancelWatchOwnedV1(frame->cancel,frame,frame->provider->cancellation);
    if(!frame->cancelWatch){
        release(frame);xchannelsubscription* subscription=frame->cancelSubscription;frame->cancelSubscription=NULL;
        xrtChannelSubscribeDestroy(subscription);goto failed;
    }
    XL6_ContinuationAdmission admission=xl6ContinuationExecutorReserve(executor,&frame->ticket,run,retire,frame);
    if(admission!=XL6_CONTINUATION_ADMITTED){
        if(!xrtGetError())xrtSetErrorKind(admission==XL6_CONTINUATION_FULL?XERR_RANGE:
            admission==XL6_CONTINUATION_RESOURCE_FAILURE?XERR_MEMORY:XERR_STATE);goto failed;
    }
    enter(frame,&mutation);
    frame->ticket.storage=&storage;frame->publishing=true;frame->bound=true;leave(frame,&mutation);
    if(frame->wake&&!frame->wake->Arm(frame->wakeData,wakeContext)){
        if(!xl6ContinuationAbort(&frame->ticket))abort();
        enter(frame,&mutation);frame->publishing=false;frame->bound=false;leave(frame,&mutation);goto failed;
    }
    if(!xrtChannelSubscribeStart(frame->subscription)){
        if(!xl6ContinuationAbort(&frame->ticket))abort();
        enter(frame,&mutation);frame->publishing=false;frame->bound=false;leave(frame,&mutation);goto failed;
    }
    enter(frame,&mutation);--frame->active;leave(frame,&mutation);
    xl6ContinuationCommit(&frame->ticket); /* final transfer; no frame access */
    return future;
failed:;
    xerror* error=xrtTakeError();xrtFutureDestroy(future);xrtClearError();
    enter(frame,&mutation);--frame->active;leave(frame,&mutation);
    if(error)xrtSetErrorTake(error);else xrtSetErrorKind(XERR_INTERNAL);return NULL;
}
xfuture* xl6ChannelFutureCoreStartV1(XL6_ContinuationExecutor* executor,Frame* frame,const xvalue* owner,
    const char* field,size_t size,xcancel* parent)
{return start(executor,frame,owner,field,size,parent,NULL);}
xfuture* xl6ChannelFutureCoreStartWakeV1(XL6_ContinuationExecutor* executor,Frame* frame,const xvalue* owner,
    const char* field,size_t size,xcancel* parent,const void* context)
{
    if(!frame||!frame->wake||!context){xrtSetErrorKind(XERR_TYPE);return NULL;}
    return start(executor,frame,owner,field,size,parent,context);
}
bool xl6ChannelFutureCoreCountV1(const void* data,const XL6_ChannelFutureProviderV1* expected,size_t* count)
{
    const Frame* frame=data;
    if(!frame||!expected||frame->provider!=expected||!count||!frame->references||frame->publishing||frame->active||frame->retiring||frame->cleared)return false;
    /* Only explicitly published stable builders gain a private rollback
     * preparation. Raw half-built frames remain BUSY, not graph-admitted. */
    if(!frame->committed&&(!frame->builderPublished||frame->bound||frame->ticket.queue))return false;
    if(frame->ticket.queue&&(frame->ticket.ownershipRefs!=1||frame->ticket.transportOwned||
        frame->ticket.predecessor||frame->ticket.successor||
        (frame->ticket.flags!=XL6_CONTINUATION_COMMITTED&&frame->ticket.flags!=
            (XL6_CONTINUATION_COMMITTED|XL6_CONTINUATION_READY|XL6_CONTINUATION_TRANSPORT_RELEASED))))return false;
    *count=frame->references;return true;
}
bool xl6ChannelFutureCoreBusyV1(const void* data,const XL6_ChannelFutureProviderV1* expected)
{
    const Frame* frame=data;size_t count;
    return frame&&expected&&frame->provider==expected&&frame->references&&!frame->cleared&&
        !xl6ChannelFutureCoreCountV1(data,expected,&count);
}
bool xl6ChannelFutureCoreTraceV1(const void* data,const XL6_ChannelFutureProviderV1* expected,xrtownershipvisitor visit,void* context)
{
    const Frame* frame=data;size_t count;if(!visit||!xl6ChannelFutureCoreCountV1(data,expected,&count))return false;
    return visit(xrtChannelSubscribeOwnership(frame->subscription),context)&&
        visit(xrtChannelSubscribeOwnership(frame->cancelSubscription),context)&&
        visit(xrtCancelWatchOwnership(frame->cancelWatch),context)&&visit(xrtCancelOwnership(frame->cancel),context)&&
        visit(xrtPromiseOwnership(frame->promise),context)&&visit(xrtValueOwnership(frame->handle),context)&&
        visit(xrtValueOwnership(frame->payload&&expected->payloadKind==XL6_FUTURE_PAYLOAD_XVALUE?*(xvalue**)frame->payload:NULL),context)&&
        expected->Trace(frame->state,visit,context)&&(!frame->wake||frame->wake->Trace(frame->wakeData,visit,context));
}
static bool claim(const void* data,const void* token)
{
    Frame* frame=(Frame*)data;
    /* Accepted work must finish before ANY of its collectible descendants
     * stay claimed across Freeze. Otherwise the worker cannot reactivate a
     * parked Channel operation while the module plan waits for that worker. */
    if(!token||frame->cleared||frame->active||frame->publishing||frame->retiring||
        (frame->committed&&!frame->retired)||(frame->claim&&frame->claim!=token))return false;
    frame->claim=token;return true;
}
static void restore(const void* data,const void* token)
{Frame* frame=(Frame*)data;if(!token||frame->claim!=token||frame->cleared)abort();frame->claim=NULL;}
static bool prepared(const void* data)
{
    const Frame* frame=data;
    return frame->retired&&!frame->active&&!frame->ticket.queue&&!frame->subscription&&
        !frame->cancelSubscription&&!frame->cancelWatch&&!frame->cancel&&!frame->promise&&!frame->handle&&!frame->payload&&
        frame->provider->Empty(frame->state)&&(!frame->wake||frame->wake->Empty(frame->wakeData));
}
static xrtownershipprepareresult prepare_drop(const void* data,const void* token)
{
    Frame* frame=(Frame*)data;xrtownershipscope mutation={0};enter(frame,&mutation);
    if(!token||frame->claim!=token||frame->cleared)abort();
    bool ready=prepared(frame);
    bool rollback=!ready&&frame->builderPublished&&!frame->committed&&!frame->publishing&&
        !frame->bound&&!frame->ticket.queue&&!frame->active&&!frame->retiring;
    leave(frame,&mutation);
    /* Preparation runs outside Freeze and only for a claimed private builder.
     * It consumes semantic duties, not the language handle's creator credit. */
    if(rollback){
        if(!discard_contents(frame))return XRT_OWNERSHIP_PREPARE_FAILED;
        ready=prepared(frame);
    }
    return ready?XRT_OWNERSHIP_PREPARE_READY:XRT_OWNERSHIP_PREPARE_BUSY;
}
static void clear(const void* data,const void* token)
{Frame* frame=(Frame*)data;if(!token||frame->claim!=token||!prepared(frame))abort();frame->cleared=true;}
const xrtownershipadapterv1* xl6ChannelFutureCoreAdapterV1(xrtownershipref ref,
    const XL6_ChannelFutureProviderV1* expected,const xrtownershippreparationv1** preparation)
{
    static const xrtownershipadapterv1 adapter={sizeof(adapter),hold,release,claim,restore,NULL,clear,NULL};
    static const xrtownershippreparationv1 plan={sizeof(plan),&adapter,prepared,prepare_drop};size_t count;
    if(!expected||ref.Ops!=expected->ops||!preparation||!xl6ChannelFutureCoreCountV1(ref.Data,expected,&count))return NULL;
    *preparation=&plan;return &adapter;
}
void xl6ChannelFutureBuilderDestroyV1(void* data)
{
    Frame* frame=data;if(!frame)return;
    if(frame->committed)release(frame);else xl6ChannelFutureCoreDiscardV1(frame);
}
xvalue* xl6ChannelFutureBuilderResultV1(void* data)
{
    const xvalue* handle=xl6ChannelFutureCoreResultHandleV1(data);
    if(!handle)xrtSetErrorKind(XERR_STATE);
    xvalue* result=handle?xrtValueRetain(handle):NULL;
    if(!result)xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});
    return result;
}
xfuture* xl6ChannelFutureBuilderStartV1(XL6_ContinuationExecutor* executor,void* data,
    const xvalue* owner,const char* field,size_t size,xcancel* parent)
{
    Frame* frame=data;
    if(!frame || frame->committed || frame->publishing || frame->bound){xrtSetErrorKind(XERR_STATE);return NULL;}
    if(!hold(frame)){xrtSetErrorKind(XERR_STATE);return NULL;}
    xfuture* result=xl6ChannelFutureCoreStartV1(executor,frame,owner,field,size,parent);
    if(!result)release(frame);
    return result;
}
static bool builder_trace(const xvalue* value,xrtownershipvisitor visit,void* context)
{
    void* data=NULL;const xvaluehandleops* ops=NULL;void* user=NULL;
    return visit && xrtValueGetHandle(value,&data,&ops,&user) && data && !user &&
        ops==xl6ChannelFutureBuilderHandlePolicyV1Get()->Ops && visit(xl6ChannelFutureCoreOwnershipV1(data),context);
}
XL6_DEFINE_NATIVE_HANDLE_POLICY(xl6ChannelFutureBuilderHandlePolicyV1Get,xl6ChannelFutureBuilderDestroyV1,builder_trace)
