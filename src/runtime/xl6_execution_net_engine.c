#include "xl6_execution_resource_internal.h"
#include "xl6_execution_net_engine.h"
#include "xl6_detached_work.h"
#include "xl6_execution_worker_exit.h"
#include "xl6_concurrent_deadline.h"
#include "src/api/xl_foreign_error.h"

typedef XL6_ExecutionResourceV1 Resource;
static void work(xnetworker* worker,void* data)
{(void)worker;xl6DetachedWorkInvokeV1(data);}
static void timer(xnetworker* worker,uint64 id,xnetresult result,void* data)
{
    (void)worker;(void)id;
    if(result==XNET_RESULT_OK)xl6DetachedWorkInvokeV1(data);
    else if(result!=XNET_RESULT_CANCELLED&&result!=XNET_RESULT_CLOSED)xl6DetachedWorkFailV1(data,xrtTakeError());
}
static void workDrop(void* data){xl6DetachedWorkDropV1(data,NULL);}
const xnettaskownershipv1* xl6NetEngineWorkPolicyV1Get(void)
{static const xnettaskownershipv1 policy={sizeof(policy),work,workDrop,&xl6DetachedWorkOpsV1};return &policy;}
const xnettimerownershipv1* xl6NetEngineTimerPolicyV1Get(void)
{static const xnettimerownershipv1 policy={sizeof(policy),timer,workDrop,&xl6DetachedWorkOpsV1};return &policy;}
const xnetengineworkerexitownershipv1* xl6NetEngineExitPolicyV1Get(void)
{static const xnetengineworkerexitownershipv1 policy={sizeof(policy),xl6ExecutionWorkerExitV1,xl6ExecutionWorkerExitDropV1,&xl6ExecutionFailuresOpsV1};return &policy;}
static bool create(Resource* r,const XL6_ExecutionCreateOptionsV1* options)
{
    r->failures=xl6ExecutionFailuresCreateV1();
    if(r->failures){
        XL6_ExecutionFailuresV1* owned=xl6ExecutionFailuresRetainV1(r->failures);
        if(owned){
            r->engine=xrtNetEngineCreateOwnedExitV1(options->value.engine,owned,xl6NetEngineExitPolicyV1Get());
            if(!r->engine){xerror* error=xrtTakeError();xl6ExecutionFailuresReleaseV1(owned);xrtClearError();xrtSetErrorTake(error);}
        }
    }
    return r->engine!=NULL;
}
static bool rollback(Resource* r)
{if(r->engine&&!xrtNetEngineDestroy(r->engine))return false;r->engine=NULL;return true;}
static XL6_ModulePrepareResult retire(Resource* r)
{
    if(r->engine){
        xnetretireresult result=xrtNetEngineTryDestroy(r->engine);
        if(result==XNET_RETIRE_BUSY)return XL6_MODULE_PREPARE_BUSY;
        if(result!=XNET_RETIRE_READY)return XL6_MODULE_PREPARE_FAILED;
    }
    /* Native creator consumed only on READY. The source clears that physical
     * slot under its gate, independently of semantic callback/drop failure. */
    if(!xl6ExecutionFailuresUnreportedV1(r->failures))
        xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});
    return XL6_MODULE_PREPARE_READY;
}
static bool finish(Resource* r){return !r->engine||xrtNetEngineDestroy(r->engine);}
static void drop(Resource* r){xl6ExecutionFailuresReleaseV1(r->failures);r->failures=NULL;}
static bool trace(const Resource* r,xrtownershipvisitor visit,void* context)
{return (!r->engine||visit(xrtNetEngineOwnership(r->engine),context))&&
    (!r->failures||visit(xl6ExecutionFailuresOwnershipV1(r->failures),context));}
/* Logical closure is ordered by the shared metadata gate. The physical stop
 * is driven by explicit Wait or final-owner retirement, not by an unsafe
 * concurrent lifecycle call here. The Engine has no separate job-cancel API. */
