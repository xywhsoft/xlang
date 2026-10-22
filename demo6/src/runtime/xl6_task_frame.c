#define XRT_MODULE_TASK
#define XRT_MODULE_THREAD
#define XRT_MODULE_MEMORY
#define XRT_MODULE_ATOMIC
#include "lib/xrt/xrt.h"
#include "xl6_task.h"
#include "xl6_ownership_scope.h"
#include "src/api/xl_foreign_error.h"
#include <stddef.h>

#include "xl6_task_frame_internal.h"

typedef struct TaskFrame {
    xatomic64 gate;
    XL6_CodeLease* lease;
    XL6_TaskPool* pool; /* optional actual owner through the physical frame tail */
    const XL6_TaskFramePoolV1* poolBinding; /* permanent resident, borrowed */
    XL6_TaskRun run;
    XL6_TaskDrop drop;
    XL6_CodeContextTrace trace;
    const XL6_CaptureLayoutV1* layout;
    const XL6_CaptureLayoutV2* layoutV2;
    size_t references, active, dataSize;
    const void* claim;
    bool publishing, committed, runEntered, payloadLive, cleared;
    xfuturewatch watch;
    xfuture* source;
    xpromise* output;
    /* Native completion already preserves the body outcome before data Drop.
     * Language capture cleanup has a separate checked result. It may replace
     * RESOLVED, never an earlier native failure/cancellation/close. */
    xerror* cleanupError;
    bool languageOutcome,cleanupFailed;
    max_align_t alignment;
    unsigned char data[];
} TaskFrame;
static void mutation_begin(xrtownershipscope* scope) { if(!xrtOwnershipMutationBegin(scope))abort(); }
static void mutation_end(xrtownershipscope* scope) { if(!xrtOwnershipScopeEnd(scope))abort(); }
static void error_set(xerrkind kind,const char* name,const char* message)
{
    if(xrtGetError())return;
    xerrordesc desc={kind,0,0,"xlang","throw",message,name,NULL};
    xerror* error=xrtErrorBuild(&desc); if(error)xrtSetErrorTake(error);else xrtSetErrorKind(kind);
}
static TaskFrame* frame_header(const void* data)
{ return data?(TaskFrame*)((unsigned char*)data - offsetof(TaskFrame,data)):NULL; }
/* Mutation admission stabilizes inspection; it does NOT serialize concurrent
 * writers. Job, submitter and Watch references also need this physical lock. */
