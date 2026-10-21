#define XRT_MODULE_FUTURE
#define XRT_MODULE_MEMORY
#define XRT_MODULE_ATOMIC
#define XRT_MODULE_THREAD
#include "lib/xrt/xrt.h"
#include "xl6_future_callback.h"
#include "xl6_future_callback_internal.h"
#include "xl6_future_wait.h"
#include "xl6_ownership_scope.h"
#include "src/api/xl_foreign_error.h"

void xl6FutureCallbackFailV1(xpromise* output,xerror* error)
{
    xerror* previous=xrtTakeError();
    if(!error){xrtSetErrorKind(XERR_INTERNAL);error=xrtTakeError();}
    (void)xrtPromiseReject(output,error);xrtErrorFree(error);
    xrtClearError();xrtSetErrorTake(previous);
}

typedef XL6_FutureCallbackFrameV1 FutureCallback;
static void mutation_begin(xrtownershipscope* scope) { if(!xrtOwnershipMutationBegin(scope))abort(); }
static void mutation_end(xrtownershipscope* scope) { if(!xrtOwnershipScopeEnd(scope))abort(); }
static void frame_begin(FutureCallback* frame,xrtownershipscope* scope)
{
    mutation_begin(scope);
    for(;;){uint64 expected=0;
        if(xrtAtomic64CompareExchange(&frame->gate,&expected,1,XMEMORY_ACQUIRE,XMEMORY_RELAXED))return;
        xrtThreadYield();
    }
}
static void frame_unlock(FutureCallback* frame) { xrtAtomic64Store(&frame->gate,0,XMEMORY_RELEASE); }
static void frame_end(FutureCallback* frame,xrtownershipscope* scope) { frame_unlock(frame);mutation_end(scope); }
void xl6CallbackFrameBeginV1(FutureCallback* frame,xrtownershipscope* scope) {frame_begin(frame,scope);}
void xl6CallbackFrameEndV1(FutureCallback* frame,xrtownershipscope* scope) {frame_end(frame,scope);}
static FutureCallback* header(const void* data)
{ return data?(FutureCallback*)((unsigned char*)data - offsetof(FutureCallback,data)):NULL; }
static bool frame_count(const void* data,size_t* count)
{
    const FutureCallback* frame=data;
    if(!frame || !count || !frame->references || !frame->committed || frame->publishing || frame->active || frame->retiring || frame->cleared)return false;
    if(frame->queued && frame->payloadLive){
        const XL6_ContinuationTicket* ticket=frame->ticket;
        if(!ticket || !ticket->queue || ticket->predecessor || ticket->successor ||
            (ticket->flags!=XL6_CONTINUATION_COMMITTED && ticket->flags!=
                (XL6_CONTINUATION_COMMITTED|XL6_CONTINUATION_READY|XL6_CONTINUATION_TRANSPORT_RELEASED)))return false;
        size_t expected=1+(ticket->transportOwned&&!(ticket->flags&XL6_CONTINUATION_TRANSPORT_RELEASED)?1u:0u);
        if(ticket->ownershipRefs!=expected || frame->references<expected)return false;
    }
    if(frame->lease){xrtownershipref lease=xl6CodeLeaseOwnership(frame->lease);size_t references;
        if(!lease.Ops->Count(lease.Data,&references))return false;}
    *count=frame->references;return true;
}
static bool frame_trace(const void* data,xrtownershipvisitor visit,void* context)
{
    const FutureCallback* frame=data;size_t count;
    if(!visit || !frame_count(frame,&count))return false;
    return (!frame->lease || visit(xl6CodeLeaseOwnership(frame->lease),context)) &&
        visit(xrtFutureOwnership(frame->source),context) && visit(xrtPromiseOwnership(frame->output),context) &&
        visit(xrtCancelOwnership(frame->cancel),context) &&
        visit(xrtCancelWatchOwnership(frame->cancelSource),context) &&
        (!frame->interruptOps || frame->interruptOps->Trace(frame,visit,context)) && (!frame->payloadLive ||
            (frame->layoutV2 ? xl6CaptureRuntimeV2Get()->Trace(frame->layoutV2,frame->data,frame->dataSize,visit,context) :
             frame->layout ? xl6CaptureRuntimeV1Get()->Trace(frame->layout,frame->data,visit,context)
                           : frame->trace(frame->data,visit,context)));
}
const xrtownershipops xl6FutureCallbackFrameOpsV1={frame_count,frame_trace};
static bool frame_has_layout(const FutureCallback* frame)
{ return frame->layout || frame->layoutV2; }
static bool frame_layout_valid(const FutureCallback* frame)
{
    return frame->layoutV2 ? !frame->layout && xl6CaptureRuntimeV2Get()->Validate(frame->layoutV2,frame->dataSize) :
        frame->layout && xl6CaptureRuntimeV1Get()->Validate(frame->layout,frame->dataSize);
}
bool xl6CallbackFutureWaitV1(xrtownershipref ref,const void* token,XL6_FutureWaitV1* wait)
{
    const FutureCallback* frame=ref.Data;size_t count;
    if(ref.Ops!=&xl6FutureCallbackFrameOpsV1 || !frame || !token || !wait || frame->claim!=token ||
        !frame_layout_valid(frame) ||
        !frame_count(frame,&count) || !frame->payloadLive || !frame->source || !frame->output || !frame->cancel)return false;
    *wait=(XL6_FutureWaitV1){frame->output,frame->source};return true;
}
static xrtownershipref ownership(const void* data) { return (xrtownershipref){header(data),data?&xl6FutureCallbackFrameOpsV1:NULL}; }
static void* allocate_body(const XL6_CodeOwner* owner,size_t size,XL6_FutureCallbackProc proc,
    XL6_FutureCallbackDrop drop,XL6_CodeContextTrace trace,const XL6_CaptureLayoutV1* layout,const XL6_CaptureLayoutV2* layoutV2,bool queued)
{
    FutureCallback* frame;XL6_CodeLease* lease=NULL;XL6_CodeLeaseAdmission admission;
    if(!proc || size>SIZE_MAX-sizeof(*frame) || (layoutV2 ? layout || !xl6CaptureRuntimeV2Get()->Validate(layoutV2,size) : layout
        ? !xl6CaptureRuntimeV1Get()->Validate(layout,size) : !drop || !trace)){
        xrtSetErrorKind(XERR_ARGUMENT);return NULL;
    }
    size_t bytes=sizeof(*frame)+size,ticketOffset=0;
    if(queued){
        const size_t align=_Alignof(XL6_ContinuationTicket);
        if(bytes>SIZE_MAX-(align-1)){xrtSetErrorKind(XERR_RANGE);return NULL;}
        ticketOffset=(bytes+align-1)&~(align-1);
        if(ticketOffset>SIZE_MAX-sizeof(XL6_ContinuationTicket)){xrtSetErrorKind(XERR_RANGE);return NULL;}
        bytes=ticketOffset+sizeof(XL6_ContinuationTicket);
    }
    admission=xl6CodeLeaseAcquireScoped(owner,&lease);
    if(admission!=XL6_CODE_LEASE_ACQUIRED){if(!xrtGetError())xrtSetErrorKind(admission==XL6_CODE_LEASE_RESOURCE_FAILURE?XERR_MEMORY:XERR_STATE);return NULL;}
    frame=xrtMalloc(bytes);
    if(!frame){xl6CodeLeaseRelease(lease);return NULL;}
    *frame=(FutureCallback){0};frame->lease=lease;frame->proc=proc;frame->drop=drop;frame->trace=trace;frame->layout=layout;frame->layoutV2=layoutV2;
    frame->references=1;frame->payloadLive=true;frame->dataSize=size;frame->queued=queued;
    if(queued){frame->ticket=(XL6_ContinuationTicket*)((unsigned char*)frame+ticketOffset);*frame->ticket=(XL6_ContinuationTicket){0};}
    return frame->data;
}
static void* allocate(const XL6_CodeOwner* owner,size_t size,XL6_FutureCallbackProc proc,
    XL6_FutureCallbackDrop drop,XL6_CodeContextTrace trace)
{ XL6_OWNERSHIP_RETURN(void*,allocate_body(owner,size,proc,drop,trace,NULL,NULL,false)); }
static void* allocate_v2(const XL6_CodeOwner* owner,size_t size,XL6_FutureCallbackProc proc,
    const XL6_CaptureLayoutV1* layout)
{
    if(!layout){xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    XL6_OWNERSHIP_RETURN(void*,allocate_body(owner,size,proc,NULL,NULL,layout,NULL,false));
}
static void* allocate_v4(const XL6_CodeOwner* owner,size_t size,XL6_FutureCallbackProc proc,const XL6_CaptureLayoutV2* layout)
{
    if(!layout){xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    XL6_OWNERSHIP_RETURN(void*,allocate_body(owner,size,proc,NULL,NULL,NULL,layout,false));
}
void* xl6CallbackFrameAllocateQueuedV1(const XL6_CodeOwner* owner,size_t size,XL6_FutureCallbackProc proc,const XL6_CaptureLayoutV2* layout)
{
    if(!layout){xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    XL6_OWNERSHIP_RETURN(void*,allocate_body(owner,size,proc,NULL,NULL,NULL,layout,true));
}
static void cleanup(void* data)
{
    FutureCallback* frame=header(data);
    if(!frame || !frame_has_layout(frame) || !frame->payloadLive || (!frame->active && frame->committed))abort();
    if(frame->layoutV2) {
        xl6_error_result outcome=xl6CaptureRuntimeV2Get()->Cleanup(frame->layoutV2,data,frame->dataSize,(xl6_error_result){true,NULL});
        if(!outcome.ok)xl6ErrorScopePropagateFailure(outcome);
    } else xl6CaptureRuntimeV1Get()->Cleanup(frame->layout,data);
}
xl6_error_result xl6CallbackFrameCleanupResultV1(FutureCallback* frame,xl6_error_result incoming)
{
    if(!frame||!frame->layoutV2||!frame->payloadLive||!frame->active||frame->retiring)abort();
    return xl6CaptureRuntimeV2Get()->Cleanup(frame->layoutV2,frame->data,frame->dataSize,incoming);
}
static void frame_release(FutureCallback* frame)
{
    xrtownershipscope mutation={0};XL6_CodeLease* lease;frame_begin(frame,&mutation);
    if(!frame->references)abort();
    if(--frame->references){frame_end(frame,&mutation);return;}
    if(frame->active || frame->payloadLive || frame->source || frame->output || frame->cancel || frame->cancelSource ||
        (frame->ticket && frame->ticket->queue))abort();
    lease=frame->lease;frame_unlock(frame);
    if(frame->interruptOps)frame->interruptOps->Destroy(frame);
    xrtFree(frame);mutation_end(&mutation);xl6CodeLeaseRelease(lease);
}
static void producer_drop(const void* data) { frame_release((FutureCallback*)data); }
void xl6CallbackFrameReleaseV1(FutureCallback* frame) {frame_release(frame);}
static void cancel_source(void* data)
{
    FutureCallback* frame=data;xrtownershipscope mutation={0};xfuture* source;
    xerror* previous=xrtTakeError();
    /* Cancellation may synchronously complete the source and retire its
     * Watch. Keep an independent source reference until dispatch returns;
     * the certified cancellation registration independently keeps the frame. */
    frame_begin(frame,&mutation);++frame->active;
    source=xrtFutureRef(frame->source);frame_end(frame,&mutation);
    if(source){(void)xrtFutureCancel(source);xrtFutureDestroy(source);}
    frame_begin(frame,&mutation);--frame->active;frame_end(frame,&mutation);
    xrtClearError();xrtSetErrorTake(previous);
}
static const xcancelwatchownershipv1 cancel_policy={sizeof(cancel_policy),cancel_source,producer_drop,&xl6FutureCallbackFrameOpsV1};
const xcancelwatchownershipv1* xl6FutureCallbackCancelPolicyV1Get(void) { return &cancel_policy; }
bool xl6CallbackFrameBindCancellationV1(FutureCallback* frame)
{
    if(!frame||!frame->source||!frame->output||!frame->cancel||frame->cancelSource||
        frame->committed||frame->publishing){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    xrtownershipscope mutation={0};frame_begin(frame,&mutation);
    if(frame->references==SIZE_MAX){frame_end(frame,&mutation);xrtSetErrorKind(XERR_RANGE);return false;}
    ++frame->references;frame_end(frame,&mutation);
    frame->cancelSource=xrtCancelWatchOwnedV1(frame->cancel,frame,&cancel_policy);
    if(frame->cancelSource)return true;
    frame_release(frame);return false;
}
static const xfutureproducerownershipv1 producer_policy={sizeof(producer_policy),producer_drop};
const xfutureproducerownershipv1* xl6FutureCallbackProducerPolicyV1Get(void) { return &producer_policy; }
static bool bind_output(FutureCallback* frame)
{
    xrtownershipscope mutation={0};
    frame_begin(frame,&mutation);
    if(frame->references==SIZE_MAX)abort();
    ++frame->references;frame_end(frame,&mutation);
    if(xrtPromiseProducerBindTakeV1(frame->output,(xrtownershipref){frame,&xl6FutureCallbackFrameOpsV1},&producer_policy))return true;
    frame_release(frame);return false;
}
bool xl6CallbackFrameBindOutputV1(FutureCallback* frame) {return bind_output(frame);}
static void detach_transport(FutureCallback* frame)
{
    if(frame->interruptOps)frame->interruptOps->Detach(frame);
    xrtownershipscope mutation={0};xfuture* source;xpromise* output;xcancel* cancel;xcancelwatch* cancelSource;
    frame_begin(frame,&mutation);++frame->active;
    source=frame->source;output=frame->output;cancel=frame->cancel;cancelSource=frame->cancelSource;
    frame->source=NULL;frame->output=NULL;frame->cancel=NULL;frame->cancelSource=NULL;frame_end(frame,&mutation);
    xrtCancelUnwatch(cancelSource);
    xrtPromiseDestroy(output);xrtCancelDestroy(cancel);xrtFutureDestroy(source);
    frame_begin(frame,&mutation);--frame->active;frame_end(frame,&mutation);
}
void xl6CallbackFrameDetachV1(FutureCallback* frame) {detach_transport(frame);}
void xl6CallbackFrameRetireV1(FutureCallback* frame,bool consume)
{
    XL6_CodeScope code={0};xrtownershipscope mutation={0};xerror* previous=xrtTakeError();
    xl6CodeScopeEnter(&code,frame->lease);
    if(frame->interruptOps)frame->interruptOps->Detach(frame);
    frame_begin(frame,&mutation);if(!frame->payloadLive)abort();frame->retiring=true;++frame->active;frame_end(frame,&mutation);
    if(frame_has_layout(frame))cleanup(frame->data);else frame->drop(frame->data,NULL);
    frame_begin(frame,&mutation);frame->payloadLive=false;--frame->active;frame_end(frame,&mutation);
    detach_transport(frame);
    frame_begin(frame,&mutation);frame->retiring=false;frame_end(frame,&mutation);
    if(consume)frame_release(frame);
    xl6CodeScopeLeave(&code);
    xrtClearError();xrtSetErrorTake(previous);
}
static void drop_release(void* data) {xl6CallbackFrameRetireV1(data,true);}
static void discard(void* data)
{
    FutureCallback* frame=header(data);if(!frame)return;
    if(frame->committed || frame->publishing || (!frame->interruptOps && frame->references!=1))abort();
    drop_release(frame);
}
void xl6CallbackFrameInvokeV1(FutureCallback* frame)
{
    XL6_CodeScope code={0};xrtownershipscope mutation={0};xfutureresult input={0};
    xerror* previous=xrtTakeError();xl6CodeScopeEnter(&code,frame->lease);
    frame_begin(frame,&mutation);++frame->active;frame_end(frame,&mutation);
    bool valid=frame->source && xrtFutureResult(frame->source,&input);xerror* selected=NULL;
    if(frame->interruptOps){frame->interruptOps->Select(frame,&input,&selected);valid=true;}
    frame->proc(valid?&input:NULL,frame->output,frame->cancel,frame->data);
    xrtErrorFree(selected);
    frame_begin(frame,&mutation);--frame->active;frame_end(frame,&mutation);
    xl6CodeScopeLeave(&code);xrtClearError();xrtSetErrorTake(previous);
}
static void notify(void* data) {xl6CallbackFrameInvokeV1(data);}
static bool watch_trace(const void* data,xrtownershipvisitor visit,void* context)
{ return visit((xrtownershipref){data,&xl6FutureCallbackFrameOpsV1},context); }
static const xfuturewatchownershipv1 watch_policy={sizeof(watch_policy),notify,drop_release,&xl6FutureCallbackFrameOpsV1};
const xfuturewatchownershipv1* xl6FutureCallbackWatchPolicyV1Get(void) { return &watch_policy; }
static bool hold_frame(const void* data)
{
    FutureCallback* frame=(FutureCallback*)data;xrtownershipscope mutation={0};bool held=false;
    frame_begin(frame,&mutation);
    if(!frame->cleared && frame->references && frame->references<SIZE_MAX){++frame->references;held=true;}
    frame_end(frame,&mutation);return held;
}
static bool claim_frame(const void* data,const void* token)
{
    FutureCallback* frame=(FutureCallback*)data;
    if(!token || frame->cleared || (frame->claim && frame->claim!=token))return false;
    frame->claim=token;return true;
}
static void restore_frame(const void* data,const void* token)
{
    FutureCallback* frame=(FutureCallback*)data;
    if(!token || frame->claim!=token || frame->cleared)abort();
    frame->claim=NULL;
}
static bool prepared_frame(const void* data)
{
    const FutureCallback* frame=data;
    return !frame->payloadLive && !frame->active && !frame->retiring && !frame->source && !frame->output && !frame->cancel && !frame->cancelSource &&
        (!frame->interruptOps || frame->interruptOps->Idle(frame));
}
static xrtownershipprepareresult prepare_frame(const void* data,const void* token)
{
    FutureCallback* frame=(FutureCallback*)data;xrtownershipscope mutation={0};bool ready;
    frame_begin(frame,&mutation);
    if(!token || frame->claim!=token || frame->cleared)abort();
    ready=prepared_frame(frame);frame_end(frame,&mutation);
    /* Only normal source notification retires the accepted language callback.
     * Never remove its Watch here: doing so would omit catch/finally. */
    return ready?XRT_OWNERSHIP_PREPARE_READY:XRT_OWNERSHIP_PREPARE_BUSY;
}
static void clear_frame(const void* data,const void* token)
{
    FutureCallback* frame=(FutureCallback*)data;
    if(!token || frame->claim!=token || frame->cleared || !prepared_frame(frame))abort();
    frame->cleared=true; /* Real code lease remains until the final plan Drop. */
}
const xrtownershipadapterv1* xl6FutureCallbackOwnershipAdapterV1(xrtownershipref ref,
    const xrtownershippreparationv1** preparation)
{
    static const xrtownershipadapterv1 adapter={sizeof(adapter),hold_frame,producer_drop,
        claim_frame,restore_frame,NULL,clear_frame,NULL};
    static const xrtownershippreparationv1 prepare={sizeof(prepare),&adapter,prepared_frame,prepare_frame};
    const FutureCallback* frame=ref.Data;size_t count;
    if(!preparation || ref.Ops!=&xl6FutureCallbackFrameOpsV1 || !frame || !frame_layout_valid(frame) || !frame_count(frame,&count))return NULL;
    *preparation=&prepare;return &adapter;
}
static xfuture* start_body(void* data,xfuture* source,bool exclusive)
{
    FutureCallback* frame=header(data);xfuture* future=NULL;xfuturewatchresult status;
    xrtownershipscope mutation={0};xerror* previous;
    if(!frame || !source || frame->queued || frame->references!=1 || frame->committed || frame->publishing){xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    /* Every fallible preparation precedes registration. Private fields cannot
     * be traced; publication and RC transitions are serialized independently. */
    frame->source=xrtFutureRef(source);if(!frame->source)goto failed;
    frame->output=xrtPromiseCreate(&future,NULL);if(!frame->output)goto failed;
    if(!bind_output(frame))goto failed;
    frame->cancel=xrtPromiseCancelToken(frame->output);if(!frame->cancel)goto failed;
    if(exclusive&&!xl6CallbackFrameBindCancellationV1(frame))goto failed;
    if(frame_has_layout(frame)){
        if(!xrtFutureWatchInitOwnershipV1(&frame->watch,frame,&watch_policy))goto failed;
    }else if(!xrtFutureWatchInitPhased(&frame->watch,notify,drop_release,frame,watch_trace))goto failed;
    frame_begin(frame,&mutation);frame->publishing=true;++frame->references;frame_end(frame,&mutation);
    status=xrtFutureWatchAdd(source,&frame->watch);
    if(status==XFUTURE_WATCH_ERROR){
        frame_begin(frame,&mutation);--frame->references;frame->publishing=false;frame_end(frame,&mutation);goto failed;
    }
    if(status==XFUTURE_WATCH_READY){notify(frame);drop_release(frame);}
    frame_begin(frame,&mutation);frame->committed=true;frame->publishing=false;frame_end(frame,&mutation);
    frame_release(frame);return future;
failed:
    previous=xrtTakeError();detach_transport(frame);xrtFutureDestroy(future);
    xrtClearError();if(previous)xrtSetErrorTake(previous);else xrtSetErrorKind(XERR_INTERNAL);
    return NULL;
}
static xfuture* start(void* data,xfuture* source) { return start_body(data,source,false); }
static xfuture* start_exclusive(void* data,xfuture* source) { return start_body(data,source,true); }
const XL6_FutureCallbackRuntimeV1* xl6FutureCallbackRuntimeV1Get(void)
{
    static const XL6_FutureCallbackRuntimeV1 api={allocate,discard,ownership,start};return &api;
}
const XL6_FutureCallbackRuntimeV2* xl6FutureCallbackRuntimeV2Get(void)
{
    static const XL6_FutureCallbackRuntimeV2 api={allocate_v2,cleanup};return &api;
}
const XL6_FutureCallbackRuntimeV3* xl6FutureCallbackRuntimeV3Get(void)
{
    static const XL6_FutureCallbackRuntimeV3 api={start_exclusive};return &api;
}
const XL6_FutureCallbackRuntimeV4* xl6FutureCallbackRuntimeV4Get(void)
{
    static const XL6_FutureCallbackRuntimeV4 api={allocate_v4,cleanup};return &api;
}
