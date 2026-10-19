#define XRT_MODULE_TASK_POOL
#define XRT_MODULE_MEMORY
#define XRT_MODULE_ATOMIC
#include "lib/xrt/xrt.h"
#include "xl6_task.h"
#include "xl6_ownership_scope.h"
#include "src/api/xl_foreign_error.h"
#include <stddef.h>

#include "xl6_task_pool_internal.h"

typedef struct TaskFrame {
    xatomic64 gate;
    XL6_CodeLease* lease;
    XL6_TaskPool* pool; /* one actual reference through the physical frame tail */
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
        (!frame->pool || visit(xl6TaskPoolOwnership(frame->pool),context)) &&
        visit(xrtFutureOwnership(frame->source),context) && visit(xrtPromiseOwnership(frame->output),context) &&
        (!frame->payloadLive || (frame->layoutV2
            ? xl6CaptureRuntimeV2Get()->Trace(frame->layoutV2,frame->data,frame->dataSize,visit,context) : frame->layout
            ? xl6CaptureRuntimeV1Get()->Trace(frame->layout,frame->data,visit,context)
            : frame->trace(frame->data,visit,context)));
}
static const xrtownershipops frame_ops={frame_count,frame_trace};
static xrtownershipref frame_ownership(const void* data)
{ return (xrtownershipref){frame_header(data),data?&frame_ops:NULL}; }
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
static void* frame_allocate(const XL6_CodeOwner* owner,size_t size,XL6_TaskRun run,XL6_TaskDrop drop,XL6_CodeContextTrace trace)
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
    xrtownershipscope mutation={0}; XL6_CodeLease* lease;XL6_TaskPool* pool;frame_mutation_begin(frame,&mutation);
    if(!frame->references)abort();
    if(--frame->references){frame_mutation_end(frame,&mutation);return;}
    if(frame->payloadLive || frame->active || frame->source || frame->output)abort();
    lease=frame->lease;pool=frame->pool;frame_unlock(frame);xrtFree(frame);
    xl6TaskPoolDropInternalV1(pool);mutation_end(&mutation);xl6CodeLeaseRelease(lease);
}
static void producer_drop(const void* data) { frame_release((TaskFrame*)data); }
static const xfutureproducerownershipv1 producer_policy={sizeof(producer_policy),producer_drop};
const xfutureproducerownershipv1* xl6TaskProducerPolicyV1Get(void) { return &producer_policy; }
static bool frame_bind_output(TaskFrame* frame)
{
    xrtownershipscope mutation={0};frame_mutation_begin(frame,&mutation);
    if(frame->references==SIZE_MAX)abort();
    ++frame->references;frame_mutation_end(frame,&mutation);
    if(xrtPromiseProducerBindTakeV1(frame->output,(xrtownershipref){frame,&frame_ops},&producer_policy))return true;
    frame_release(frame);return false;
}
static void frame_drop_payload(TaskFrame* frame)
{
    xrtownershipscope mutation={0}; XL6_CodeScope code={0}; XL6_TaskDrop drop;
    TaskActivity activity={0};
    xl6CodeScopeEnter(&code,frame->lease);
    if(frame->pool)xl6TaskActivityEnterInternalV1(&activity,frame->pool);
    frame_mutation_begin(frame,&mutation);
    if(!frame->payloadLive)abort();
    ++frame->active; drop=frame->drop; frame_mutation_end(frame,&mutation);
    if(frame_has_layout(frame)){
        xerror* previous=xrtTakeError();frame_cleanup(frame->data);
        xrtClearError();xrtSetErrorTake(previous);
    }else drop(frame->data,NULL);
    frame_mutation_begin(frame,&mutation);frame->payloadLive=false;--frame->active;
    if(frame->runEntered){--frame->active;frame->runEntered=false;}
    frame_mutation_end(frame,&mutation);
    frame_release(frame); /* The code scope survives freeing this allocation. */
    xl6CodeScopeLeave(&code);
    if(activity.pool)xl6TaskActivityLeaveInternalV1(&activity);
}
static void frame_discard(void* data)
{
    TaskFrame* frame=frame_header(data); if(!frame)return;
    if(frame->committed || frame->publishing || frame->references!=1)abort();
    frame_drop_payload(frame);
}
static xtaskoutcome task_run(xcancel* cancel,void* data,xtaskvalue* result)
{
    TaskFrame* frame=data; XL6_TaskResult value={0}; XL6_TaskOutcome outcome; XL6_CodeScope code={0};
    TaskActivity activity={0}; xrtownershipscope mutation={0};
    xl6CodeScopeEnter(&code,frame->lease); xl6TaskActivityEnterInternalV1(&activity,frame->pool);
    frame_mutation_begin(frame,&mutation);++frame->active;frame->runEntered=true;frame_mutation_end(frame,&mutation);
    outcome=frame->run(cancel,frame->data,&value);
    /* Keep busy through the separate job-data release, including the worker's
     * return gap. Results and partially retired captures must not be traced. */
    result->Value=value.Value;result->Destroy=value.Destroy;result->DestroyData=value.DestroyData;
    xl6CodeScopeLeave(&code);xl6TaskActivityLeaveInternalV1(&activity);
    if(outcome==XL6_TASK_SUCCESS)return XTASK_SUCCESS;
    if(outcome==XL6_TASK_CANCELLED)return XTASK_CANCELLED;
    if(outcome!=XL6_TASK_FAILED)xrtSetErrorKind(XERR_INTERNAL);
    return XTASK_FAILED;
}
static void task_drop(void* data,void* unused) { (void)unused;frame_drop_payload(data); }
static const xtaskdataownershipv1 native_data_policy={sizeof(native_data_policy),task_run,task_drop,&frame_ops};
const xtaskdataownershipv1* xl6TaskNativeDataPolicyV1Get(void) { return &native_data_policy; }
static bool complete_error(xpromise* promise,const xerror* error)
{
    if(!error)return xrtPromiseClose(promise);
    switch(xrtErrorKind(error)){
    case XERR_CANCELLED:return xrtPromiseCancel(promise);
    case XERR_CLOSED:return xrtPromiseClose(promise);
    default:return xrtPromiseReject(promise,error);
    }
}
static void outcome_notify(void* data)
{
    TaskFrame* frame=data; xfutureresult input={0}; bool completed;
    xerror* previous=xrtTakeError(); xrtownershipscope mutation={0}; XL6_CodeScope code={0}; TaskActivity activity={0};
    xl6CodeScopeEnter(&code,frame->lease);xl6TaskActivityEnterInternalV1(&activity,frame->pool);
    frame_mutation_begin(frame,&mutation);++frame->active;frame_mutation_end(frame,&mutation);
    if(!xrtFutureResult(frame->source,&input)){
        if(!xrtGetError())xrtSetErrorKind(XERR_INTERNAL);
        completed=complete_error(frame->output,xrtGetError());
    }else if(input.State==XFUTURE_FAILED)completed=complete_error(frame->output,input.Error);
    else completed=xrtPromiseForward(frame->output,frame->source);
    if(!completed && !xrtPromiseDone(frame->output)){
        xrtSetErrorKind(XERR_INTERNAL);(void)xrtPromiseReject(frame->output,xrtGetError());
    }
    xrtClearError();xrtSetErrorTake(previous);
    frame_mutation_begin(frame,&mutation);--frame->active;frame_mutation_end(frame,&mutation);
    xl6CodeScopeLeave(&code);xl6TaskActivityLeaveInternalV1(&activity);
}
static void outcome_detach(TaskFrame* frame)
{
    xrtownershipscope mutation={0}; xfuture* source; xpromise* output; xerror* previous=xrtTakeError();
    frame_mutation_begin(frame,&mutation);++frame->active;source=frame->source;output=frame->output;frame->source=NULL;frame->output=NULL;frame_mutation_end(frame,&mutation);
    xrtPromiseDestroy(output);xrtFutureDestroy(source);
    frame_mutation_begin(frame,&mutation);--frame->active;frame_mutation_end(frame,&mutation);
    xrtClearError();xrtSetErrorTake(previous);
}
static void outcome_release(void* data)
{
    TaskFrame* frame=data; XL6_CodeScope code={0}; TaskActivity activity={0};
    xl6CodeScopeEnter(&code,frame->lease);xl6TaskActivityEnterInternalV1(&activity,frame->pool);
    outcome_detach(frame);frame_release(frame);
    xl6CodeScopeLeave(&code);xl6TaskActivityLeaveInternalV1(&activity);
}
static bool outcome_trace(const void* data,xrtownershipvisitor visit,void* context)
{ return visit((xrtownershipref){data,&frame_ops},context); }
static const xfuturewatchownershipv1 watch_policy={sizeof(watch_policy),outcome_notify,outcome_release,&frame_ops};
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
    return !frame->payloadLive&&!frame->active&&!frame->runEntered&&!frame->source&&!frame->output;
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
    if(!preparation||ref.Ops!=&frame_ops||!frame||!frame_layout_valid(frame)||!frame_count(frame,&count))return NULL;
    *preparation=&prepare;return &adapter;
}
static void outcome_registration_failed(TaskFrame* frame,xfuture* future)
{
    xerror* error=xrtTakeError();if(!error){xrtSetErrorKind(XERR_INTERNAL);error=xrtTakeError();}
    (void)xrtFutureCancel(future);(void)complete_error(frame->output,error);xrtErrorFree(error);outcome_release(frame);
}
static xfuture* task_submit_body(XL6_TaskPool* pool,void* data,xcancel* cancel,
    XL6_FuturePayloadTrace trace,const xfuturepayloadownershipv1* policy,bool language)
{
    TaskFrame* frame=frame_header(data);xtaskpool* native;xfuture *future=NULL,*source;
    xtaskargs args={0};TaskActivity activity={0};xrtownershipscope mutation={0};xerror* previous;
    xcancel* token=NULL;
    if(!frame || frame->references!=1 || frame->committed || frame->publishing){xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    if(language){
        frame->output=xrtPromiseCreate(&future,cancel);if(!frame->output)return NULL;
        if(!frame_bind_output(frame))goto preparation_failed;
        if(frame_has_layout(frame)){
            if(!xrtFutureWatchInitOwnershipV1(&frame->watch,frame,&watch_policy))goto preparation_failed;
        }else if(!xrtFutureWatchInitPhased(&frame->watch,outcome_notify,outcome_release,frame,outcome_trace))goto preparation_failed;
        token=xrtPromiseCancelToken(frame->output);if(!token)goto preparation_failed;
    }
    xl6TaskActivityEnterInternalV1(&activity,pool);
    native=xl6TaskSubmitBeginInternalV1(pool);if(!native){xl6TaskActivityLeaveInternalV1(&activity);goto preparation_failed;}
    if(!xl6TaskPoolHoldInternalV1(pool))abort(); /* frame owner, separate from Submit/activity */
    frame_mutation_begin(frame,&mutation);frame->pool=pool;frame->publishing=true;
    /* One provisional job reference is returned without payload destruction
     * on rejection. The registration reference is committed to Watch later. */
    ++frame->references;if(language)++frame->references;frame_mutation_end(frame,&mutation);
    args.Cancel=language?token:cancel;args.Destroy=task_drop;
    source=policy?xrtTaskSubmitOwnedJobV1(native,frame,args.Cancel,&native_data_policy,policy):
        trace?xrtTaskSubmitTraced(native,task_run,frame,&args,trace):xrtTaskSubmit(native,task_run,frame,&args);
    xrtCancelDestroy(token);token=NULL;
    if(!source){
        frame_mutation_begin(frame,&mutation);if(frame->references!=(language?4u:2u) || !frame->payloadLive)abort();
        /* Return the provisional job/registration references, not the output's
         * actual producer owner. Closing that private output returns it below. */
        frame->references-=language?2u:1u;frame->publishing=false;frame->pool=NULL;
        frame_unlock(frame);xl6TaskPoolDropInternalV1(pool);mutation_end(&mutation);
        xl6TaskActivityLeaveInternalV1(&activity);xl6TaskSubmitEndInternalV1(pool);goto preparation_failed;
    }
    if(language){
        frame_mutation_begin(frame,&mutation);frame->source=source;frame_mutation_end(frame,&mutation);
        previous=xrtTakeError();xfuturewatchresult status=xrtFutureWatchAdd(source,&frame->watch);
        if(status==XFUTURE_WATCH_READY){outcome_notify(frame);outcome_release(frame);}
        else if(status==XFUTURE_WATCH_ERROR)outcome_registration_failed(frame,future);
        xrtClearError();xrtSetErrorTake(previous);
    }else future=source;
    frame_mutation_begin(frame,&mutation);frame->committed=true;frame->publishing=false;frame_mutation_end(frame,&mutation);
    frame_release(frame);xl6TaskActivityLeaveInternalV1(&activity);xl6TaskSubmitEndInternalV1(pool);return future;
preparation_failed:
    previous=xrtTakeError();xrtCancelDestroy(token);
    if(language){outcome_detach(frame);xrtFutureDestroy(future);}
    xrtClearError();if(previous)xrtSetErrorTake(previous);else xrtSetErrorKind(XERR_INTERNAL);
    return NULL;
}
static xfuture* task_submit_policy(XL6_TaskPool* pool,void* data,xcancel* cancel,
    XL6_FuturePayloadTrace trace,const xfuturepayloadownershipv1* policy,bool language)
{
    if(!pool){error_set(XERR_STATE,"StateError","module task resource is inactive");return NULL;}
    xl6TaskPoolEnterInternalV1(pool);xfuture* future=task_submit_body(pool,data,cancel,trace,policy,language);
    xl6TaskPoolLeaveInternalV1(pool);return future;
}
static xfuture* task_submit(XL6_TaskPool* pool,void* data,xcancel* cancel,XL6_FuturePayloadTrace trace,bool language)
{ return task_submit_policy(pool,data,cancel,trace,NULL,language); }
xfuture* xl6TaskSubmitPayloadV1(XL6_TaskPool* pool,void* data,xcancel* cancel,XL6_FuturePayloadKind kind,bool language)
{
    const xfuturepayloadownershipv1* policy=xl6PayloadPolicyGet(kind);
    if(!policy){xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    return task_submit_policy(pool,data,cancel,NULL,policy,language);
}
xfuture* xl6TaskSubmitOwnedPolicyV1(XL6_TaskPool* pool,void* data,xcancel* cancel,
    const xfuturepayloadownershipv1* policy,bool language)
{
    if(!policy || policy->size!=sizeof(*policy) || !policy->Drop || !policy->Trace) {
        xrtSetErrorKind(XERR_ARGUMENT);return NULL;
    }
    return task_submit_policy(pool,data,cancel,NULL,policy,language);
}
const XL6_TaskRuntimeV1* xl6TaskRuntimeV1Get(void)
{
    static const XL6_TaskRuntimeV1 api={xl6TaskPoolCreateInternalV1,xl6TaskPoolQuiesceInternalV1,xl6TaskPoolDestroyInternalV1,xl6TaskPoolIsQuiescentInternalV1,xl6TaskPoolIsClosingInternalV1,
        frame_allocate,frame_discard,frame_ownership,task_submit};return &api;
}
