#include "xl6_text_value.h"
#include "xl6_task_pool_language.h"
#include "xl6_execution_resource.h"
#include "xl6_native_handle.h"
#include "src/api/xl_foreign_error.h"

static void failed(void){xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});}
static bool enter(void* raw,bool submit,XL6_ExecutionScopeV1* scope)
{
    if(!xl6ExecutionResourceEnterV1(raw,submit,scope))return false;
    if(scope->pool)return true;
    xl6ExecutionResourceLeaveV1(scope);xrtSetErrorKind(XERR_TYPE);return false;
}
void* xl6TaskPoolCreateLanguageV1(XL6_ExecutionServicesV1 services,int64_t threads,int64_t limit,int64_t stack)
{
    if(threads<0||(uint64_t)threads>XRT_TASK_POOL_THREAD_LIMIT||limit<=0||(uint64_t)limit>SIZE_MAX||
        stack<0||(uint64_t)stack>SIZE_MAX){xrtSetErrorKind(XERR_RANGE);failed();return NULL;}
    void* result=xl6ExecutionTaskPoolCreateV1(services.source,(uint32_t)threads,(size_t)limit,(size_t)stack);
    if(!result)failed();
    return result;
}
void xl6TaskPoolDestroyLanguageV1(void* resource){xl6ExecutionResourceReleaseV1(resource);}
void xl6TaskPoolStartLanguageV1(void* raw)
{
    XL6_ExecutionScopeV1 scope={0};bool ok=enter(raw,true,&scope);
    if(ok)ok=xl6TaskPoolControlV1Get()->Start(scope.pool);
    if(scope.resource)xl6ExecutionResourceLeaveV1(&scope);
    if(!ok)failed();
}
void xl6TaskPoolCloseLanguageV1(void* raw){if(!xl6ExecutionResourceCloseV1(raw))failed();}
void xl6TaskPoolCancelLanguageV1(void* raw){if(!xl6ExecutionResourceCancelV1(raw))failed();}
static bool waitFor(void* raw,bool absolute,uint64_t until,int64_t timeout,void* cancel)
{
    XL6_ExecutionScopeV1 scope={0};XL6_TaskPoolWaitStatusV1 status=XL6_TASK_POOL_TIMEOUT_V1;
    bool ok=enter(raw,false,&scope);
    if(ok)ok=absolute?xl6TaskPoolControlV1Get()->WaitUntil(scope.pool,until,cancel,&status):
        xl6TaskPoolControlV1Get()->WaitFor(scope.pool,timeout,cancel,&status);
    if(scope.resource)xl6ExecutionResourceLeaveV1(&scope);
    if(ok&&status==XL6_TASK_POOL_CANCELLED_V1){xrtSetErrorKind(XERR_CANCELLED);ok=false;}
    if(!ok){failed();return false;}return status==XL6_TASK_POOL_READY_V1;
}
bool xl6TaskPoolWaitLanguageV1(void* raw,int64_t timeout){return waitFor(raw,false,0,timeout,NULL);}
bool xl6TaskPoolWaitUntilLanguageV1(void* raw,uint64_t until){return waitFor(raw,true,until,0,NULL);}
bool xl6TaskPoolWaitCancelLanguageV1(void* raw,void* cancel,int64_t timeout){return waitFor(raw,false,0,timeout,cancel);}
bool xl6TaskPoolWaitUntilCancelLanguageV1(void* raw,uint64_t until,void* cancel){return waitFor(raw,true,until,0,cancel);}
void* xl6TaskPoolSnapshotLanguageV1(void* raw)
{
    XL6_ExecutionScopeV1 scope={0};XL6_TaskPoolStatsV1 stats={.size=sizeof(stats)};
    bool ok=enter(raw,false,&scope);if(ok)ok=xl6TaskPoolControlV1Get()->Snapshot(scope.pool,&stats);
    if(scope.resource)xl6ExecutionResourceLeaveV1(&scope);
    if(!ok){failed();return NULL;}
    XL6_TaskPoolStatsV1* output=xrtMalloc(sizeof(*output));if(output)*output=stats;else failed();return output;
}
void xl6TaskPoolSnapshotDestroyLanguageV1(void* value){xrtFree(value);}
#define STAT(Name,Field,Type) Type xl6TaskPoolSnapshot##Name##LanguageV1(const void* raw) \
{if(!raw){xrtSetErrorKind(XERR_ARGUMENT);failed();return (Type)0;}return ((const XL6_TaskPoolStatsV1*)raw)->Field;}
STAT(Threads,threads,uint64_t)
STAT(QueueLimit,queueLimit,uint64_t)
STAT(Queued,queued,uint64_t)
STAT(Running,running,uint64_t)
STAT(Submitted,submitted,uint64_t)
STAT(Completed,completed,uint64_t)
STAT(Succeeded,succeeded,uint64_t)
STAT(Failed,failed,uint64_t)
STAT(Cancelled,cancelled,uint64_t)
STAT(Rejected,rejected,uint64_t)
STAT(Initialized,initialized,bool)
STAT(Closed,closed,bool)
STAT(Cancelling,cancelling,bool)
#undef STAT
static bool handleTrace(const xvalue* value,xrtownershipvisitor visit,void* context)
{
    void* resource=NULL;void* user=NULL;const xvaluehandleops* ops=NULL;
    return visit&&xrtValueGetHandle(value,&resource,&ops,&user)&&resource&&!user&&
        ops==xl6TaskPoolHandlePolicyV1Get()->Ops&&visit(xl6ExecutionResourceOwnershipV1(resource),context);
}
XL6_DEFINE_NATIVE_HANDLE_POLICY(xl6TaskPoolHandlePolicyV1Get,xl6TaskPoolDestroyLanguageV1,handleTrace)
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6TaskPoolSnapshotHandlePolicyV1Get,xl6TaskPoolSnapshotDestroyLanguageV1)
