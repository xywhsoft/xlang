#include "xl6_execution_resource_internal.h"
#include "xl6_concurrent_deadline.h"
#include "src/api/xl_foreign_error.h"
typedef XL6_ExecutionResourceV1 Resource;
typedef struct ExecutorResource {Resource base;xexecutorstats stats;} ExecutorResource;
_Static_assert(offsetof(ExecutorResource,base)==0,"Execution resource base must be first");
static bool create(Resource* r,const XL6_ExecutionCreateOptionsV1* options)
{
    r->failures=xl6ExecutionFailuresCreateV1();
    if(r->failures){
        XL6_ExecutionFailuresV1* owned=xl6ExecutionFailuresRetainV1(r->failures);
        if(owned){
            xexecutorconfig config={options->value.workers.threads,options->value.workers.limit,options->value.workers.stack};
            r->executor=xrtExecutorCreateOwnedExitV1(&config,owned,xl6ExecutorWorkerExitPolicyV1Get());
            if(!r->executor){xerror* error=xrtTakeError();xl6ExecutionFailuresReleaseV1(owned);xrtClearError();xrtSetErrorTake(error);}
        }
    }
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
    if(result!=XWAIT_OK||!xrtExecutorGet(r->executor,&((ExecutorResource*)r)->stats))return XL6_MODULE_PREPARE_FAILED;
    if(!xrtExecutorDestroy(r->executor))return XL6_MODULE_PREPARE_FAILED;
    /* Physical completion and semantic failure are independent. The source
     * consumes its actual native creator even when a detached callback failed,
     * and its enclosing checked cleanup scope latches the original outcome.
     * In particular, NULL is a valid first failed diagnostic, not success. */
    if(!xl6ExecutionFailuresUnreportedV1(r->failures))
        xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});
    return XL6_MODULE_PREPARE_READY;
}
static bool finish(Resource* r){return !r->executor||xrtExecutorDestroy(r->executor);}
static void drop(Resource* r){xl6ExecutionFailuresReleaseV1(r->failures);r->failures=NULL;}
static bool trace(const Resource* r,xrtownershipvisitor visit,void* context)
{return (!r->executor||visit(xrtExecutorOwnership(r->executor),context))&&
    (!r->failures||visit(xl6ExecutionFailuresOwnershipV1(r->failures),context));}
static bool cancel(Resource* r){return !r->executor||xrtExecutorCancel(r->executor);}
static bool close(Resource* r){return !r->executor||xrtExecutorClose(r->executor);}
static bool present(const Resource* r){return r->executor!=NULL;}
const XL6_ExecutionFamilyV1* xl6ExecutionExecutorFamilyV1Get(void)
{static const XL6_ExecutionFamilyV1 family={sizeof(family),XL6_EXECUTION_FAMILY_TAG_V1,XL6_EXECUTION_EXECUTOR_V1,create,rollback,retire,finish,drop,trace,close,cancel,sizeof(ExecutorResource),present};return &family;}
Resource* xl6ExecutionExecutorCreateV1(XL6_ExecutionSourceV1* s,uint32_t n,size_t q,size_t stack)
{
    XL6_ExecutionCreateOptionsV1 options={.kind=XL6_EXECUTION_EXECUTOR_V1,.value.workers={n,q,stack}};
    return xl6ExecutionCreateInternalV1(s,xl6ExecutionExecutorFamilyV1Get(),&options);
}
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
    bool ok=resource->kind==XL6_EXECUTION_EXECUTOR_V1&&xl6ExecutionFailuresCheckV1(resource->failures);
    if(resource->kind!=XL6_EXECUTION_EXECUTOR_V1)xrtSetErrorKind(XERR_TYPE);
    xl6ExecutionResourceLeaveV1(&scope);return ok;
}
bool xl6ExecutionExecutorFailuresV1(Resource* resource,XL6_ExecutionFailureSnapshotV1* snapshot)
{
    XL6_ExecutionScopeV1 scope={0};if(!xl6ExecutionResourceEnterV1(resource,false,&scope))return false;
    bool ok=resource->kind==XL6_EXECUTION_EXECUTOR_V1&&xl6ExecutionFailuresSnapshotV1(resource->failures,snapshot);
    if(resource->kind!=XL6_EXECUTION_EXECUTOR_V1)xrtSetErrorKind(XERR_TYPE);
    xl6ExecutionResourceLeaveV1(&scope);return ok;
}
bool xl6ExecutionExecutorStatsV1(Resource* resource,xexecutorstats* output)
{
    if(!output){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    XL6_ExecutionScopeV1 scope={0};if(!xl6ExecutionResourceEnterV1(resource,false,&scope))return false;
    bool ok=resource->kind==XL6_EXECUTION_EXECUTOR_V1;
    if(ok){if(scope.executor)ok=xrtExecutorGet(scope.executor,output);else *output=((ExecutorResource*)resource)->stats;}
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
            else if(waited==XWAIT_OK){*status=XL6_EXECUTOR_READY_V1;ok=xl6ExecutionFailuresCheckV1(resource->failures);}
        }
    }
    else {*status=XL6_EXECUTOR_READY_V1;ok=xl6ExecutionFailuresCheckV1(resource->failures);}
    xl6ExecutionResourceLeaveV1(&scope);return ok;
}
