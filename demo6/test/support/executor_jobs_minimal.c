#include "executor-jobs-modules.h"
#define XRUNTIME_IMPLEMENTATION
#include "lib/xruntime/xruntime.h"
#include "src/runtime/xl6_executor_jobs.h"
#include <assert.h>
#include <stdio.h>
static bool body(void* data){assert(!data);return true;}
int main(void)
{
    const XL6_ClosureRuntimeV1* api=xl6ClosureRuntimeV1Get();
    XL6_Closure* callback=xl6ClosureRuntimeV2Get()->Create(NULL,(void (*)(void))body,NULL,NULL,NULL,NULL,NULL,0);assert(callback);
    XL6_ExecutionFailuresV1* state=xl6ExecutionFailuresCreateV1();assert(state);
    xexecutorconfig config={1,8,0};xexecutor* executor=xrtExecutorCreate(&config);assert(executor);
    assert(xl6ExecutorSubmitClosureV1(executor,state,callback));api->Release(callback);
    XL6_ExecutorWaitStatusV1 status;assert(xrtExecutorClose(executor));
    assert(xl6ExecutorWaitV1(executor,state,5000,&status)&&status==XL6_EXECUTOR_READY_V1);
    assert(xrtExecutorDestroy(executor));xl6ExecutionFailuresReleaseV1(state);
    puts("Executor jobs minimal: actual planned sources/selectors and native implementation only");return 0;
}
