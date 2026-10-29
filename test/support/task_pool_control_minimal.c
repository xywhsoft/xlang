/* Implementation selectors come ONLY from the real deployment/ownership plan.
 * No test-side ALL, ATOMIC, ONCE or diagnostic feature can mask a missing
 * production dependency. Compile separately from the debug/race fixture. */
#include "pool-modules.h"
#ifdef XL6_TASK_POOL_TEST_RUNTIME
#define XRUNTIME_IMPLEMENTATION
#include "lib/xruntime/xruntime_features.h"
#if defined(XRUNTIME_IMPLEMENTATION) && !defined(XRT_IMPLEMENTATION)
#define XRT_IMPLEMENTATION
#endif
#include "lib/xrt/xrt.h"
#include "lib/xruntime/xruntime.h"
#else
#define XRT_IMPLEMENTATION
#include "lib/xrt/xrt.h"
#endif
#include "src/runtime/xl6_task.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    const XL6_TaskPoolControlV1* api=xl6TaskPoolControlV1Get();
    XL6_TaskPoolConfig config={1,8,0};XL6_TaskPool* pool=api->Create(&config);assert(pool);
    assert(api->Start(pool)&&api->Close(pool));
    XL6_TaskPoolWaitStatusV1 status;assert(api->WaitFor(pool,5000,NULL,&status)&&status==XL6_TASK_POOL_READY_V1);
    XL6_TaskPoolStatsV1 stats={.size=sizeof(stats)};
    assert(api->Snapshot(pool,&stats)&&stats.initialized&&stats.threads==1&&stats.closed&&!stats.submitted);
    assert(api->Quiesce(pool)&&api->Destroy(pool));
    puts("Task pool minimal deployment: exact planned sources/selectors, actual native implementation, no test feature additions");
    return 0;
}
