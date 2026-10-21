#include "xl6_execution_resource_internal.h"
#include "xl6_task_pool_exit.h"
#include "src/api/xl_foreign_error.h"
typedef XL6_ExecutionResourceV1 Resource;
static bool create(Resource* r,const XL6_ExecutionCreateOptionsV1* options)
{
    r->failures=xl6ExecutionFailuresCreateV1();
    XL6_TaskPoolConfig config={options->value.workers.threads,options->value.workers.limit,options->value.workers.stack};
    if(r->failures)r->pool=xl6TaskPoolCreateManagedV1(&config,r->failures);
    return r->pool!=NULL;
}
static bool rollback(Resource* r)
{if(r->pool&&!xl6TaskPoolControlV1Get()->Destroy(r->pool))return false;r->pool=NULL;return true;}
static XL6_ModulePrepareResult retire(Resource* r)
{
    XL6_ModulePrepareResult result=xl6TaskPoolControlV1Get()->TryQuiesce(r->pool);
    if(result!=XL6_MODULE_PREPARE_READY)return result;
    if(!xl6TaskPoolControlV1Get()->Destroy(r->pool))return XL6_MODULE_PREPARE_FAILED;
    if(!xl6ExecutionFailuresUnreportedV1(r->failures))
        xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});
    return XL6_MODULE_PREPARE_READY;
}
static bool finish(Resource* r){return !r->pool||xl6TaskPoolReleaseClearedOwnerV1(r->pool);}
static void drop(Resource* r){xl6ExecutionFailuresReleaseV1(r->failures);r->failures=NULL;}
static bool trace(const Resource* r,xrtownershipvisitor visit,void* context)
{return (!r->pool||visit(xl6TaskPoolOwnership(r->pool),context))&&
    (!r->failures||visit(xl6ExecutionFailuresOwnershipV1(r->failures),context));}
static bool cancel(Resource* r){return !r->pool||xl6TaskPoolControlV1Get()->Cancel(r->pool);}
static bool close(Resource* r){return !r->pool||xl6TaskPoolControlV1Get()->Close(r->pool);}
static bool present(const Resource* r){return r->pool!=NULL;}
const XL6_ExecutionFamilyV1* xl6ExecutionTaskPoolFamilyV1Get(void)
{static const XL6_ExecutionFamilyV1 family={sizeof(family),XL6_EXECUTION_FAMILY_TAG_V1,XL6_EXECUTION_TASK_POOL_V1,create,rollback,retire,finish,drop,trace,close,cancel,sizeof(Resource),present};return &family;}
Resource* xl6ExecutionTaskPoolCreateV1(XL6_ExecutionSourceV1* s,uint32_t n,size_t q,size_t stack)
{
    XL6_ExecutionCreateOptionsV1 options={.kind=XL6_EXECUTION_TASK_POOL_V1,.value.workers={n,q,stack}};
    return xl6ExecutionCreateInternalV1(s,xl6ExecutionTaskPoolFamilyV1Get(),&options);
}
bool xl6ExecutionTaskPoolCheckExitV1(Resource* r)
{
    XL6_ExecutionScopeV1 scope={0};if(!xl6ExecutionResourceEnterV1(r,false,&scope))return false;
    bool ok=r->kind==XL6_EXECUTION_TASK_POOL_V1&&xl6ExecutionFailuresCheckV1(r->failures);
    if(r->kind!=XL6_EXECUTION_TASK_POOL_V1)xrtSetErrorKind(XERR_TYPE);
    xl6ExecutionResourceLeaveV1(&scope);return ok;
}
bool xl6ExecutionTaskPoolExitFailuresV1(Resource* r,XL6_ExecutionFailureSnapshotV1* snapshot)
{
    XL6_ExecutionScopeV1 scope={0};if(!xl6ExecutionResourceEnterV1(r,false,&scope))return false;
    bool ok=r->kind==XL6_EXECUTION_TASK_POOL_V1&&xl6ExecutionFailuresSnapshotV1(r->failures,snapshot);
    if(r->kind!=XL6_EXECUTION_TASK_POOL_V1)xrtSetErrorKind(XERR_TYPE);
    xl6ExecutionResourceLeaveV1(&scope);return ok;
}
