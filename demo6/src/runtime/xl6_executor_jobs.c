#define XRT_MODULE_EXECUTOR
#include "lib/xrt/xrt.h"
#include "xl6_executor_jobs.h"
#include "xl6_concurrent_deadline.h"
#include "xl6_thread_exit.h"
#include "src/api/xl_foreign_error.h"
#include <stdlib.h>

struct XL6_ExecutorFailuresV1 {
    xatomic64 gate;
    size_t references;
    uint64_t count;
    xerror* error;
    const void* claim;
    bool saturated,cleared,unreported;
};
typedef struct ExecutorJob {
    xatomic64 gate;
    size_t references;
    XL6_Closure* callback;
    XL6_ExecutorFailuresV1* failures;
    xl6_error_result body;
    const void* claim;
    bool active,finished,cleared;
} ExecutorJob;
static const xrtownershipops failureOps,jobOps;
static void begin(xatomic64* gate,xrtownershipscope* scope)
{
    if(!xrtOwnershipMutationBegin(scope))abort();
    for(;;){uint64 expected=0;
        if(xrtAtomic64CompareExchange(gate,&expected,1,XMEMORY_ACQUIRE,XMEMORY_RELAXED))return;
        xrtThreadYield();}
}
static void end(xatomic64* gate,xrtownershipscope* scope)
{xrtAtomic64Store(gate,0,XMEMORY_RELEASE);if(!xrtOwnershipScopeEnd(scope))abort();}
XL6_ExecutorFailuresV1* xl6ExecutorFailuresCreateV1(void)
{
    XL6_ExecutorFailuresV1* state=xrtCalloc(1,sizeof(*state));
    if(state)state->references=1;
    return state;
}
XL6_ExecutorFailuresV1* xl6ExecutorFailuresRetainV1(XL6_ExecutorFailuresV1* state)
{
    if(!state){xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    xrtownershipscope mutation={0};begin(&state->gate,&mutation);
    bool ok=state->references&&state->references<SIZE_MAX&&!state->cleared;
    if(ok)++state->references;
    end(&state->gate,&mutation);if(!ok)xrtSetErrorKind(XERR_STATE);return ok?state:NULL;
}
void xl6ExecutorFailuresReleaseV1(XL6_ExecutorFailuresV1* state)
{
    if(!state)return;
    xrtownershipscope mutation={0};begin(&state->gate,&mutation);
    if(!state->references)abort();
    bool last=--state->references==0;
    if(last&&state->claim&&!state->cleared)abort();
    xerror* error=last?state->error:NULL;if(last)state->error=NULL;
    end(&state->gate,&mutation);
    if(last){xrtErrorFree(error);xrtFree(state);}
}
static void record(XL6_ExecutorFailuresV1* state,xl6_error_result result)
{
    if(result.ok){xrtErrorFree(result.error);return;}
    xrtownershipscope mutation={0};begin(&state->gate,&mutation);
    if(!state->references||state->cleared)abort();
    if(!state->count){state->error=result.error;result.error=NULL;}
    if(state->count==UINT64_MAX)state->saturated=true;else ++state->count;
    state->unreported=true;
    end(&state->gate,&mutation);xrtErrorFree(result.error);
}
bool xl6ExecutorFailuresSnapshotV1(XL6_ExecutorFailuresV1* state,XL6_ExecutorFailureSnapshotV1* output)
{
    if(!state||!output||output->size!=sizeof(*output)){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    xrtownershipscope mutation={0};begin(&state->gate,&mutation);
    bool ok=state->references&&!state->cleared;
    if(ok)*output=(XL6_ExecutorFailureSnapshotV1){sizeof(*output),state->count,state->saturated};
    end(&state->gate,&mutation);if(!ok)xrtSetErrorKind(XERR_STATE);return ok;
}
static bool checkFailure(XL6_ExecutorFailuresV1* state,bool retirement)
{
    if(!state){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    xrtownershipscope mutation={0};begin(&state->gate,&mutation);
    bool valid=state->references&&!state->cleared;
    bool pending=valid&&state->count&&(!retirement||state->unreported),ok=valid&&!pending;
    xerror* error=pending?xrtErrorRef(state->error):NULL;
    if(pending&&!retirement)state->unreported=false;
    end(&state->gate,&mutation);
    if(!valid){xrtSetErrorKind(XERR_STATE);return false;}
    if(!ok)xrtSetErrorTake(error);
    return ok;
}
bool xl6ExecutorThrowIfFailedV1(XL6_ExecutorFailuresV1* state){return checkFailure(state,false);}
bool xl6ExecutorUnreportedCheckV1(XL6_ExecutorFailuresV1* state){return checkFailure(state,true);}
static void jobRelease(const void* raw)
{
    ExecutorJob* job=(ExecutorJob*)raw;xrtownershipscope mutation={0};begin(&job->gate,&mutation);
    if(!job->references)abort();
    bool last=--job->references==0;
    if(last&&(job->active||!job->finished||job->callback||job->failures||job->body.error||
        (job->claim&&!job->cleared)))abort();
    end(&job->gate,&mutation);if(last)xrtFree(job);
}
static void invoke(void* raw)
{
    ExecutorJob* job=raw;xrtownershipscope mutation={0};begin(&job->gate,&mutation);
    if(job->active||job->finished||!job->callback||job->cleared)abort();
    job->active=true;XL6_Closure* callback=job->callback;end(&job->gate,&mutation);
    xerror* previous=xrtTakeError();xl6_error_scope work={0};bool begun=xl6ErrorScopeBegin(&work);
    xl6_error_result result={false,NULL};
    if(begun){
        XL6_ClosureScope scope={0};const XL6_ClosureRuntimeV1* api=xl6ClosureRuntimeV1Get();
        api->Enter(&scope,callback);
        if(!((bool (*)(void*))callback->invoke)(callback->environment))xl6ErrorScopeCaptureFailure(&work);
        api->Leave(&scope);result=xl6ErrorScopeEnd(&work);
    }else result.error=xrtTakeError();
    begin(&job->gate,&mutation);job->body=result;end(&job->gate,&mutation);
    xrtClearError();xrtSetErrorTake(previous);
    /* Remain BUSY through Drop; no inspection of half-finished captures. */
}
static void dispose(void* raw,void* context)
{
    if(context)abort();
    ExecutorJob* job=raw;xrtownershipscope mutation={0};begin(&job->gate,&mutation);
    if(job->finished||job->cleared||!job->callback||!job->failures)abort();
    job->active=true;XL6_Closure* callback=job->callback;job->callback=NULL;
    XL6_ExecutorFailuresV1* state=job->failures;job->failures=NULL;
    xl6_error_result body=job->body;job->body=(xl6_error_result){true,NULL};end(&job->gate,&mutation);
    xerror* previous=xrtTakeError();xl6_error_scope cleanup={0};bool begun=xl6ErrorScopeBegin(&cleanup);
    xl6_error_result result={false,NULL};
    if(begun){
        if(!body.ok){xrtSetErrorTake(body.error);body.error=NULL;xl6ErrorScopeCaptureFailure(&cleanup);}
        xl6ClosureRuntimeV1Get()->Release(callback);result=xl6ErrorScopeEnd(&cleanup);
    }else{
        result.error=xrtTakeError();
        if(!body.ok){xrtErrorFree(result.error);result=body;body.error=NULL;}
        xl6ClosureRuntimeV1Get()->Release(callback);
    }
    xrtErrorFree(body.error);record(state,result);xl6ExecutorFailuresReleaseV1(state);
    begin(&job->gate,&mutation);job->active=false;job->finished=true;end(&job->gate,&mutation);
    jobRelease(job);xrtClearError();xrtSetErrorTake(previous);
}
static ExecutorJob* prepare(XL6_ExecutorFailuresV1* state,XL6_Closure* callback)
{
    if(!callback||!callback->invoke||callback->call_parameter_count||
        !xl6ClosureOwnershipAdapterV1(xl6ClosureRuntimeV1Get()->Ownership(callback))){xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    ExecutorJob* job=xrtCalloc(1,sizeof(*job));if(!job)return NULL;
    job->references=1;job->body.ok=true;job->callback=xl6ClosureRuntimeV1Get()->Retain(callback);
    job->failures=xl6ExecutorFailuresRetainV1(state);
    if(!job->callback||!job->failures){
        xerror* error=xrtTakeError();xl6ClosureRuntimeV1Get()->Release(job->callback);
        xl6ExecutorFailuresReleaseV1(job->failures);xrtFree(job);xrtClearError();xrtSetErrorTake(error);return NULL;
    }
    return job;
}
/* Rejection owns PREPARED records, not accepted work. Do not execute dispose:
 * rolling back a borrowed capture is not a failed/cancelled executor job. */
static void abortPrepared(ExecutorJob* job)
{
    XL6_Closure* callback=job->callback;XL6_ExecutorFailuresV1* state=job->failures;
    xrtownershipscope mutation={0};begin(&job->gate,&mutation);
    job->callback=NULL;job->failures=NULL;job->finished=true;end(&job->gate,&mutation);
    xl6ClosureRuntimeV1Get()->Release(callback);xl6ExecutorFailuresReleaseV1(state);jobRelease(job);
}
bool xl6ExecutorSubmitClosuresV1(xexecutor* executor,XL6_ExecutorFailuresV1* state,XL6_Closure* const* callbacks,size_t count)
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
bool xl6ExecutorSubmitClosureV1(xexecutor* executor,XL6_ExecutorFailuresV1* state,XL6_Closure* callback)
{
    if(!executor||!state){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    ExecutorJob* job=prepare(state,callback);if(!job)return false;
    bool accepted=xrtExecutorSubmitOwnedV1(executor,job,xl6ExecutorClosureDataPolicyV1Get());
    if(!accepted){xerror* error=xrtTakeError();abortPrepared(job);xrtClearError();xrtSetErrorTake(error);}return accepted;
}
bool xl6ExecutorWaitV1(xexecutor* executor,XL6_ExecutorFailuresV1* state,int64_t milliseconds,XL6_ExecutorWaitStatusV1* status)
{
    if(status)*status=XL6_EXECUTOR_TIMEOUT_V1;
    if(!status||!state||!executor){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    xdeadline deadline;
    if(!xl6ConcurrentDeadline(milliseconds,&deadline))return false;
    xwaitresult result=xrtExecutorWaitUntil(executor,deadline);
    if(result==XWAIT_TIMEOUT)return true;
    if(result!=XWAIT_OK)return false;
    *status=XL6_EXECUTOR_READY_V1;return xl6ExecutorThrowIfFailedV1(state);
}
static bool failuresCount(const void* raw,size_t* count)
{const XL6_ExecutorFailuresV1* state=raw;if(!state||!count||!state->references||state->cleared)return false;*count=state->references;return true;}
static bool failuresTrace(const void* raw,xrtownershipvisitor visit,void* context)
{const XL6_ExecutorFailuresV1* state=raw;size_t count;return visit&&failuresCount(raw,&count)&&(!state->error||visit(xrtErrorOwnership(state->error),context));}
static const xrtownershipops failureOps={failuresCount,failuresTrace};
static void workerExit(void* raw)
{
    XL6_ExecutorFailuresV1* state=raw;
    xerror* previous=xrtTakeError();xl6_error_scope cleanup={0};xl6_error_result outcome={false,NULL};
    if(xl6ErrorScopeBegin(&cleanup)){
        if(!xl6ThreadExitCurrentV1())xl6ErrorScopeCaptureFailure(&cleanup);
        outcome=xl6ErrorScopeEnd(&cleanup);
    }else outcome.error=xrtTakeError();
    record(state,outcome);xrtClearError();xrtSetErrorTake(previous);
}
static void workerExitDrop(void* raw,void* context)
{if(context)abort();xl6ExecutorFailuresReleaseV1(raw);}
const xexecutorworkerexitownershipv1* xl6ExecutorWorkerExitPolicyV1Get(void)
{static const xexecutorworkerexitownershipv1 policy={sizeof(policy),workerExit,workerExitDrop,&failureOps};return &policy;}
xrtownershipref xl6ExecutorFailuresOwnershipV1(const XL6_ExecutorFailuresV1* state)
{return (xrtownershipref){state,state?&failureOps:NULL};}
static bool jobCount(const void* raw,size_t* count)
{const ExecutorJob* job=raw;if(!job||!count||!job->references||job->active||job->cleared)return false;*count=job->references;return true;}
static bool jobTrace(const void* raw,xrtownershipvisitor visit,void* context)
{
    const ExecutorJob* job=raw;size_t count;if(!visit||!jobCount(raw,&count))return false;
    return (!job->callback||visit(xl6ClosureRuntimeV1Get()->Ownership(job->callback),context))&&
        (!job->failures||visit(xl6ExecutorFailuresOwnershipV1(job->failures),context))&&
        (!job->body.error||visit(xrtErrorOwnership(job->body.error),context));
}
static const xrtownershipops jobOps={jobCount,jobTrace};
const xexecutordataownershipv1* xl6ExecutorClosureDataPolicyV1Get(void)
{static const xexecutordataownershipv1 policy={sizeof(policy),invoke,dispose,&jobOps};return &policy;}
static bool jobBusy(const void* raw)
{const ExecutorJob* job=raw;return job&&job->references&&!job->cleared&&job->active;}
const xrtownershipquiescencev1* xl6ExecutorClosureQuiescenceV1Get(void)
{static const xrtownershipquiescencev1 policy={sizeof(policy),&jobOps,jobBusy};return &policy;}
static bool failuresHold(const void* raw){return xl6ExecutorFailuresRetainV1((XL6_ExecutorFailuresV1*)raw)!=NULL;}
static void failuresDrop(const void* raw){xl6ExecutorFailuresReleaseV1((XL6_ExecutorFailuresV1*)raw);}
static bool failuresClaim(const void* raw,const void* token)
{XL6_ExecutorFailuresV1* s=(XL6_ExecutorFailuresV1*)raw;if(!token||s->cleared||(s->claim&&s->claim!=token))return false;s->claim=token;return true;}
static void failuresRestore(const void* raw,const void* token)
{XL6_ExecutorFailuresV1* s=(XL6_ExecutorFailuresV1*)raw;if(!token||s->claim!=token||s->cleared)abort();s->claim=NULL;}
static void failuresClear(const void* raw,const void* token)
{XL6_ExecutorFailuresV1* s=(XL6_ExecutorFailuresV1*)raw;if(!token||s->claim!=token||s->cleared)abort();s->cleared=true;}
static bool failuresFinish(const void* raw,const void* token)
{
    XL6_ExecutorFailuresV1* s=(XL6_ExecutorFailuresV1*)raw;xrtownershipscope mutation={0};begin(&s->gate,&mutation);
    if(!token||s->claim!=token||!s->cleared)abort();
    bool pending=s->unreported;xerror* error=pending?xrtErrorRef(s->error):NULL;s->unreported=false;end(&s->gate,&mutation);
    /* The complete graph has prepared ALL native work before Clear. Report
     * unobserved failures before plan pins disappear. A failed physical duty
     * can be retried without repeating this notification; the host retains
     * its checked first-failure outcome independently of physical readiness. */
    if(pending)xrtSetErrorTake(error);
    return !pending;
}
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
{const ExecutorJob* j=raw;return j->finished&&!j->active&&!j->callback&&!j->failures&&!j->body.error;}
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
    static const xrtownershipadapterv1 failures={sizeof(failures),failuresHold,failuresDrop,failuresClaim,failuresRestore,NULL,failuresClear,failuresFinish};
    static const xrtownershipadapterv1 jobs={sizeof(jobs),jobHold,jobRelease,jobClaim,jobRestore,NULL,jobClear,NULL};
    static const xrtownershippreparationv1 policy={sizeof(policy),&jobs,jobPrepared,jobPrepare};
    size_t count;if(!preparation)return NULL;
    if(ref.Ops==&failureOps&&failuresCount(ref.Data,&count)){*preparation=NULL;return &failures;}
    if(ref.Ops==&jobOps&&jobCount(ref.Data,&count)){*preparation=&policy;return &jobs;}
    return NULL;
}
