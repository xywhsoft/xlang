#define XRT_MODULE_FUTURE
#define XRT_MODULE_MEMORY
#define XRT_MODULE_ATOMIC
#define XRT_MODULE_THREAD
#include "lib/xrt/xrt.h"
#include "xl6_future_callback_internal.h"
#include "xl6_continuation_executor.h"
#include "xl6_continuation_future.h"
#include "xl6_ownership_scope.h"
#include "../api/xl_foreign_error.h"

typedef XL6_FutureCallbackFrameV1 Frame;
static void* allocate(const XL6_CodeOwner* owner,size_t bytes,XL6_FutureCallbackProc proc,const XL6_CaptureLayoutV2* layout)
{
    return xl6CallbackFrameAllocateQueuedV1(owner,bytes,proc,layout);
}
static xl6_error_result cleanup(void* data,xl6_error_result incoming)
{
    Frame* frame=xl6CallbackFrameHeaderV1(data);
    if(!frame||!frame->queued||!frame->active||frame->retiring)abort();
    xl6_error_result outcome=xl6CallbackFrameCleanupResultV1(frame,incoming);
    /* Semantic completion precedes terminal output publication. The executor
     * still joins every physical cleanup/code-owner tail independently. */
    if(!xl6ContinuationComplete(frame->ticket))abort();
    return outcome;
}
static bool hold(const void* data)
{
    Frame* frame=xl6CallbackFrameHeaderV1(data);xrtownershipscope mutation={0};bool held=false;
    if(!frame)return false;
    xl6CallbackFrameBeginV1(frame,&mutation);
    if(!frame->cleared&&frame->references&&frame->references<SIZE_MAX){++frame->references;held=true;}
    xl6CallbackFrameEndV1(frame,&mutation);return held;
}
static void release(const void* data) {xl6CallbackFrameReleaseV1(xl6CallbackFrameHeaderV1(data));}
static xrtownershipref ownership(const void* data) {return xl6FutureCallbackRuntimeV1Get()->Ownership(data);}
static void ready(void* data) {Frame* frame=data;xl6ContinuationReady(frame->ticket);}
static void released(void* data) {Frame* frame=data;xl6ContinuationTransportReleased(frame->ticket);}
const xfuturewatchownershipv1* xl6FutureCallbackQueuedWatchPolicyV1Get(void)
{
    static const xfuturewatchownershipv1 policy={sizeof(policy),ready,released,&xl6FutureCallbackFrameOpsV1};
    return &policy;
}
static bool watch_init(const void* data,xfuturewatch* watch)
{
    Frame* frame=xl6CallbackFrameHeaderV1(data);
    if(!frame||!frame->queued||watch!=&frame->watch){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    return xrtFutureWatchInitOwnershipV1(watch,frame,xl6FutureCallbackQueuedWatchPolicyV1Get());
}
static void commit(const void* data)
{
    Frame* frame=xl6CallbackFrameHeaderV1(data);xrtownershipscope mutation={0};
    xl6CallbackFrameBeginV1(frame,&mutation);
    if(!frame->queued||frame->committed||!frame->publishing||!frame->references||frame->cleared)abort();
    frame->committed=true;frame->publishing=false;
    xl6CallbackFrameEndV1(frame,&mutation);
}
static const XL6_ContinuationStorageV2 storage={hold,release,ownership,watch_init,commit,NULL,NULL,NULL};
const XL6_ContinuationStorageV2* xl6FutureCallbackQueuedStorageV2Get(void) {return &storage;}
static void run(void* data) {xl6CallbackFrameInvokeV1(xl6CallbackFrameHeaderV1(data));}
static void retire(void* data) {xl6CallbackFrameRetireV1(xl6CallbackFrameHeaderV1(data),false);}
static xfuture* start_body(XL6_ContinuationExecutor* executor,void* data,xfuture* source,bool exclusive)
{
    Frame* frame=xl6CallbackFrameHeaderV1(data);xfuture* future=NULL;xrtownershipscope mutation={0};
    if(!executor||!frame||!frame->queued||!frame->layoutV2||!source||frame->references!=1||
        frame->committed||frame->publishing||!frame->ticket||frame->ticket->queue){xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    frame->source=xrtFutureRef(source);if(!frame->source)goto failed;
    frame->output=xrtPromiseCreate(&future,NULL);if(!frame->output)goto failed;
    if(!xl6CallbackFrameBindOutputV1(frame))goto failed;
    frame->cancel=xrtPromiseCancelToken(frame->output);if(!frame->cancel)goto failed;
    if(exclusive&&!xl6CallbackFrameBindCancellationV1(frame))goto failed;
    XL6_ContinuationAdmission admitted=xl6ContinuationExecutorReserve(executor,frame->ticket,run,retire,data);
    if(admitted!=XL6_CONTINUATION_ADMITTED){
        if(!xrtGetError())xrtSetErrorKind(admitted==XL6_CONTINUATION_FULL?XERR_RANGE:
            admitted==XL6_CONTINUATION_RESOURCE_FAILURE?XERR_MEMORY:XERR_STATE);
        goto failed;
    }
    /* The builder reference is the queue's actual base owner after Commit;
     * no extra code lease, hidden owner or independent ticket node exists. */
    xl6CallbackFrameBeginV1(frame,&mutation);frame->ticket->storage=&storage;frame->publishing=true;
    xl6CallbackFrameEndV1(frame,&mutation);
    if(!xl6ContinuationWatchRegister(frame->ticket,&frame->watch,source)){
        if(!xl6ContinuationAbort(frame->ticket))abort();
        xl6CallbackFrameBeginV1(frame,&mutation);frame->publishing=false;xl6CallbackFrameEndV1(frame,&mutation);
        goto failed;
    }
    xl6ContinuationCommit(frame->ticket); /* final handoff: frame may retire */
    return future;
failed:;
    xerror* previous=xrtTakeError();xl6CallbackFrameDetachV1(frame);xrtFutureDestroy(future);
    xrtClearError();if(previous)xrtSetErrorTake(previous);else xrtSetErrorKind(XERR_INTERNAL);
    return NULL;
}
static xfuture* start(XL6_ContinuationExecutor* executor,void* data,xfuture* source)
{XL6_OWNERSHIP_RETURN(xfuture*,start_body(executor,data,source,false));}
static xfuture* start_exclusive(XL6_ContinuationExecutor* executor,void* data,xfuture* source)
{XL6_OWNERSHIP_RETURN(xfuture*,start_body(executor,data,source,true));}
const XL6_FutureCallbackRuntimeV5* xl6FutureCallbackRuntimeV5Get(void)
{
    static const XL6_FutureCallbackRuntimeV5 api={allocate,cleanup,start,xl6FutureCallbackFailV1};return &api;
}
const XL6_FutureCallbackRuntimeV6* xl6FutureCallbackRuntimeV6Get(void)
{
    static const XL6_FutureCallbackRuntimeV6 api={start_exclusive};return &api;
}
