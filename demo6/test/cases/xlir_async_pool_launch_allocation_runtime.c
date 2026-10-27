#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_ERROR
#include "lib/xrt/xrt.h"
#include <assert.h>
#include <stdio.h>
#include "xlir_owner_fault_runtime.h"
extern void payload_pool_prepare(void), payload_pool_invoke(int, int), payload_pool_finish(int);
int main(void)
{
    xmemdebugsnapshot initial, before; int round, caught, fault, runs = 0, faults = 0;
    assert(xrtMemDebugEnable(true)); xrtMemDebugSnapshot(&initial);
    for (round = 0; round < 5; ++round) for (caught = 0; caught < 2; ++caught)
    for (fault = -1; fault <= 1; ++fault) {
        xrtMemDebugSnapshot(&before); payload_pool_prepare();
        /* The fixed executor already exists after module initialization.
         * These first two launch allocations precede submission, so worker
         * allocator timing cannot alter the thread-local injection target. */
        continuation_owner_fault_arm(fault); payload_pool_invoke(caught, fault);
        (void)continuation_owner_fault_end(fault);
        payload_pool_finish(fault); continuation_owner_fault_balanced(&before);
        ++runs; faults += fault >= 0;
    }
    continuation_owner_fault_balanced(&initial);
    printf("async pool launch: %d module lifetimes, %d real pre-submission allocation failures, caught/propagated outcomes and balanced memory\n", runs, faults);
    return 0;
}