static bool close(Resource* r){(void)r;return true;}
static bool present(const Resource* r){return r->engine!=NULL;}
const XL6_ExecutionFamilyV1* xl6ExecutionNetEngineFamilyV1Get(void)
{static const XL6_ExecutionFamilyV1 family={sizeof(family),XL6_EXECUTION_FAMILY_TAG_V1,XL6_EXECUTION_NET_ENGINE_V1,create,rollback,retire,finish,drop,trace,close,close,sizeof(Resource),present};return &family;}
Resource* xl6ExecutionNetEngineCreateV1(XL6_ExecutionSourceV1* source,const xnetengineconfig* config)
{
    XL6_ExecutionCreateOptionsV1 options={.kind=XL6_EXECUTION_NET_ENGINE_V1,.value.engine=config};
    return xl6ExecutionCreateInternalV1(source,xl6ExecutionNetEngineFamilyV1Get(),&options);
}
static bool enter(Resource* resource,bool submit,bool control,XL6_ExecutionScopeV1* scope)
{
    bool ok=control?xl6ExecutionResourceControlV1(resource,submit,scope):xl6ExecutionResourceEnterV1(resource,submit,scope);
    if(ok&&resource->kind!=XL6_EXECUTION_NET_ENGINE_V1){xl6ExecutionResourceLeaveV1(scope);xrtSetErrorKind(XERR_TYPE);return false;}
    if(ok&&!scope->engine){xl6ExecutionResourceLeaveV1(scope);xrtSetErrorKind(XERR_STATE);return false;}
    return ok;
}
bool xl6ExecutionNetEngineStartV1(Resource* resource)
{
    XL6_ExecutionScopeV1 scope={0};if(!enter(resource,true,true,&scope))return false;
    bool ok=xrtNetEngineStart(scope.engine);xl6ExecutionResourceLeaveV1(&scope);return ok;
}
bool xl6ExecutionNetEngineStopV1(Resource* resource)
{
    XL6_ExecutionScopeV1 scope={0};if(!enter(resource,false,true,&scope))return false;
    bool ok=xrtNetEngineStop(scope.engine)&&xl6ExecutionFailuresCheckV1(resource->failures);
    xl6ExecutionResourceLeaveV1(&scope);return ok;
}
bool xl6ExecutionNetEngineTryStopV1(Resource* resource,bool* ready)
{
    if(!ready){xrtSetErrorKind(XERR_ARGUMENT);return false;}*ready=false;
    XL6_ExecutionScopeV1 scope={0};if(!enter(resource,false,true,&scope))return false;
    xnetretireresult result=xrtNetEngineTryStop(scope.engine);*ready=result==XNET_RETIRE_READY;
    bool ok=result!=XNET_RETIRE_ERROR&&(!*ready||xl6ExecutionFailuresCheckV1(resource->failures));
    xl6ExecutionResourceLeaveV1(&scope);return ok;
}
bool xl6ExecutionNetEngineWaitV1(Resource* resource,int64_t ms,bool* ready)
{
    if(!ready){xrtSetErrorKind(XERR_ARGUMENT);return false;}*ready=false;
    xdeadline deadline;if(!xl6ConcurrentDeadline(ms,&deadline))return false;
    XL6_ExecutionScopeV1 scope={0};if(!enter(resource,false,true,&scope))return false;
    bool ok=false;
    if(!xl6ExecutionRequestedInternalV1(resource)||xrtNetEngineCurrent(scope.engine))xrtSetErrorKind(XERR_STATE);
    else for(;;){
        xnetretireresult result=xrtNetEngineTryStop(scope.engine);
        if(result==XNET_RETIRE_ERROR)break;
        if(result==XNET_RETIRE_READY){*ready=true;ok=xl6ExecutionFailuresCheckV1(resource->failures);break;}
        if(xrtDeadlineExpired(deadline)){ok=true;break;}
        xrtSleepUs(UINT64_C(1000));
    }
    xl6ExecutionResourceLeaveV1(&scope);return ok;
}
static void reject(XL6_DetachedWorkV1* work)
{xerror* error=xrtTakeError();xl6DetachedWorkAbortV1(work);xrtClearError();xrtSetErrorTake(error);}
bool xl6ExecutionNetEnginePostV1(Resource* resource,uint64_t affinity,XL6_Closure* callback)
{
    XL6_ExecutionScopeV1 scope={0};if(!enter(resource,true,false,&scope))return false;
    XL6_DetachedWorkV1* prepared=xl6DetachedWorkCreateV1(resource->failures,callback);
    bool ok=prepared&&xrtNetEnginePostOwnedV1(scope.engine,affinity,prepared,xl6NetEngineWorkPolicyV1Get());
    if(prepared&&!ok)reject(prepared);
    xl6ExecutionResourceLeaveV1(&scope);return ok;
}
bool xl6ExecutionNetEngineAfterV1(Resource* resource,uint64_t affinity,int64_t ms,XL6_Closure* callback,uint64_t* id)
{
    if(!id){xrtSetErrorKind(XERR_ARGUMENT);return false;}*id=0;
    if(ms<0){xrtSetErrorKind(XERR_RANGE);return false;}
    xdeadline deadline;if(!xl6ConcurrentDeadline(ms,&deadline))return false;
    XL6_ExecutionScopeV1 scope={0};if(!enter(resource,true,false,&scope))return false;
    XL6_DetachedWorkV1* prepared=xl6DetachedWorkCreateV1(resource->failures,callback);
    if(prepared)*id=xrtNetEngineScheduleOwnedV1(scope.engine,affinity,deadline,prepared,xl6NetEngineTimerPolicyV1Get());
    if(prepared&&!*id)reject(prepared);
    xl6ExecutionResourceLeaveV1(&scope);return *id!=0;
}
bool xl6ExecutionNetEngineCancelTimerV1(Resource* resource,uint64_t id)
{
    if(!id){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    XL6_ExecutionScopeV1 scope={0};if(!enter(resource,false,false,&scope))return false;
    bool ok=xrtNetEngineTimerCancel(scope.engine,id);xl6ExecutionResourceLeaveV1(&scope);return ok;
}
bool xl6ExecutionNetEngineCheckV1(Resource* resource)
{
    XL6_ExecutionScopeV1 scope={0};if(!enter(resource,false,false,&scope))return false;
    bool ok=xl6ExecutionFailuresCheckV1(resource->failures);xl6ExecutionResourceLeaveV1(&scope);return ok;
}
bool xl6ExecutionNetEngineFailuresV1(Resource* resource,XL6_ExecutionFailureSnapshotV1* output)
{
    XL6_ExecutionScopeV1 scope={0};if(!enter(resource,false,false,&scope))return false;
    bool ok=xl6ExecutionFailuresSnapshotV1(resource->failures,output);xl6ExecutionResourceLeaveV1(&scope);return ok;
}
bool xl6ExecutionNetEngineConfigV1(Resource* resource,xnetengineconfig* output,xnetbufpoolconfig* buffers)
{
    XL6_ExecutionScopeV1 scope={0};if(!enter(resource,false,false,&scope))return false;
    bool ok=xrtNetEngineGetConfig(scope.engine,output,buffers);xl6ExecutionResourceLeaveV1(&scope);return ok;
}
bool xl6ExecutionNetEngineBackendV1(Resource* resource,uint32_t worker,xnetportbackendinfo* output)
{
    XL6_ExecutionScopeV1 scope={0};if(!enter(resource,false,false,&scope))return false;
    bool ok=xrtNetEngineBackendInfo(scope.engine,worker,output);xl6ExecutionResourceLeaveV1(&scope);return ok;
}
bool xl6ExecutionNetEngineStatsV1(Resource* resource,xnetenginestats* output)
{
    XL6_ExecutionScopeV1 scope={0};if(!enter(resource,false,false,&scope))return false;
    bool ok=xrtNetEngineStats(scope.engine,output);xl6ExecutionResourceLeaveV1(&scope);return ok;
}
