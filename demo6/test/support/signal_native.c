#define XRT_MODULE_SIGNAL
#define XRT_MODULE_STRING
#define XRT_MODULE_MEMORY_DEBUG
#define XRT_IMPLEMENTATION
#include "lib/xrt/xrt.h"

/* White-box witnesses for counts unreachable in a bounded test. No production
 * test hook or alternative algorithm is added to the adapter/runtime. */
bool xl6SignalTestDormant(void)
{
    return !__xrtSignalState.PlatformReady && !__xrtSignalState.Dispatcher &&
        !__xrtSignalState.Running;
}
void xl6SignalTestTotals(uint64 first, uint64 second)
{
    (void)xrtMutexLock(&__xrtSignalState.Lock);
    __xrtSignalState.Slots[0].Total = first;
    __xrtSignalState.Slots[1].Total = second;
    (void)xrtMutexUnlock(&__xrtSignalState.Lock);
}
