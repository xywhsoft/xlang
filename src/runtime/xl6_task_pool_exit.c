#include "include/xrt_decl.h"
#include "include/xruntime_decl.h"
#include "include/xl_clock.h"
#include "xl6_task_pool_exit.h"
#include "xl6_task_pool_internal.h"
#include "xl6_execution_worker_exit.h"
const xtaskpoolworkerexitownershipv1* xl6TaskPoolWorkerExitPolicyV1Get(void)
{
    static const xtaskpoolworkerexitownershipv1 policy={sizeof(policy),
        xl6ExecutionWorkerExitV1,xl6ExecutionWorkerExitDropV1,&xl6ExecutionFailuresOpsV1};
    return &policy;
}
XL6_TaskPool* xl6TaskPoolCreateManagedV1(const XL6_TaskPoolConfig* config,XL6_ExecutionFailuresV1* failures)
{
    XL6_ExecutionFailuresV1* owned=xl6ExecutionFailuresRetainV1(failures);if(!owned)return NULL;
    XL6_TaskPool* pool=xl6TaskPoolCreateOwnedExitInternalV1(config,owned,xl6TaskPoolWorkerExitPolicyV1Get());
    if(!pool){xerror* error=xrtTakeError();xl6ExecutionFailuresReleaseV1(owned);xrtClearError();xrtSetErrorTake(error);}
    return pool;
}
