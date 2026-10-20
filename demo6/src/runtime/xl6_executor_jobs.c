#define XRT_MODULE_EXECUTOR
#define XRUNTIME_MODULE_RUNTIME_CALL
#define XRUNTIME_MODULE_RUNTIME_VALUE_CALLABLE
#include "lib/xruntime/xruntime.h"
#include "xl6_executor_jobs.h"
#include "xl6_detached_work.h"
#include "xl6_concurrent_deadline.h"
#include "xl6_execution_worker_exit.h"
#include "src/api/xl_foreign_error.h"
#include <stdlib.h>

bool xl6ExecutorPrepareBoxedBatchV1(XL6_ExecutionFailuresV1* state,xvalue* const* values,
    size_t count,uint64_t signature,XL6_ExecutorPreparedBatchV1* output)
{
    if(!state||!signature||!output||output->items||output->count||(count&&!values)||
        count>SIZE_MAX/sizeof(xexecutorowneditemv1)){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    if(!count)return true;
    xl6_error_scope scope={0};if(!xl6ErrorScopeBegin(&scope))return false;
    xexecutorowneditemv1* items=xrtCalloc(count,sizeof(*items));size_t prepared=0;
    if(items)for(;prepared<count;++prepared){
        XL6_DetachedWorkV1* job=xl6DetachedWorkCreateBoxedV1(state,values[prepared],signature);if(!job)break;
        items[prepared]=(xexecutorowneditemv1){job,xl6ExecutorClosureDataPolicyV1Get()};
    }
    bool ok=items&&prepared==count&&!scope.failed&&!xrtGetError();
    if(!ok){
        xl6ErrorScopeCaptureFailure(&scope);
        for(size_t i=prepared;i>0;--i)xl6DetachedWorkAbortV1(items[i-1].Data);
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
    if(!accepted)for(size_t i=batch->count;i>0;--i)xl6DetachedWorkAbortV1(batch->items[i-1].Data);
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
        XL6_DetachedWorkV1* job=xl6DetachedWorkCreateV1(state,callbacks[prepared]);if(!job)break;
        items[prepared]=(xexecutorowneditemv1){job,xl6ExecutorClosureDataPolicyV1Get()};
    }
    bool accepted=prepared==count&&xrtExecutorSubmitOwnedBatchV1(executor,items,count);
    xerror* error=accepted?NULL:xrtTakeError();
    if(!accepted)for(size_t i=prepared;i>0;--i)xl6DetachedWorkAbortV1(items[i-1].Data);
    xrtFree(items);if(!accepted){xrtClearError();xrtSetErrorTake(error);}return accepted;
}
bool xl6ExecutorSubmitClosureV1(xexecutor* executor,XL6_ExecutionFailuresV1* state,XL6_Closure* callback)
{
    if(!executor||!state){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    XL6_DetachedWorkV1* job=xl6DetachedWorkCreateV1(state,callback);if(!job)return false;
    bool accepted=xrtExecutorSubmitOwnedV1(executor,job,xl6ExecutorClosureDataPolicyV1Get());
    if(!accepted){xerror* error=xrtTakeError();xl6DetachedWorkAbortV1(job);xrtClearError();xrtSetErrorTake(error);}return accepted;
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
const xexecutordataownershipv1* xl6ExecutorClosureDataPolicyV1Get(void)
{
    static const xexecutordataownershipv1 policy={sizeof(policy),xl6DetachedWorkInvokeV1,xl6DetachedWorkDropV1,&xl6DetachedWorkOpsV1};
    return &policy;
}
const xrtownershipquiescencev1* xl6ExecutorClosureQuiescenceV1Get(void)
{return xl6DetachedWorkQuiescenceV1Get();}
const xrtownershipadapterv1* xl6ExecutorJobsOwnershipAdapterV1(xrtownershipref ref,const xrtownershippreparationv1** prep)
{return xl6DetachedWorkOwnershipAdapterV1(ref,prep);}