static void frame_mutation_begin(TaskFrame* frame,xrtownershipscope* mutation)
{
    mutation_begin(mutation);
    for(;;){uint64 expected=0;
        if(xrtAtomic64CompareExchange(&frame->gate,&expected,1,XMEMORY_ACQUIRE,XMEMORY_RELAXED))return;
        xrtThreadYield();
    }
}
static void frame_unlock(TaskFrame* frame) { xrtAtomic64Store(&frame->gate,0,XMEMORY_RELEASE); }
static void frame_mutation_end(TaskFrame* frame,xrtownershipscope* mutation)
{ frame_unlock(frame);mutation_end(mutation); }
static void frame_checked_cleanup(TaskFrame* frame,xl6_error_result outcome)
{
    if(outcome.ok){xrtErrorFree(outcome.error);return;}
    xrtownershipscope mutation={0};frame_mutation_begin(frame,&mutation);
    if(!frame->cleanupFailed){
        frame->cleanupFailed=true;
        if(frame->output){frame->cleanupError=outcome.error;outcome.error=NULL;}
    }
    frame_mutation_end(frame,&mutation);xrtErrorFree(outcome.error);
}
static bool frame_count(const void* data,size_t* count)
{
    const TaskFrame* frame=data;
    if(!frame || !count || !frame->references || !frame->committed || frame->publishing || frame->active || frame->cleared)return false;
    if(frame->lease){xrtownershipref lease=xl6CodeLeaseOwnership(frame->lease);size_t references;
        if(!lease.Ops->Count(lease.Data,&references))return false;}
    *count=frame->references; return true;
}
static bool frame_has_layout(const TaskFrame* frame)
{ return frame->layout || frame->layoutV2; }
static bool frame_layout_valid(const TaskFrame* frame)
{
    return frame->layoutV2 ? !frame->layout && xl6CaptureRuntimeV2Get()->Validate(frame->layoutV2,frame->dataSize) :
        frame->layout && xl6CaptureRuntimeV1Get()->Validate(frame->layout,frame->dataSize);
}
static bool frame_trace(const void* data,xrtownershipvisitor visit,void* context)
{
    const TaskFrame* frame=data; size_t count;
    if(!visit || !frame_count(data,&count))return false;
    return (!frame->lease || visit(xl6CodeLeaseOwnership(frame->lease),context)) &&
        (!frame->pool || visit(frame->poolBinding->Ownership(frame->pool),context)) &&
        (!frame->cleanupError || visit(xrtErrorOwnership(frame->cleanupError),context)) &&
        visit(xrtFutureOwnership(frame->source),context) && visit(xrtPromiseOwnership(frame->output),context) &&
        (!frame->payloadLive || (frame->layoutV2
            ? xl6CaptureRuntimeV2Get()->Trace(frame->layoutV2,frame->data,frame->dataSize,visit,context) : frame->layout
            ? xl6CaptureRuntimeV1Get()->Trace(frame->layout,frame->data,visit,context)
            : frame->trace(frame->data,visit,context)));
}
const xrtownershipops xl6TaskFrameOwnershipOpsInternalV1={frame_count,frame_trace};
xrtownershipref xl6TaskFrameOwnershipInternalV1(const void* data)
{ return (xrtownershipref){frame_header(data),data?&xl6TaskFrameOwnershipOpsInternalV1:NULL}; }
static void* frame_allocate_body(const XL6_CodeOwner* owner,size_t size,XL6_TaskRun run,XL6_TaskDrop drop,
    XL6_CodeContextTrace trace,const XL6_CaptureLayoutV1* layout,const XL6_CaptureLayoutV2* layoutV2)
{
    TaskFrame* frame; XL6_CodeLease* lease=NULL;
    if(!run || size>SIZE_MAX-sizeof(*frame) || (layoutV2 ? layout || !xl6CaptureRuntimeV2Get()->Validate(layoutV2,size) : layout
        ? !xl6CaptureRuntimeV1Get()->Validate(layout,size) : !drop || !trace)) {
        xrtSetErrorKind(XERR_ARGUMENT); return NULL;
    }
    XL6_CodeLeaseAdmission admission=xl6CodeLeaseAcquire(owner,&lease);
    if(admission!=XL6_CODE_LEASE_ACQUIRED){
        error_set(admission==XL6_CODE_LEASE_RESOURCE_FAILURE?XERR_MEMORY:XERR_STATE,
            admission==XL6_CODE_LEASE_RESOURCE_FAILURE?"MemoryError":"StateError","task code admission failed"); return NULL;
    }
    frame=xrtMalloc(sizeof(*frame)+size);
    if(!frame){xl6CodeLeaseRelease(lease);return NULL;}
    *frame=(TaskFrame){0}; frame->lease=lease;frame->run=run;frame->drop=drop;frame->trace=trace;frame->layout=layout;frame->layoutV2=layoutV2;
    frame->references=1;frame->payloadLive=true;frame->dataSize=size;return frame->data;
}
void* xl6TaskFrameAllocateInternalV1(const XL6_CodeOwner* owner,size_t size,XL6_TaskRun run,XL6_TaskDrop drop,XL6_CodeContextTrace trace)
{ XL6_OWNERSHIP_RETURN(void*,frame_allocate_body(owner,size,run,drop,trace,NULL,NULL)); }
static void* frame_allocate_v2(const XL6_CodeOwner* owner,size_t size,XL6_TaskRun run,const XL6_CaptureLayoutV1* layout)
{
    if(!layout){xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    XL6_OWNERSHIP_RETURN(void*,frame_allocate_body(owner,size,run,NULL,NULL,layout,NULL));
}
static void* frame_allocate_v3(const XL6_CodeOwner* owner,size_t size,XL6_TaskRun run,const XL6_CaptureLayoutV2* layout)
{
    if(!layout){xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    XL6_OWNERSHIP_RETURN(void*,frame_allocate_body(owner,size,run,NULL,NULL,NULL,layout));
}
static void frame_cleanup(void* data)
{
    TaskFrame* frame=frame_header(data);
    if(!frame || !frame_has_layout(frame) || !frame->payloadLive || (!frame->active && frame->committed))abort();
    if(frame->layoutV2) {
        xl6_error_result outcome=xl6CaptureRuntimeV2Get()->Cleanup(frame->layoutV2,data,frame->dataSize,(xl6_error_result){true,NULL});
        if(!outcome.ok)xl6ErrorScopePropagateFailure(outcome);
    } else xl6CaptureRuntimeV1Get()->Cleanup(frame->layout,data);
}
const XL6_TaskRuntimeV2* xl6TaskRuntimeV2Get(void)
{
    static const XL6_TaskRuntimeV2 api={frame_allocate_v2,frame_cleanup};return &api;
}
const XL6_TaskRuntimeV3* xl6TaskRuntimeV3Get(void)
{
    static const XL6_TaskRuntimeV3 api={frame_allocate_v3,frame_cleanup};return &api;
}
/* Every physical reference has a real holder: private caller, accepted job,
 * registration caller, registered Watch or pending language output Future.
 * No synthetic module root. Native job output participation is independent. */
static void frame_release(TaskFrame* frame)
{
    xrtownershipscope mutation={0}; XL6_CodeLease* lease;XL6_TaskPool* pool;const XL6_TaskFramePoolV1* framePoolBinding;frame_mutation_begin(frame,&mutation);
    if(!frame->references)abort();
    if(--frame->references){frame_mutation_end(frame,&mutation);return;}
    if(frame->payloadLive || frame->active || frame->source || frame->output)abort();
    lease=frame->lease;pool=frame->pool;framePoolBinding=frame->poolBinding;xerror* cleanupError=frame->cleanupError;frame_unlock(frame);xrtFree(frame);
    xrtErrorFree(cleanupError);
    if(pool)framePoolBinding->Drop(pool);
    mutation_end(&mutation);xl6CodeLeaseRelease(lease);
}
static void producer_drop(const void* data) { frame_release((TaskFrame*)data); }
static const xfutureproducerownershipv1 producer_policy={sizeof(producer_policy),producer_drop};
const xfutureproducerownershipv1* xl6TaskProducerPolicyV1Get(void) { return &producer_policy; }
static bool frame_bind_output(TaskFrame* frame)
{
    xrtownershipscope mutation={0};frame_mutation_begin(frame,&mutation);
    if(frame->references==SIZE_MAX)abort();
    ++frame->references;frame_mutation_end(frame,&mutation);
    if(xrtPromiseProducerBindTakeV1(frame->output,(xrtownershipref){frame,&xl6TaskFrameOwnershipOpsInternalV1},&producer_policy))return true;
    frame_release(frame);return false;
}
static void frame_drop_payload(TaskFrame* frame)
{
    xrtownershipscope mutation={0}; XL6_CodeScope code={0}; XL6_TaskDrop drop;
    XL6_TaskNativeScopeV1 activity={0};const XL6_TaskFramePoolV1* framePoolBinding=frame->poolBinding;
    xl6CodeScopeEnter(&code,frame->lease);
    if(frame->pool)frame->poolBinding->ActivityEnter(&activity,frame->pool);
    frame_mutation_begin(frame,&mutation);
    if(!frame->payloadLive)abort();
    ++frame->active; drop=frame->drop; frame_mutation_end(frame,&mutation);
    if(frame->languageOutcome){
        xerror* previous=xrtTakeError();xl6_error_scope errors={0};
        bool begun=xl6ErrorScopeBegin(&errors);xl6_error_result outcome={false,NULL};
        if(!begun)outcome.error=xrtTakeError();
        if(frame_has_layout(frame))frame_cleanup(frame->data);else drop(frame->data,NULL);
        if(begun)outcome=xl6ErrorScopeEnd(&errors);
        frame_checked_cleanup(frame,outcome);
        xrtClearError();xrtSetErrorTake(previous);
    }else if(frame_has_layout(frame)){
        xerror* previous=xrtTakeError();frame_cleanup(frame->data);
        xrtClearError();xrtSetErrorTake(previous);
    }else drop(frame->data,NULL);
    frame_mutation_begin(frame,&mutation);frame->payloadLive=false;--frame->active;
    if(frame->runEntered){--frame->active;frame->runEntered=false;}
    frame_mutation_end(frame,&mutation);
    frame_release(frame); /* The code scope survives freeing this allocation. */
    xl6CodeScopeLeave(&code);
    if(activity.pool)framePoolBinding->ActivityLeave(&activity);
}
void xl6TaskFrameDiscardInternalV1(void* data)
{
    TaskFrame* frame=frame_header(data); if(!frame)return;
    if(frame->committed || frame->publishing || frame->references!=1)abort();
    frame_drop_payload(frame);
}
static xtaskoutcome task_run(xcancel* cancel,void* data,xtaskvalue* result)
{
    TaskFrame* frame=data; XL6_TaskResult value={0}; XL6_TaskOutcome outcome; XL6_CodeScope code={0};
    XL6_TaskNativeScopeV1 activity={0}; xrtownershipscope mutation={0};
    xl6_error_scope errors={0};bool begun=false;xl6_error_result checked={true,NULL};
    if(frame->languageOutcome){
        begun=xl6ErrorScopeBegin(&errors);
        if(!begun)checked=(xl6_error_result){false,xrtTakeError()};
    }
    xl6CodeScopeEnter(&code,frame->lease); if(frame->pool)frame->poolBinding->ActivityEnter(&activity,frame->pool);
    frame_mutation_begin(frame,&mutation);++frame->active;frame->runEntered=true;frame_mutation_end(frame,&mutation);
    outcome=frame->languageOutcome&&!begun?XL6_TASK_FAILED:frame->run(cancel,frame->data,&value);
    if(outcome!=XL6_TASK_SUCCESS&&outcome!=XL6_TASK_FAILED&&outcome!=XL6_TASK_CANCELLED){
        xrtSetErrorKind(XERR_INTERNAL);outcome=XL6_TASK_FAILED;}
    if(begun&&(outcome==XL6_TASK_FAILED||(outcome==XL6_TASK_SUCCESS&&(errors.failed||xrtGetError()))))
        xl6ErrorScopeCaptureFailure(&errors);
    /* Keep busy through the separate job-data release, including the worker's
     * return gap. Results and partially retired captures must not be traced. */
    result->Value=value.Value;result->Destroy=value.Destroy;result->DestroyData=value.DestroyData;
    xl6CodeScopeLeave(&code);if(activity.pool)frame->poolBinding->ActivityLeave(&activity);
    if(frame->languageOutcome){
        if(begun)checked=xl6ErrorScopeEnd(&errors);
        if(outcome!=XL6_TASK_CANCELLED&&!checked.ok){
            /* XRT converts a diagnosticless FAILED to its allocation-free
             * INTERNAL at the strict native boundary, before any capture Drop.
             * Later cleanup errors cannot replace that already selected body. */
            outcome=XL6_TASK_FAILED;xrtSetErrorTake(checked.error);
        }else xrtErrorFree(checked.error);
    }
    if(outcome==XL6_TASK_SUCCESS)return XTASK_SUCCESS;
    if(outcome==XL6_TASK_CANCELLED)return XTASK_CANCELLED;
    return XTASK_FAILED;
}
static void task_drop(void* data,void* unused) { (void)unused;frame_drop_payload(data); }
static const xtaskdataownershipv1 native_data_policy={sizeof(native_data_policy),task_run,task_drop,&xl6TaskFrameOwnershipOpsInternalV1};
const xtaskdataownershipv1* xl6TaskNativeDataPolicyV1Get(void) { return &native_data_policy; }
static bool complete_error(xpromise* promise,const xerror* error)
{
    /* Native Reject requires a diagnostic. Materialize its allocation-free
     * fallback ONLY after the original checked failure has been selected. */
    if(!error){xrtSetErrorKind(XERR_INTERNAL);return xrtPromiseReject(promise,xrtGetError());}
    switch(xrtErrorKind(error)){
    case XERR_CANCELLED:return xrtPromiseCancel(promise);
    case XERR_CLOSED:return xrtPromiseClose(promise);
    default:return xrtPromiseReject(promise,error);
    }
}
static void outcome_notify(void* data)
{
    TaskFrame* frame=data; xfutureresult input={0}; bool completed;
    xerror* previous=xrtTakeError(); xrtownershipscope mutation={0}; XL6_CodeScope code={0}; XL6_TaskNativeScopeV1 activity={0};
    xl6CodeScopeEnter(&code,frame->lease);if(frame->pool)frame->poolBinding->ActivityEnter(&activity,frame->pool);
    frame_mutation_begin(frame,&mutation);++frame->active;
    bool cleanupFailed=frame->cleanupFailed;
    /* Move the actual owner into this notification, not a shadow error ref
     * retained for the lifetime of an otherwise empty collector-pinned frame. */
    xerror* cleanupError=frame->cleanupError;frame->cleanupError=NULL;
    frame_mutation_end(frame,&mutation);
    if(!xrtFutureResult(frame->source,&input)){
        if(!xrtGetError())xrtSetErrorKind(XERR_INTERNAL);
        completed=complete_error(frame->output,xrtGetError());
    }else if(cleanupFailed&&input.State==XFUTURE_RESOLVED)
        completed=complete_error(frame->output,cleanupError);
    else if(input.State==XFUTURE_FAILED)completed=complete_error(frame->output,input.Error);
    else completed=xrtPromiseForward(frame->output,frame->source);
    /* Native pre-run/body/payload-contract failures precede capture cleanup;
     * cancellation/close likewise keeps its established priority. A successful
     * native body cannot hide a later checked capture failure as RESOLVED. */
    xrtErrorFree(cleanupError);
    if(!completed && !xrtPromiseDone(frame->output)){
        xrtSetErrorKind(XERR_INTERNAL);(void)xrtPromiseReject(frame->output,xrtGetError());
    }
    xrtClearError();xrtSetErrorTake(previous);
    frame_mutation_begin(frame,&mutation);--frame->active;frame_mutation_end(frame,&mutation);
    xl6CodeScopeLeave(&code);if(activity.pool)frame->poolBinding->ActivityLeave(&activity);
}
static void outcome_detach(TaskFrame* frame)
{
    xrtownershipscope mutation={0}; xfuture* source; xpromise* output; xerror* previous=xrtTakeError();
    frame_mutation_begin(frame,&mutation);++frame->active;source=frame->source;output=frame->output;
    xerror* cleanupError=frame->cleanupError;frame->cleanupError=NULL;
    frame->source=NULL;frame->output=NULL;frame_mutation_end(frame,&mutation);
    xrtPromiseDestroy(output);xrtFutureDestroy(source);xrtErrorFree(cleanupError);
    frame_mutation_begin(frame,&mutation);--frame->active;frame_mutation_end(frame,&mutation);
    xrtClearError();xrtSetErrorTake(previous);
}
static void outcome_release(void* data)
{
    TaskFrame* frame=data; XL6_CodeScope code={0}; XL6_TaskNativeScopeV1 activity={0};
    const XL6_TaskFramePoolV1* framePoolBinding=frame->poolBinding;
    xl6CodeScopeEnter(&code,frame->lease);if(frame->pool)frame->poolBinding->ActivityEnter(&activity,frame->pool);
    outcome_detach(frame);frame_release(frame);
    xl6CodeScopeLeave(&code);if(activity.pool)framePoolBinding->ActivityLeave(&activity);
}
static bool outcome_trace(const void* data,xrtownershipvisitor visit,void* context)
{ return visit((xrtownershipref){data,&xl6TaskFrameOwnershipOpsInternalV1},context); }
static const xfuturewatchownershipv1 watch_policy={sizeof(watch_policy),outcome_notify,outcome_release,&xl6TaskFrameOwnershipOpsInternalV1};
const xfuturewatchownershipv1* xl6TaskWatchPolicyV1Get(void) { return &watch_policy; }
static bool hold_frame(const void* data)
{
    TaskFrame* frame=(TaskFrame*)data;xrtownershipscope mutation={0};bool held=false;
    frame_mutation_begin(frame,&mutation);
    if(!frame->cleared&&frame->references&&frame->references<SIZE_MAX){++frame->references;held=true;}
    frame_mutation_end(frame,&mutation);return held;
}
static bool claim_frame(const void* data,const void* token)
{
    TaskFrame* frame=(TaskFrame*)data;
    if(!token||frame->cleared||(frame->claim&&frame->claim!=token))return false;
    frame->claim=token;return true;
}
static void restore_frame(const void* data,const void* token)
{
    TaskFrame* frame=(TaskFrame*)data;
    if(!token||frame->claim!=token||frame->cleared)abort();
    frame->claim=NULL;
}
static bool prepared_frame(const void* data)
{
    const TaskFrame* frame=data;
    return !frame->payloadLive&&!frame->active&&!frame->runEntered&&!frame->source&&!frame->output&&!frame->cleanupError;
}
static xrtownershipprepareresult prepare_frame(const void* data,const void* token)
{
    TaskFrame* frame=(TaskFrame*)data;xrtownershipscope mutation={0};bool ready;
    frame_mutation_begin(frame,&mutation);
    if(!token||frame->claim!=token||frame->cleared)abort();
    ready=prepared_frame(frame);frame_mutation_end(frame,&mutation);
    /* Accepted work and outcome forwarding own these duties. Never remove
     * the Watch or destroy captures to make a pending task look prepared. */
    return ready?XRT_OWNERSHIP_PREPARE_READY:XRT_OWNERSHIP_PREPARE_BUSY;
}
static void clear_frame(const void* data,const void* token)
{
    TaskFrame* frame=(TaskFrame*)data;
    if(!token||frame->claim!=token||frame->cleared||!prepared_frame(frame))abort();
    frame->cleared=true;
}
const xrtownershipadapterv1* xl6TaskOwnershipAdapterV1(xrtownershipref ref,
    const xrtownershippreparationv1** preparation)
{
    static const xrtownershipadapterv1 adapter={sizeof(adapter),hold_frame,producer_drop,
        claim_frame,restore_frame,NULL,clear_frame,NULL};
    static const xrtownershippreparationv1 prepare={sizeof(prepare),&adapter,prepared_frame,prepare_frame};
    const TaskFrame* frame=ref.Data;size_t count;
    if(!preparation||ref.Ops!=&xl6TaskFrameOwnershipOpsInternalV1||!frame||!frame_layout_valid(frame)||!frame_count(frame,&count))return NULL;
    *preparation=&prepare;return &adapter;
}
static void outcome_registration_failed(TaskFrame* frame,xfuture* future)
{
    xerror* error=xrtTakeError();if(!error){xrtSetErrorKind(XERR_INTERNAL);error=xrtTakeError();}
    (void)xrtFutureCancel(future);(void)complete_error(frame->output,error);xrtErrorFree(error);outcome_release(frame);
}
xfuture* xl6TaskFrameSubmitInternalV1(void* data,xcancel* cancel,
    XL6_FuturePayloadTrace trace,const xfuturepayloadownershipv1* policy,bool language,const XL6_TaskFrameTargetV1* target)
{
    TaskFrame* frame=frame_header(data);xfuture *future=NULL,*source;
    XL6_TaskPool* pool=target?target->pool:NULL;
    const XL6_TaskFramePoolV1* poolBinding=target?target->poolBinding:NULL;
    xrtownershipscope mutation={0};xerror* previous;
    xcancel* token=NULL;
    if(!target || !target->Submit || (!!target->Begin!=!!target->End) || (!!pool!=!!poolBinding) ||
        (poolBinding&&(!poolBinding->Hold||!poolBinding->Drop||!poolBinding->Ownership||!poolBinding->ActivityEnter||!poolBinding->ActivityLeave)) ||
        !frame || frame->references!=1 || frame->committed || frame->publishing){xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    if(language){
        frame->output=xrtPromiseCreate(&future,cancel);if(!frame->output)return NULL;
        if(!frame_bind_output(frame))goto preparation_failed;
        if(frame_has_layout(frame)){
            if(!xrtFutureWatchInitOwnershipV1(&frame->watch,frame,&watch_policy))goto preparation_failed;
        }else if(!xrtFutureWatchInitPhased(&frame->watch,outcome_notify,outcome_release,frame,outcome_trace))goto preparation_failed;
        token=xrtPromiseCancelToken(frame->output);if(!token)goto preparation_failed;
    }
    if(target->Begin&&!target->Begin(target->context))goto preparation_failed;
    if(pool&&!poolBinding->Hold(pool))abort(); /* real frame owner, independent of admission */
    frame_mutation_begin(frame,&mutation);frame->pool=pool;frame->poolBinding=poolBinding;frame->publishing=true;
    /* One provisional job reference is returned without payload destruction
     * on rejection. The registration reference is committed to Watch later. */
    ++frame->references;if(language)++frame->references;frame->languageOutcome=language;frame_mutation_end(frame,&mutation);
    source=target->Submit(target->context,frame,language?token:cancel,trace,policy);
    xrtCancelDestroy(token);token=NULL;
    if(!source){
        frame_mutation_begin(frame,&mutation);if(frame->references!=(language?4u:2u) || !frame->payloadLive)abort();
        /* Return the provisional job/registration references, not the output's
         * actual producer owner. Closing that private output returns it below. */
        frame->references-=language?2u:1u;frame->publishing=false;frame->pool=NULL;frame->poolBinding=NULL;frame->languageOutcome=false;
        frame_unlock(frame);if(pool)poolBinding->Drop(pool);mutation_end(&mutation);
        if(target->End)target->End(target->context);
        goto preparation_failed;
    }
    if(language){
        frame_mutation_begin(frame,&mutation);frame->source=source;frame_mutation_end(frame,&mutation);
        previous=xrtTakeError();xfuturewatchresult status=xrtFutureWatchAdd(source,&frame->watch);
        if(status==XFUTURE_WATCH_READY){outcome_notify(frame);outcome_release(frame);}
        else if(status==XFUTURE_WATCH_ERROR)outcome_registration_failed(frame,future);
        xrtClearError();xrtSetErrorTake(previous);
    }else future=source;
    frame_mutation_begin(frame,&mutation);frame->committed=true;frame->publishing=false;frame_mutation_end(frame,&mutation);
    frame_release(frame);if(target->End)target->End(target->context);return future;
preparation_failed:
    previous=xrtTakeError();xrtCancelDestroy(token);
    if(language){outcome_detach(frame);xrtFutureDestroy(future);}
    xrtClearError();if(previous)xrtSetErrorTake(previous);else xrtSetErrorKind(XERR_INTERNAL);
    return NULL;
}
