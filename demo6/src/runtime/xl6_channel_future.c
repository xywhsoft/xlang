#include "xl6_text_value.h"
#include "xl6_channel_future.h"
#include "xl6_channel_case_internal.h"
#include "xl6_channel_internal.h"
#include "xl6_future_payload.h"
#include "src/api/xl_foreign_error.h"
#include <string.h>

typedef struct XL6_ChannelFutureFrameV1 Frame;
struct XL6_ChannelFutureFrameV1 {
    xatomic64 gate;
    size_t references,active;
    const void* claim;
    bool committed,publishing,bound,retiring,retired,cleared;
    XL6_ContinuationTicket ticket;
    XL6_ChannelOwnedSelectFrameV1 owned;
    XL6_ChannelSelectFrameV1 operations;
    XL6_ChannelSelectionV1 selected;
    xchannelselectresult nativeResult;
    xchannelsubscription *subscription,*cancelSubscription;
    xcancelwatch* cancelWatch;
    xcancel* cancel;
    xpromise* promise;
    xvalue* handle;
    xvalue** payload;
};
static const xrtownershipops frameOps;
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
        frame->cancelWatch||frame->cancel||frame->promise||frame->handle||frame->payload||frame->ticket.queue))abort();
    leave(frame,&mutation);if(last)xrtFree(frame);
}
static xrtownershipref ownership(const Frame* frame)
{return (xrtownershipref){frame,frame?&frameOps:NULL};}
static xrtownershipref storage_ownership(const void* data) {return ownership(data);}
static void commit(const void* data)
{
    Frame* frame=(Frame*)data;xrtownershipscope mutation={0};enter(frame,&mutation);
    if(frame->committed||!frame->publishing||!frame->bound||!frame->references)abort();
    frame->committed=true;frame->publishing=false;leave(frame,&mutation);
}
static const XL6_ContinuationStorageV2 storage={hold,release,storage_ownership,NULL,commit,NULL,NULL,NULL};
const XL6_ContinuationStorageV2* xl6ChannelFutureStorageV2Get(void) {return &storage;}
static void data_drop(const void* data)
{
    /* Inspection pins may preserve the native subscription long after its
     * operation completed. This returns only its physical Data reference;
     * logical delivery must NOT wait for physical object destruction. */
    release(data);
}
static void notify(xchannelsubscription* subscription,void* data)
{
    Frame* frame=data;xerror* previous=xrtTakeError();void* received=NULL;
    xrtownershipscope mutation={0};enter(frame,&mutation);++frame->active;leave(frame,&mutation);
    xchannelselectresult result={0};if(!xrtChannelSubscribeResult(subscription,&result,&received))abort();
    enter(frame,&mutation);
    if(!frame->bound||frame->subscription!=subscription)abort();
    frame->nativeResult=result;
    if(result.Wait==XWAIT_OK&&result.Result==XCHANNEL_OK){
        if(result.Index>=frame->operations.count)abort();
        if(!frame->operations.operations[result.Index].sending)
            frame->operations.operations[result.Index].received=received;
    }
    xchannelsubscription* owner=frame->subscription;frame->subscription=NULL;
    xcancelwatch* watch=frame->cancelWatch;frame->cancelWatch=NULL;
    leave(frame,&mutation);
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
static const xchannelsubscribeownershipv1 subscriptionPolicy={sizeof(subscriptionPolicy),notify,data_drop,&frameOps};
const xchannelsubscribeownershipv1* xl6ChannelFutureSubscriptionPolicyV1Get(void) {return &subscriptionPolicy;}
static const xfutureproducerownershipv1 producerPolicy={sizeof(producerPolicy),release};
const xfutureproducerownershipv1* xl6ChannelFutureProducerPolicyV1Get(void) {return &producerPolicy;}
static void cancel_request(void* data)
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
static void cancel_drop(const void* data)
{
    Frame* frame=(Frame*)data;xrtownershipscope mutation={0};enter(frame,&mutation);
    xchannelsubscription* subscription=frame->cancelSubscription;frame->cancelSubscription=NULL;
    leave(frame,&mutation);xrtChannelSubscribeDestroy(subscription);release(frame);
}
static const xcancelwatchownershipv1 cancelPolicy={sizeof(cancelPolicy),cancel_request,cancel_drop,&frameOps};
const xcancelwatchownershipv1* xl6ChannelFutureCancelPolicyV1Get(void) {return &cancelPolicy;}
static void capture_failure(xl6_error_scope* scope)
{if(xrtGetError()||scope->failed)xl6ErrorScopeCaptureFailure(scope);}
static void cleanup_values(Frame* frame,xl6_error_scope* scope)
{
    xl6ChannelSelectDiscardV1(&frame->operations);capture_failure(scope);
    (void)xl6ChannelOwnedSelectDiscardV1(&frame->owned,false);capture_failure(scope);
    xvalue* handle=frame->handle;frame->handle=NULL;xrtValueRelease(handle);capture_failure(scope);
    xcancel* cancel=frame->cancel;frame->cancel=NULL;xrtCancelDestroy(cancel);capture_failure(scope);
}
static void discard(Frame* frame)
{
    if(!frame)return;
    if(frame->committed||frame->publishing||frame->bound||frame->ticket.queue)abort();
    xl6_error_scope scope={0};if(!xl6ErrorScopeBeginCleanup(&scope,false))abort();
    xcancelwatch* watch=frame->cancelWatch;frame->cancelWatch=NULL;xrtCancelUnwatch(watch);capture_failure(&scope);
    xchannelsubscription* subscription=frame->subscription;frame->subscription=NULL;
    xrtChannelSubscribeDestroy(subscription);capture_failure(&scope);
    cleanup_values(frame,&scope);
    void* payload=frame->payload;frame->payload=NULL;
    xl6PayloadDropGet(XL6_FUTURE_PAYLOAD_XVALUE)(payload,NULL);capture_failure(&scope);
    xpromise* promise=frame->promise;frame->promise=NULL;xrtPromiseDestroy(promise);capture_failure(&scope);
    xrtownershipscope mutation={0};enter(frame,&mutation);frame->retired=true;leave(frame,&mutation);
    xl6_error_result result=xl6ErrorScopeEnd(&scope);release(frame);
    if(!result.ok)xl6ErrorScopePropagateFailure(result);else xrtErrorFree(result.error);
}
static Frame* prepare(XL6_ChannelCaseOwnerV1* const* cases,size_t count)
{
    xl6_error_scope scope={0};if(!xl6ErrorScopeBegin(&scope))return NULL;
    Frame* frame=xrtCalloc(1,sizeof(*frame));xchannelsubscribecase local[8],*native=local;bool ok=false;
    if(!frame)goto done;
    frame->references=1;
    XL6_ChannelWaitOptionsV2 options={sizeof(options),XL6_CHANNEL_WAIT_V1,0,0};
    if(!xl6ChannelOwnedSelectPrepareV1(cases,count,&options,&frame->owned))goto done;
    frame->handle=xl6ChannelOwnedSelectResultHandleV1(&frame->owned);if(!frame->handle)goto done;
    if(!xl6ChannelSelectPrepareV1(frame->owned.cases,count,&frame->selected,&frame->operations))goto done;
    if(!xl6ChannelOwnedSelectParkV1(&frame->owned)||!xl6ChannelSelectParkV1(&frame->operations))goto done;
    if(count>SIZE_MAX/sizeof(*native)){xrtSetErrorKind(XERR_RANGE);goto done;}
    if(count>8){native=xrtCalloc(count,sizeof(*native));if(!native)goto done;}
    for(size_t i=0;i<count;++i){XL6_ChannelOperationV1* item=frame->operations.operations+i;
        native[i]=(xchannelsubscribecase){item->native,item->sending?XCHANNEL_OP_SEND:XCHANNEL_OP_RECV,item->message};
    }
    if(!hold(frame))abort();
    frame->subscription=xrtChannelSubscribeCreateOwnedV1(native,count,frame,&subscriptionPolicy);
    if(!frame->subscription){release(frame);goto done;}ok=true;
done:
    if(native!=local)xrtFree(native);
    if(!ok)xl6ErrorScopeCaptureFailure(&scope);
    if(!ok){discard(frame);frame=NULL;capture_failure(&scope);}
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    if(!outcome.ok){xl6ErrorScopePropagateFailure(outcome);return NULL;}
    xrtErrorFree(outcome.error);return frame;
}
static const xvalue* result_handle(const Frame* frame)
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
    bool ok=xl6ChannelSelectCompleteV1(&frame->operations,frame->nativeResult,&frame->selected);
    if(ok)ok=xl6ChannelOwnedSelectCompleteV1(&frame->owned,frame->selected);
    if(!ok)xl6ErrorScopeCaptureFailure(&scope);
    cleanup_values(frame,&scope);
    if(scope.failed){void* payload=frame->payload;frame->payload=NULL;
        xl6PayloadDropGet(XL6_FUTURE_PAYLOAD_XVALUE)(payload,NULL);capture_failure(&scope);}
    xl6_error_result result=xl6ErrorScopeEnd(&scope);
    if(!xl6ContinuationComplete(&frame->ticket))abort();
    if(!result.ok)fail(frame->promise,result.error);
    else {
        xrtErrorFree(result.error);xvalue** payload=frame->payload;frame->payload=NULL;
        if(!xl6PayloadResolveOwned(frame->promise,payload,XL6_FUTURE_PAYLOAD_XVALUE)){
            xerror* error=xrtTakeError();xl6PayloadDropGet(XL6_FUTURE_PAYLOAD_XVALUE)(payload,NULL);
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
    const char* field,size_t fieldSize,xcancel* parent)
{
    if(!executor||!frame||frame->committed||frame->publishing||frame->bound||!frame->subscription||
        !frame->handle||frame->payload||frame->promise||!owner||((field==NULL)!=(fieldSize==0))){
        xrtSetErrorKind(XERR_ARGUMENT);return NULL;
    }
    const xvalue* handle=field?xrtValueObjectGet(owner,(xstrview){field,fieldSize}):owner;
    if(handle!=frame->handle){xrtSetErrorKind(XERR_TYPE);return NULL;}
    xfuture* future=NULL;frame->payload=xrtCalloc(1,sizeof(*frame->payload));if(!frame->payload)return NULL;
    *frame->payload=xrtValueRetain(owner);if(!*frame->payload)goto failed;
    xvalue* initial=frame->handle;frame->handle=NULL;xrtValueRelease(initial);
    frame->promise=xrtPromiseCreate(&future,parent);if(!frame->promise)goto failed;
    if(!hold(frame))abort();
    if(!xrtPromiseProducerBindTakeV1(frame->promise,ownership(frame),&producerPolicy)){release(frame);goto failed;}
    frame->cancel=xrtPromiseCancelToken(frame->promise);if(!frame->cancel)goto failed;
    frame->cancelSubscription=xrtChannelSubscribeRef(frame->subscription);if(!frame->cancelSubscription)goto failed;
    if(!hold(frame))abort();
    frame->cancelWatch=xrtCancelWatchOwnedV1(frame->cancel,frame,&cancelPolicy);
    if(!frame->cancelWatch){
        release(frame);xchannelsubscription* subscription=frame->cancelSubscription;frame->cancelSubscription=NULL;
        xrtChannelSubscribeDestroy(subscription);goto failed;
    }
    XL6_ContinuationAdmission admission=xl6ContinuationExecutorReserve(executor,&frame->ticket,run,retire,frame);
    if(admission!=XL6_CONTINUATION_ADMITTED){
        if(!xrtGetError())xrtSetErrorKind(admission==XL6_CONTINUATION_FULL?XERR_RANGE:
            admission==XL6_CONTINUATION_RESOURCE_FAILURE?XERR_MEMORY:XERR_STATE);goto failed;
    }
    xrtownershipscope mutation={0};enter(frame,&mutation);
    frame->ticket.storage=&storage;frame->publishing=true;frame->bound=true;leave(frame,&mutation);
    if(!xrtChannelSubscribeStart(frame->subscription)){
        if(!xl6ContinuationAbort(&frame->ticket))abort();
        enter(frame,&mutation);frame->publishing=false;frame->bound=false;leave(frame,&mutation);goto failed;
    }
    xl6ContinuationCommit(&frame->ticket); /* final transfer; no frame access */
    return future;
failed:;
    xerror* error=xrtTakeError();xrtFutureDestroy(future);xrtClearError();
    if(error)xrtSetErrorTake(error);else xrtSetErrorKind(XERR_INTERNAL);return NULL;
}
static bool count_frame(const void* data,size_t* count)
{
    const Frame* frame=data;
    if(!frame||!count||!frame->references||!frame->committed||frame->publishing||frame->active||frame->retiring||frame->cleared)return false;
    if(frame->ticket.queue&&(frame->ticket.ownershipRefs!=1||frame->ticket.transportOwned||
        frame->ticket.predecessor||frame->ticket.successor||
        (frame->ticket.flags!=XL6_CONTINUATION_COMMITTED&&frame->ticket.flags!=
            (XL6_CONTINUATION_COMMITTED|XL6_CONTINUATION_READY|XL6_CONTINUATION_TRANSPORT_RELEASED))))return false;
    *count=frame->references;return true;
}
static bool trace_frame(const void* data,xrtownershipvisitor visit,void* context)
{
    const Frame* frame=data;size_t count;if(!visit||!count_frame(data,&count))return false;
    return visit(xrtChannelSubscribeOwnership(frame->subscription),context)&&
        visit(xrtChannelSubscribeOwnership(frame->cancelSubscription),context)&&
        visit(xrtCancelWatchOwnership(frame->cancelWatch),context)&&visit(xrtCancelOwnership(frame->cancel),context)&&
        visit(xrtPromiseOwnership(frame->promise),context)&&visit(xrtValueOwnership(frame->handle),context)&&
        visit(xrtValueOwnership(frame->payload?*frame->payload:NULL),context)&&
        xl6ChannelOwnedSelectTraceV1(&frame->owned,visit,context)&&xl6ChannelSelectTraceV1(&frame->operations,visit,context);
}
static const xrtownershipops frameOps={count_frame,trace_frame};
static bool claim(const void* data,const void* token)
{Frame* frame=(Frame*)data;if(!token||frame->cleared||(frame->claim&&frame->claim!=token))return false;frame->claim=token;return true;}
static void restore(const void* data,const void* token)
{Frame* frame=(Frame*)data;if(!token||frame->claim!=token||frame->cleared)abort();frame->claim=NULL;}
static bool prepared(const void* data)
{
    const Frame* frame=data;
    return frame->retired&&!frame->active&&!frame->ticket.queue&&!frame->subscription&&
        !frame->cancelSubscription&&!frame->cancelWatch&&!frame->cancel&&!frame->promise&&!frame->handle&&!frame->payload;
}
static xrtownershipprepareresult prepare_drop(const void* data,const void* token)
{
    Frame* frame=(Frame*)data;xrtownershipscope mutation={0};enter(frame,&mutation);
    if(!token||frame->claim!=token||frame->cleared)abort();
    bool ready=prepared(frame);
    leave(frame,&mutation);return ready?XRT_OWNERSHIP_PREPARE_READY:XRT_OWNERSHIP_PREPARE_BUSY;
}
static void clear(const void* data,const void* token)
{Frame* frame=(Frame*)data;if(!token||frame->claim!=token||!prepared(frame))abort();frame->cleared=true;}
const xrtownershipadapterv1* xl6ChannelFutureOwnershipAdapterV1(xrtownershipref ref,
    const xrtownershippreparationv1** preparation)
{
    static const xrtownershipadapterv1 adapter={sizeof(adapter),hold,release,claim,restore,NULL,clear,NULL};
    static const xrtownershippreparationv1 plan={sizeof(plan),&adapter,prepared,prepare_drop};size_t count;
    if(ref.Ops!=&frameOps||!preparation||!count_frame(ref.Data,&count))return NULL;
    *preparation=&plan;return &adapter;
}
const XL6_ChannelFutureRuntimeV1* xl6ChannelFutureRuntimeV1Get(void)
{
    static const XL6_ChannelFutureRuntimeV1 api={prepare,result_handle,start,discard,ownership};return &api;
}
