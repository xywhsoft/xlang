#include "xl6_execution_resource_internal.h"
typedef XL6_ExecutionResourceV1 Resource;
static bool create(Resource* r,uint32_t threads,size_t limit,size_t stack)
{XL6_TaskPoolConfig config={threads,limit,stack};r->pool=xl6TaskPoolControlV1Get()->Create(&config);return r->pool!=NULL;}
static bool rollback(Resource* r)
{if(r->pool&&!xl6TaskPoolControlV1Get()->Destroy(r->pool))return false;r->pool=NULL;return true;}
static XL6_ModulePrepareResult retire(Resource* r)
{
    XL6_ModulePrepareResult result=xl6TaskPoolControlV1Get()->TryQuiesce(r->pool);
    if(result!=XL6_MODULE_PREPARE_READY)return result;
    return xl6TaskPoolControlV1Get()->Destroy(r->pool)?XL6_MODULE_PREPARE_READY:XL6_MODULE_PREPARE_FAILED;
}
static bool finish(Resource* r){return !r->pool||xl6TaskPoolReleaseClearedOwnerV1(r->pool);}
static void drop(Resource* r){(void)r;}
static bool trace(const Resource* r,xrtownershipvisitor visit,void* context)
{return !r->pool||visit(xl6TaskPoolOwnership(r->pool),context);}
static bool cancel(Resource* r){return !r->pool||xl6TaskPoolControlV1Get()->Cancel(r->pool);}
static bool close(Resource* r){return !r->pool||xl6TaskPoolControlV1Get()->Close(r->pool);}
const XL6_ExecutionFamilyV1* xl6ExecutionTaskPoolFamilyV1Get(void)
{static const XL6_ExecutionFamilyV1 family={sizeof(family),XL6_EXECUTION_FAMILY_TAG_V1,XL6_EXECUTION_TASK_POOL_V1,create,rollback,retire,finish,drop,trace,close,cancel};return &family;}
Resource* xl6ExecutionTaskPoolCreateV1(XL6_ExecutionSourceV1* s,uint32_t n,size_t q,size_t stack)
{return xl6ExecutionCreateInternalV1(s,xl6ExecutionTaskPoolFamilyV1Get(),n,q,stack);}
