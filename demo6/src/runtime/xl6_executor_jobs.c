#define XRT_MODULE_EXECUTOR
#define XRUNTIME_MODULE_RUNTIME_CALL
#define XRUNTIME_MODULE_RUNTIME_VALUE_CALLABLE
#include "lib/xruntime/xruntime.h"
#include "xl6_executor_jobs.h"
#include "xl6_concurrent_deadline.h"
#include "xl6_execution_worker_exit.h"
#include "src/api/xl_foreign_error.h"
#include <stdlib.h>

typedef struct ExecutorJob {
    xatomic64 gate;
    size_t references;
    XL6_Closure* callback;
    xvalue* boxed;
    XL6_ExecutionFailuresV1* failures;
    xl6_error_result body;
    const void* claim;
    bool active,finished,cleared;
} ExecutorJob;
static const xrtownershipops jobOps;
static void begin(xatomic64* gate,xrtownershipscope* scope)
{
    if(!xrtOwnershipMutationBegin(scope))abort();
    for(;;){uint64 expected=0;
        if(xrtAtomic64CompareExchange(gate,&expected,1,XMEMORY_ACQUIRE,XMEMORY_RELAXED))return;
        xrtThreadYield();}
}
static void end(xatomic64* gate,xrtownershipscope* scope)
{xrtAtomic64Store(gate,0,XMEMORY_RELEASE);if(!xrtOwnershipScopeEnd(scope))abort();}
static void jobRelease(const void* raw)
{
    ExecutorJob* job=(ExecutorJob*)raw;xrtownershipscope mutation={0};begin(&job->gate,&mutation);
    if(!job->references)abort();
    bool last=--job->references==0;
    if(last&&(job->active||!job->finished||job->callback||job->boxed||job->failures||job->body.error||
        (job->claim&&!job->cleared)))abort();
    end(&job->gate,&mutation);if(last)xrtFree(job);
}
static void invoke(void* raw)
{
    ExecutorJob* job=raw;xrtownershipscope mutation={0};begin(&job->gate,&mutation);
    if(job->active||job->finished||(!!job->callback==!!job->boxed)||job->cleared)abort();
    job->active=true;XL6_Closure* callback=job->callback;xvalue* boxed=job->boxed;end(&job->gate,&mutation);
    xerror* previous=xrtTakeError();xl6_error_scope work={0};bool begun=xl6ErrorScopeBegin(&work);
    xl6_error_result result={false,NULL};
    if(begun){
        if(boxed){
            xrtcallframe frame={0};xrtcallresult values=XRT_CALL_RESULT_INIT;
            if(!xrtValueInvoke(boxed,&frame,&values))xl6ErrorScopeCaptureFailure(&work);
            else if(xrtCallResultCount(&values)){xrtSetErrorKind(XERR_TYPE);xl6ErrorScopeCaptureFailure(&work);}
            xrtCallResultUnit(&values);
        }else{
            XL6_ClosureScope scope={0};const XL6_ClosureRuntimeV1* api=xl6ClosureRuntimeV1Get();
            api->Enter(&scope,callback);
            if(!((bool (*)(void*))callback->invoke)(callback->environment))xl6ErrorScopeCaptureFailure(&work);
            api->Leave(&scope);
        }
        result=xl6ErrorScopeEnd(&work);
    }else result.error=xrtTakeError();
    begin(&job->gate,&mutation);job->body=result;end(&job->gate,&mutation);
    xrtClearError();xrtSetErrorTake(previous);
    /* Remain BUSY through Drop; no inspection of half-finished captures. */
}
static void dispose(void* raw,void* context)
{
    if(context)abort();
    ExecutorJob* job=raw;xrtownershipscope mutation={0};begin(&job->gate,&mutation);
    if(job->finished||job->cleared||(!!job->callback==!!job->boxed)||!job->failures)abort();
    job->active=true;XL6_Closure* callback=job->callback;job->callback=NULL;
    xvalue* boxed=job->boxed;job->boxed=NULL;
    XL6_ExecutionFailuresV1* state=job->failures;job->failures=NULL;
    xl6_error_result body=job->body;job->body=(xl6_error_result){true,NULL};end(&job->gate,&mutation);
    xerror* previous=xrtTakeError();xl6_error_scope cleanup={0};bool begun=xl6ErrorScopeBegin(&cleanup);
    xl6_error_result result={false,NULL};
    if(begun){
        if(!body.ok){xrtSetErrorTake(body.error);body.error=NULL;xl6ErrorScopeCaptureFailure(&cleanup);}
        xl6ClosureRuntimeV1Get()->Release(callback);xrtValueRelease(boxed);result=xl6ErrorScopeEnd(&cleanup);
    }else{
        result.error=xrtTakeError();
        if(!body.ok){xrtErrorFree(result.error);result=body;body.error=NULL;}
        xl6ClosureRuntimeV1Get()->Release(callback);xrtValueRelease(boxed);
    }
    xrtErrorFree(body.error);xl6ExecutionFailuresRecordV1(state,result.ok,result.error);xl6ExecutionFailuresReleaseV1(state);
    begin(&job->gate,&mutation);job->active=false;job->finished=true;end(&job->gate,&mutation);
    jobRelease(job);xrtClearError();xrtSetErrorTake(previous);
}
static ExecutorJob* prepare(XL6_ExecutionFailuresV1* state,XL6_Closure* callback)
{
    if(!callback||!callback->invoke||callback->call_parameter_count||
        !xl6ClosureOwnershipAdapterV1(xl6ClosureRuntimeV1Get()->Ownership(callback))){xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    ExecutorJob* job=xrtCalloc(1,sizeof(*job));if(!job)return NULL;
    job->references=1;job->body.ok=true;job->callback=xl6ClosureRuntimeV1Get()->Retain(callback);
    job->failures=xl6ExecutionFailuresRetainV1(state);
    if(!job->callback||!job->failures){
        xerror* error=xrtTakeError();xl6ClosureRuntimeV1Get()->Release(job->callback);
        xl6ExecutionFailuresReleaseV1(job->failures);xrtFree(job);xrtClearError();xrtSetErrorTake(error);return NULL;
    }
    return job;
}
/* Rejection owns PREPARED records, not accepted work. Do not execute dispose:
 * rolling back a borrowed capture is not a failed/cancelled executor job. */
static void abortPrepared(ExecutorJob* job)
{
    XL6_Closure* callback=job->callback;xvalue* boxed=job->boxed;XL6_ExecutionFailuresV1* state=job->failures;
    xrtownershipscope mutation={0};begin(&job->gate,&mutation);
    job->callback=NULL;job->boxed=NULL;job->failures=NULL;job->finished=true;end(&job->gate,&mutation);
    xl6ClosureRuntimeV1Get()->Release(callback);xrtValueRelease(boxed);xl6ExecutionFailuresReleaseV1(state);jobRelease(job);
}
static ExecutorJob* prepareBoxed(XL6_ExecutionFailuresV1* state,const xvalue* boxed,uint64_t signature)
{
    if(!boxed||!signature||xrtValueTypeId(boxed)!=signature||!xl6ClosureBoxAdmittedV1(boxed)){
        xrtSetErrorKind(XERR_TYPE);return NULL;}
    ExecutorJob* job=xrtCalloc(1,sizeof(*job));if(!job)return NULL;
    job->references=1;job->body.ok=true;job->boxed=xrtValueRetain(boxed);
    job->failures=xl6ExecutionFailuresRetainV1(state);
    if(!job->boxed||!job->failures){
        xerror* error=xrtTakeError();xrtValueRelease(job->boxed);xl6ExecutionFailuresReleaseV1(job->failures);
        xrtFree(job);xrtClearError();xrtSetErrorTake(error);return NULL;
    }
    return job;
}
bool xl6ExecutorPrepareBoxedBatchV1(XL6_ExecutionFailuresV1* state,xvalue* const* values,
    size_t count,uint64_t signature,XL6_ExecutorPreparedBatchV1* output)
{
    if(!state||!signature||!output||output->items||output->count||(count&&!values)||
        count>SIZE_MAX/sizeof(xexecutorowneditemv1)){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    if(!count)return true;
    xl6_error_scope scope={0};if(!xl6ErrorScopeBegin(&scope))return false;
    xexecutorowneditemv1* items=xrtCalloc(count,sizeof(*items));size_t prepared=0;
    if(items)for(;prepared<count;++prepared){
        ExecutorJob* job=prepareBoxed(state,values[prepared],signature);if(!job)break;
        items[prepared]=(xexecutorowneditemv1){job,xl6ExecutorClosureDataPolicyV1Get()};
    }
    bool ok=items&&prepared==count&&!scope.failed&&!xrtGetError();
    if(!ok){
        xl6ErrorScopeCaptureFailure(&scope);
        for(size_t i=prepared;i>0;--i)abortPrepared(items[i-1].Data);
        xrtFree(items);
    }
    xl6_error_result result=xl6ErrorScopeEnd(&scope);
    if(!result.ok){xrtSetErrorTake(result.error);return false;}
    xrtErrorFree(result.error);*output=(XL6_ExecutorPreparedBatchV1){items,count};return true;
}
static bool finishBoxedBatch(xexecutor* executor,XL6_ExecutorPreparedBatchV1* batch,bool submit)
{
    if(!batch||(batch->count&&!batch->items)||(!batch->count&&batch->items)||(submit&&!executor)){
        xrtSetErrorKind(XERR_ARGUMENT);return false;}
    xl6_error_scope scope={0};bool begun=xl6ErrorScopeBegin(&scope);
    xl6_error_result outcome={false,NULL};if(!begun)outcome.error=xrtTakeError();
    bool accepted=begun&&submit&&(!batch->count||xrtExecutorSubmitOwnedBatchV1(executor,batch->items,batch->count));
    if(begun&&submit&&!accepted)xl6ErrorScopeCaptureFailure(&scope);
    if(!accepted)for(size_t i=batch->count;i>0;--i)abortPrepared(batch->items[i-1].Data);
    xrtFree(batch->items);*batch=(XL6_ExecutorPreparedBatchV1){0};
    if(begun)outcome=xl6ErrorScopeEnd(&scope);
    if(!outcome.ok){xrtClearError();xrtSetErrorTake(outcome.error);return false;}
    xrtErrorFree(outcome.error);return true;
}
bool xl6ExecutorCommitBoxedBatchV1(xexecutor* executor,XL6_ExecutorPreparedBatchV1* batch)
{return finishBoxedBatch(executor,batch,true);}
bool xl6ExecutorDiscardBoxedBatchV1(XL6_ExecutorPreparedBatchV1* batch)
{return finishBoxedBatch(NULL,batch,false);}
bool xl6ExecutorSubmitClosuresV1(xexecutor* executor,XL6_ExecutionFailuresV1* state,XL6_Closure* const* callbacks,size_t count)
{
    if(!executor||!state||(count&&!callbacks)||count>SIZE_MAX/sizeof(xexecutorowneditemv1)){
        xrtSetErrorKind(XERR_ARGUMENT);return false;}
    if(!count)return xrtExecutorSubmitOwnedBatchV1(executor,NULL,0);
    xexecutorowneditemv1* items=xrtCalloc(count,sizeof(*items));if(!items)return false;
    size_t prepared=0;
    for(;prepared<count;++prepared){
        ExecutorJob* job=prepare(state,callbacks[prepared]);if(!job)break;
        items[prepared]=(xexecutorowneditemv1){job,xl6ExecutorClosureDataPolicyV1Get()};
    }
    bool accepted=prepared==count&&xrtExecutorSubmitOwnedBatchV1(executor,items,count);
    xerror* error=accepted?NULL:xrtTakeError();
    if(!accepted)for(size_t i=prepared;i>0;--i)abortPrepared(items[i-1].Data);
    xrtFree(items);if(!accepted){xrtClearError();xrtSetErrorTake(error);}return accepted;
}
bool xl6ExecutorSubmitClosureV1(xexecutor* executor,XL6_ExecutionFailuresV1* state,XL6_Closure* callback)
{
    if(!executor||!state){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    ExecutorJob* job=prepare(state,callback);if(!job)return false;
    bool accepted=xrtExecutorSubmitOwnedV1(executor,job,xl6ExecutorClosureDataPolicyV1Get());
    if(!accepted){xerror* error=xrtTakeError();abortPrepared(job);xrtClearError();xrtSetErrorTake(error);}return accepted;
}
bool xl6ExecutorWaitV1(xexecutor* executor,XL6_ExecutionFailuresV1* state,int64_t milliseconds,XL6_ExecutorWaitStatusV1* status)
{
    if(status)*status=XL6_EXECUTOR_TIMEOUT_V1;
    if(!status||!state||!executor){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    xdeadline deadline;
    if(!xl6ConcurrentDeadline(milliseconds,&deadline))return false;
    xwaitresult result=xrtExecutorWaitUntil(executor,deadline);
    if(result==XWAIT_TIMEOUT)return true;
    if(result!=XWAIT_OK)return false;
    *status=XL6_EXECUTOR_READY_V1;return xl6ExecutionFailuresCheckV1(state);
}
const xexecutorworkerexitownershipv1* xl6ExecutorWorkerExitPolicyV1Get(void)
{static const xexecutorworkerexitownershipv1 policy={sizeof(policy),xl6ExecutionWorkerExitV1,xl6ExecutionWorkerExitDropV1,&xl6ExecutionFailuresOpsV1};return &policy;}
static bool jobCount(const void* raw,size_t* count)
{const ExecutorJob* job=raw;if(!job||!count||!job->references||job->active||job->cleared)return false;*count=job->references;return true;}
static bool jobTrace(const void* raw,xrtownershipvisitor visit,void* context)
{
    const ExecutorJob* job=raw;size_t count;if(!visit||!jobCount(raw,&count))return false;
    return (!job->callback||visit(xl6ClosureRuntimeV1Get()->Ownership(job->callback),context))&&
        (!job->boxed||visit(xrtValueOwnership(job->boxed),context))&&
        (!job->failures||visit(xl6ExecutionFailuresOwnershipV1(job->failures),context))&&
        (!job->body.error||visit(xrtErrorOwnership(job->body.error),context));
}
static const xrtownershipops jobOps={jobCount,jobTrace};
const xexecutordataownershipv1* xl6ExecutorClosureDataPolicyV1Get(void)
{static const xexecutordataownershipv1 policy={sizeof(policy),invoke,dispose,&jobOps};return &policy;}
static bool jobBusy(const void* raw)
{const ExecutorJob* job=raw;return job&&job->references&&!job->cleared&&job->active;}
const xrtownershipquiescencev1* xl6ExecutorClosureQuiescenceV1Get(void)
{static const xrtownershipquiescencev1 policy={sizeof(policy),&jobOps,jobBusy};return &policy;}
static bool jobHold(const void* raw)
{
    ExecutorJob* job=(ExecutorJob*)raw;xrtownershipscope mutation={0};begin(&job->gate,&mutation);
    bool ok=job->references&&job->references<SIZE_MAX&&!job->cleared;if(ok)++job->references;
    end(&job->gate,&mutation);return ok;
}
static bool jobClaim(const void* raw,const void* token)
{ExecutorJob* j=(ExecutorJob*)raw;if(!token||j->cleared||(j->claim&&j->claim!=token))return false;j->claim=token;return true;}
static void jobRestore(const void* raw,const void* token)
{ExecutorJob* j=(ExecutorJob*)raw;if(!token||j->claim!=token||j->cleared)abort();j->claim=NULL;}
static bool jobPrepared(const void* raw)
{const ExecutorJob* j=raw;return j->finished&&!j->active&&!j->callback&&!j->boxed&&!j->failures&&!j->body.error;}
static xrtownershipprepareresult jobPrepare(const void* raw,const void* token)
{
    ExecutorJob* j=(ExecutorJob*)raw;xrtownershipscope mutation={0};begin(&j->gate,&mutation);
    if(!token||j->claim!=token||j->cleared)abort();
    bool ready=jobPrepared(j);end(&j->gate,&mutation);return ready?XRT_OWNERSHIP_PREPARE_READY:XRT_OWNERSHIP_PREPARE_BUSY;
}
static void jobClear(const void* raw,const void* token)
{ExecutorJob* j=(ExecutorJob*)raw;if(!token||j->claim!=token||j->cleared||!jobPrepared(j))abort();j->cleared=true;}
const xrtownershipadapterv1* xl6ExecutorJobsOwnershipAdapterV1(xrtownershipref ref,const xrtownershippreparationv1** preparation)
{
    static const xrtownershipadapterv1 jobs={sizeof(jobs),jobHold,jobRelease,jobClaim,jobRestore,NULL,jobClear,NULL};
    static const xrtownershippreparationv1 policy={sizeof(policy),&jobs,jobPrepared,jobPrepare};
    size_t count;if(!preparation)return NULL;
    if(ref.Ops==&jobOps&&jobCount(ref.Data,&count)){*preparation=&policy;return &jobs;}
    return NULL;
}
