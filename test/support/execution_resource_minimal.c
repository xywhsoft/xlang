#include "execution-resource-modules.h"
#define XRUNTIME_IMPLEMENTATION
#include "lib/xruntime/xruntime.h"
#include "src/runtime/xl6_execution_resource.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    XL6_ExecutionSourceV1* source=xl6ExecutionSourceCreateV1();assert(source);
#ifdef XL6_TEST_EXECUTOR
    XL6_ExecutionResourceV1* executor=xl6ExecutionExecutorCreateV1(source,1,8,0);assert(executor);
    assert(xl6ExecutionResourceCloseV1(executor));xl6ExecutionResourceReleaseV1(executor);
#endif
#ifdef XL6_TEST_POOL
    XL6_ExecutionResourceV1* pool=xl6ExecutionTaskPoolCreateV1(source,1,8,0);assert(pool);
    assert(xl6ExecutionResourceCloseV1(pool));xl6ExecutionResourceReleaseV1(pool);
#endif
    assert(xl6ExecutionSourceCloseV1(source));xdeadline deadline=xrtDeadlineAfter(UINT64_C(5000000));
    while(source){XL6_ExecutionRetireResultV1 done=xl6ExecutionSourceDestroyV1(&source);assert(done.ok);
        if(done.ready)break;
        assert(!xrtDeadlineExpired(deadline));xrtThreadYield();}
    assert(!source);puts("Execution resources minimal: selected actual source/native closure, creator consumed after physical retirement");return 0;
}
