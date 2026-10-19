#include "xl6_execution_resource_internal.h"
#include "xl6_concurrent_deadline.h"
#include "src/api/xl_foreign_error.h"
typedef XL6_ExecutionResourceV1 Resource;
static bool create(Resource* r,uint32_t threads,size_t limit,size_t stack)
{
    r->failures=xl6ExecutorFailuresCreateV1();
    if(r->failures){xexecutorconfig config={threads,limit,stack};r->executor=xrtExecutorCreate(&config);}
    return r->executor!=NULL;
}
static bool rollback(Resource* r)
{if(r->executor&&!xrtExecutorDestroy(r->executor))return false;r->executor=NULL;return true;}
static XL6_ModulePrepareResult retire(Resource* r)
{
    if(!r->executor)return XL6_MODULE_PREPARE_READY;
    if(!xrtExecutorClose(r->executor))return XL6_MODULE_PREPARE_FAILED;
    xwaitresult result=xrtExecutorWaitFor(r->executor,0);
    if(result==XWAIT_TIMEOUT)return XL6_MODULE_PREPARE_BUSY;
    if(result!=XWAIT_OK||!xrtExecutorGet(r->executor,&r->stats))return XL6_MODULE_PREPARE_FAILED;
    if(!xrtExecutorDestroy(r->executor))return XL6_MODULE_PREPARE_FAILED;
    /* Physical completion and semantic failure are independent. The source
     * consumes its actual native creator even when a detached callback failed,
     * and its enclosing checked cleanup scope latches the original outcome.
     * In particular, NULL is a valid first failed diagnostic, not success. */
    if(!xl6ExecutorThrowIfFailedV1(r->failures))
        xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});
    return XL6_MODULE_PREPARE_READY;
}
static bool finish(Resource* r){return !r->executor||xrtExecutorDestroy(r->executor);}
static void drop(Resource* r){xl6ExecutorFailuresReleaseV1(r->failures);r->failures=NULL;}
static bool trace(const Resource* r,xrtownershipvisitor visit,void* context)
{return (!r->executor||visit(xrtExecutorOwnership(r->executor),context))&&
    (!r->failures||visit(xl6ExecutorFailuresOwnershipV1(r->failures),context));}
static bool cancel(Resource* r){return !r->executor||xrtExecutorCancel(r->executor);}
static bool close(Resource* r){return !r->executor||xrtExecutorClose(r->executor);}
const XL6_ExecutionFamilyV1* xl6ExecutionExecutorFamilyV1Get(void)
{static const XL6_ExecutionFamilyV1 family={sizeof(family),XL6_EXECUTION_FAMILY_TAG_V1,XL6_EXECUTION_EXECUTOR_V1,create,rollback,retire,finish,drop,trace,close,cancel};return &family;}
Resource* xl6ExecutionExecutorCreateV1(XL6_ExecutionSourceV1* s,uint32_t n,size_t q,size_t stack)
{return xl6ExecutionCreateInternalV1(s,xl6ExecutionExecutorFamilyV1Get(),n,q,stack);}
bool xl6ExecutionExecutorSubmitBatchV1(Resource* resource,XL6_Closure* const* callbacks,size_t count)
{
    XL6_ExecutionScopeV1 scope={0};if(!xl6ExecutionResourceEnterV1(resource,true,&scope))return false;
    bool ok=scope.executor&&xl6ExecutorSubmitClosuresV1(scope.executor,resource->failures,callbacks,count);
    if(!scope.executor)xrtSetErrorKind(XERR_TYPE);
    xl6ExecutionResourceLeaveV1(&scope);return ok;
}
bool xl6ExecutionExecutorSubmitV1(Resource* resource,XL6_Closure* callback)
{
    XL6_ExecutionScopeV1 scope={0};if(!xl6ExecutionResourceEnterV1(resource,true,&scope))return false;
    bool ok=scope.executor&&xl6ExecutorSubmitClosureV1(scope.executor,resource->failures,callback);
    if(!scope.executor)xrtSetErrorKind(XERR_TYPE);
    xl6ExecutionResourceLeaveV1(&scope);return ok;
}
bool xl6ExecutionExecutorThrowIfFailedV1(Resource* resource)
{
    XL6_ExecutionScopeV1 scope={0};if(!xl6ExecutionResourceEnterV1(resource,false,&scope))return false;
    bool ok=resource->kind==XL6_EXECUTION_EXECUTOR_V1&&xl6ExecutorThrowIfFailedV1(resource->failures);
    if(resource->kind!=XL6_EXECUTION_EXECUTOR_V1)xrtSetErrorKind(XERR_TYPE);
    xl6ExecutionResourceLeaveV1(&scope);return ok;
}
bool xl6ExecutionExecutorFailuresV1(Resource* resource,XL6_ExecutorFailureSnapshotV1* snapshot)
{
    XL6_ExecutionScopeV1 scope={0};if(!xl6ExecutionResourceEnterV1(resource,false,&scope))return false;
    bool ok=resource->kind==XL6_EXECUTION_EXECUTOR_V1&&xl6ExecutorFailuresSnapshotV1(resource->failures,snapshot);
    if(resource->kind!=XL6_EXECUTION_EXECUTOR_V1)xrtSetErrorKind(XERR_TYPE);
    xl6ExecutionResourceLeaveV1(&scope);return ok;
}
bool xl6ExecutionExecutorStatsV1(Resource* resource,xexecutorstats* output)
{
    if(!output){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    XL6_ExecutionScopeV1 scope={0};if(!xl6ExecutionResourceEnterV1(resource,false,&scope))return false;
    bool ok=resource->kind==XL6_EXECUTION_EXECUTOR_V1;
    if(ok){if(scope.executor)ok=xrtExecutorGet(scope.executor,output);else *output=resource->stats;}
    else xrtSetErrorKind(XERR_TYPE);
    xl6ExecutionResourceLeaveV1(&scope);return ok;
}
bool xl6ExecutionExecutorWaitV1(Resource* resource,int64_t ms,XL6_ExecutorWaitStatusV1* status)
{
    if(status)*status=XL6_EXECUTOR_TIMEOUT_V1;
    if(!status){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    xdeadline deadline;if(!xl6ConcurrentDeadline(ms,&deadline))return false;
    XL6_ExecutionScopeV1 scope={0};if(!xl6ExecutionResourceEnterV1(resource,false,&scope))return false;
    bool closed=xl6ExecutionRequestedInternalV1(resource);
    bool ok=false;
    if(resource->kind!=XL6_EXECUTION_EXECUTOR_V1)xrtSetErrorKind(XERR_TYPE);
    else if(!closed)xrtSetErrorKind(XERR_STATE);
    else if(scope.executor){
        if(xrtExecutorClose(scope.executor)){
            xwaitresult waited=xrtExecutorWaitUntil(scope.executor,deadline);
            if(waited==XWAIT_TIMEOUT)ok=true;
            else if(waited==XWAIT_OK){*status=XL6_EXECUTOR_READY_V1;ok=xl6ExecutorThrowIfFailedV1(resource->failures);}
        }
    }
    else {*status=XL6_EXECUTOR_READY_V1;ok=xl6ExecutorThrowIfFailedV1(resource->failures);}
    xl6ExecutionResourceLeaveV1(&scope);return ok;
}
