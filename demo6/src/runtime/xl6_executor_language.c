#include "xl6_text_value.h"
#include "xl6_execution_language.h"
#include "xl6_execution_resource.h"
#include "xl6_native_handle.h"
#include "src/api/xl_foreign_error.h"

static void failed(void)
{xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});}
void* xl6ExecutorCreateLanguageV1(XL6_ExecutionServicesV1 services,int64_t threads,int64_t limit,int64_t stack)
{
    if(threads<0 || (uint64_t)threads>UINT32_MAX || limit<=0 || (uint64_t)limit>SIZE_MAX ||
       stack<0 || (uint64_t)stack>SIZE_MAX){xrtSetErrorKind(XERR_RANGE);failed();return NULL;}
    XL6_ExecutionResourceV1* result=xl6ExecutionExecutorCreateV1(services.source,(uint32_t)threads,(size_t)limit,(size_t)stack);
    if(!result)failed();
    return result;
}
void xl6ExecutorDestroyLanguageV1(void* resource){xl6ExecutionResourceReleaseV1(resource);}
void xl6ExecutorSubmitLanguageV1(void* resource,XL6_Closure* work)
{if(!xl6ExecutionExecutorSubmitV1(resource,work))failed();}
void xl6ExecutorCloseLanguageV1(void* resource)
{if(!xl6ExecutionResourceCloseV1(resource))failed();}
void xl6ExecutorCancelLanguageV1(void* resource)
{if(!xl6ExecutionResourceCancelV1(resource))failed();}
bool xl6ExecutorWaitLanguageV1(void* resource,int64_t millis)
{
    XL6_ExecutorWaitStatusV1 status=XL6_EXECUTOR_TIMEOUT_V1;
    if(!xl6ExecutionExecutorWaitV1(resource,millis,&status)){failed();return false;}
    return status==XL6_EXECUTOR_READY_V1;
}
void xl6ExecutorThrowLanguageV1(void* resource)
{if(!xl6ExecutionExecutorThrowIfFailedV1(resource))failed();}
uint64_t xl6ExecutorFailureCountLanguageV1(void* resource)
{
    XL6_ExecutorFailureSnapshotV1 snapshot={.size=sizeof(snapshot)};
    if(!xl6ExecutionExecutorFailuresV1(resource,&snapshot))failed();
    return snapshot.count;
}
bool xl6ExecutorFailureSaturatedLanguageV1(void* resource)
{
    XL6_ExecutorFailureSnapshotV1 snapshot={.size=sizeof(snapshot)};
    if(!xl6ExecutionExecutorFailuresV1(resource,&snapshot))failed();
    return snapshot.saturated;
}
void* xl6ExecutorSnapshotLanguageV1(void* resource)
{
    xexecutorstats stats;
    if(!xl6ExecutionExecutorStatsV1(resource,&stats)){failed();return NULL;}
    xexecutorstats* result=xrtMalloc(sizeof(*result));
    if(result)*result=stats;else failed();
    return result;
}
void xl6ExecutorSnapshotDestroyLanguageV1(void* snapshot){xrtFree(snapshot);}
#define XL6_EXECUTOR_STAT(Name) uint64_t xl6ExecutorSnapshot##Name##LanguageV1(const void* raw) \
{if(!raw){xrtSetErrorKind(XERR_ARGUMENT);failed();return 0;}return ((const xexecutorstats*)raw)->Name;}
XL6_EXECUTOR_STAT(Threads)
XL6_EXECUTOR_STAT(QueueLimit)
XL6_EXECUTOR_STAT(Queued)
XL6_EXECUTOR_STAT(Running)
XL6_EXECUTOR_STAT(Submitted)
XL6_EXECUTOR_STAT(Completed)
XL6_EXECUTOR_STAT(Executed)
XL6_EXECUTOR_STAT(Stolen)
XL6_EXECUTOR_STAT(Cancelled)
XL6_EXECUTOR_STAT(Rejected)
#undef XL6_EXECUTOR_STAT
bool xl6ExecutorSnapshotClosedLanguageV1(const void* raw)
{if(!raw){xrtSetErrorKind(XERR_ARGUMENT);failed();return false;}return ((const xexecutorstats*)raw)->Closed;}
bool xl6ExecutorSnapshotCancellingLanguageV1(const void* raw)
{if(!raw){xrtSetErrorKind(XERR_ARGUMENT);failed();return false;}return ((const xexecutorstats*)raw)->Cancelling;}
static bool handleTrace(const xvalue* value,xrtownershipvisitor visit,void* context)
{
    void* resource=NULL;void* user=NULL;const xvaluehandleops* ops=NULL;
    return visit&&xrtValueGetHandle(value,&resource,&ops,&user)&&resource&&!user&&
        ops==xl6ExecutorHandlePolicyV1Get()->Ops&&visit(xl6ExecutionResourceOwnershipV1(resource),context);
}
XL6_DEFINE_NATIVE_HANDLE_POLICY(xl6ExecutorHandlePolicyV1Get,xl6ExecutorDestroyLanguageV1,handleTrace)
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6ExecutorSnapshotHandlePolicyV1Get,xl6ExecutorSnapshotDestroyLanguageV1)
