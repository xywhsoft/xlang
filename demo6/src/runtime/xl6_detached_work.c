#define XRT_MODULE_MEMORY
#define XRT_MODULE_ATOMIC
#define XRT_MODULE_THREAD
#define XRUNTIME_MODULE_RUNTIME_CALL
#define XRUNTIME_MODULE_RUNTIME_VALUE_CALLABLE
#include "lib/xruntime/xruntime.h"
#include "xl6_detached_work.h"
#include "src/api/xl_foreign_error.h"
#include <stdlib.h>

struct XL6_DetachedWorkV1 {
    xatomic64 gate;
    size_t references;
    XL6_Closure* callback;
    xvalue* boxed;
    XL6_ExecutionFailuresV1* failures;
    xl6_error_result body;
    const void* claim;
    bool active,finished,cleared;
};
const xrtownershipops xl6DetachedWorkOpsV1;
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
    XL6_DetachedWorkV1* job=(XL6_DetachedWorkV1*)raw;xrtownershipscope mutation={0};begin(&job->gate,&mutation);
    if(!job->references)abort();
    bool last=--job->references==0;
    if(last&&(job->active||!job->finished||job->callback||job->boxed||job->failures||job->body.error||
        (job->claim&&!job->cleared)))abort();
    end(&job->gate,&mutation);if(last)xrtFree(job);
}
void xl6DetachedWorkInvokeV1(void* raw)
{
    XL6_DetachedWorkV1* job=raw;xrtownershipscope mutation={0};begin(&job->gate,&mutation);
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
void xl6DetachedWorkDropV1(void* raw,void* context)
{
    if(context)abort();
    XL6_DetachedWorkV1* job=raw;xrtownershipscope mutation={0};begin(&job->gate,&mutation);
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
XL6_DetachedWorkV1* xl6DetachedWorkCreateV1(XL6_ExecutionFailuresV1* state,XL6_Closure* callback)
{
    if(!callback||!callback->invoke||callback->call_parameter_count||
        !xl6ClosureOwnershipAdapterV1(xl6ClosureRuntimeV1Get()->Ownership(callback))){xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    XL6_DetachedWorkV1* job=xrtCalloc(1,sizeof(*job));if(!job)return NULL;
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
void xl6DetachedWorkAbortV1(XL6_DetachedWorkV1* job)
{
    XL6_Closure* callback=job->callback;xvalue* boxed=job->boxed;XL6_ExecutionFailuresV1* state=job->failures;
    xrtownershipscope mutation={0};begin(&job->gate,&mutation);
    job->callback=NULL;job->boxed=NULL;job->failures=NULL;job->finished=true;end(&job->gate,&mutation);
    xl6ClosureRuntimeV1Get()->Release(callback);xrtValueRelease(boxed);xl6ExecutionFailuresReleaseV1(state);jobRelease(job);
}
XL6_DetachedWorkV1* xl6DetachedWorkCreateBoxedV1(XL6_ExecutionFailuresV1* state,const xvalue* boxed,uint64_t signature)
{
    if(!boxed||!signature||xrtValueTypeId(boxed)!=signature||!xl6ClosureBoxAdmittedV1(boxed)){
        xrtSetErrorKind(XERR_TYPE);return NULL;}
    XL6_DetachedWorkV1* job=xrtCalloc(1,sizeof(*job));if(!job)return NULL;
    job->references=1;job->body.ok=true;job->boxed=xrtValueRetain(boxed);
    job->failures=xl6ExecutionFailuresRetainV1(state);
    if(!job->boxed||!job->failures){
        xerror* error=xrtTakeError();xrtValueRelease(job->boxed);xl6ExecutionFailuresReleaseV1(job->failures);
        xrtFree(job);xrtClearError();xrtSetErrorTake(error);return NULL;
    }
    return job;
}
static bool jobCount(const void* raw,size_t* count)
{const XL6_DetachedWorkV1* job=raw;if(!job||!count||!job->references||job->active||job->cleared)return false;*count=job->references;return true;}
static bool jobTrace(const void* raw,xrtownershipvisitor visit,void* context)
{
    const XL6_DetachedWorkV1* job=raw;size_t count;if(!visit||!jobCount(raw,&count))return false;
    return (!job->callback||visit(xl6ClosureRuntimeV1Get()->Ownership(job->callback),context))&&
        (!job->boxed||visit(xrtValueOwnership(job->boxed),context))&&
        (!job->failures||visit(xl6ExecutionFailuresOwnershipV1(job->failures),context))&&
        (!job->body.error||visit(xrtErrorOwnership(job->body.error),context));
}
const xrtownershipops xl6DetachedWorkOpsV1={jobCount,jobTrace};
const xrtownershipops* xl6DetachedWorkOpsV1Get(void){return &xl6DetachedWorkOpsV1;}
static bool jobBusy(const void* raw)
{const XL6_DetachedWorkV1* job=raw;return job&&job->references&&!job->cleared&&job->active;}
const xrtownershipquiescencev1* xl6DetachedWorkQuiescenceV1Get(void)
{static const xrtownershipquiescencev1 policy={sizeof(policy),&xl6DetachedWorkOpsV1,jobBusy};return &policy;}
static bool jobHold(const void* raw)
{
    XL6_DetachedWorkV1* job=(XL6_DetachedWorkV1*)raw;xrtownershipscope mutation={0};begin(&job->gate,&mutation);
    bool ok=job->references&&job->references<SIZE_MAX&&!job->cleared;if(ok)++job->references;
    end(&job->gate,&mutation);return ok;
}
static bool jobClaim(const void* raw,const void* token)
{XL6_DetachedWorkV1* j=(XL6_DetachedWorkV1*)raw;if(!token||j->cleared||(j->claim&&j->claim!=token))return false;j->claim=token;return true;}
static void jobRestore(const void* raw,const void* token)
{XL6_DetachedWorkV1* j=(XL6_DetachedWorkV1*)raw;if(!token||j->claim!=token||j->cleared)abort();j->claim=NULL;}
static bool jobPrepared(const void* raw)
{const XL6_DetachedWorkV1* j=raw;return j->finished&&!j->active&&!j->callback&&!j->boxed&&!j->failures&&!j->body.error;}
static xrtownershipprepareresult jobPrepare(const void* raw,const void* token)
{
    XL6_DetachedWorkV1* j=(XL6_DetachedWorkV1*)raw;xrtownershipscope mutation={0};begin(&j->gate,&mutation);
    if(!token||j->claim!=token||j->cleared)abort();
    bool ready=jobPrepared(j);end(&j->gate,&mutation);return ready?XRT_OWNERSHIP_PREPARE_READY:XRT_OWNERSHIP_PREPARE_BUSY;
}
static void jobClear(const void* raw,const void* token)
{XL6_DetachedWorkV1* j=(XL6_DetachedWorkV1*)raw;if(!token||j->claim!=token||j->cleared||!jobPrepared(j))abort();j->cleared=true;}
const xrtownershipadapterv1* xl6DetachedWorkOwnershipAdapterV1(xrtownershipref ref,const xrtownershippreparationv1** preparation)
{
    static const xrtownershipadapterv1 jobs={sizeof(jobs),jobHold,jobRelease,jobClaim,jobRestore,NULL,jobClear,NULL};
    static const xrtownershippreparationv1 policy={sizeof(policy),&jobs,jobPrepared,jobPrepare};
    size_t count;if(!preparation)return NULL;
    if(ref.Ops==&xl6DetachedWorkOpsV1&&jobCount(ref.Data,&count)){*preparation=&policy;return &jobs;}
    return NULL;
}
