/* Separate translation unit: the SDK owns debug ABI definitions. Generated
 * standalone C does not need the memory-debug module or duplicate XRT types. */
#include "release/include/xrt_decl.h"
#include "module_product_memory.h"
#include <assert.h>
#include <stdio.h>
static xmemdebugsnapshot productBefore;
static bool productLive(const xmemdebugallocation* allocation, void* context)
{
    (void)context;
    fprintf(stderr, "live %p size=%zu at %s:%u\n", allocation->Address,
        allocation->Size, allocation->File ? allocation->File : "unknown", allocation->Line);
    return true;
}
void xl6ProductMemoryBegin(size_t point)
{
    assert(xrtMemDebugEnable(true));
    xrtMemDebugSnapshot(&productBefore);
    if (point != SIZE_MAX) assert(xrtMemDebugFailAfter(point));
}
bool xl6ProductMemoryEnd(void)
{
    bool hit = xrtMemDebugFailTriggered();
    xrtMemDebugFailClear();
    return hit;
}
void xl6ProductMemoryVerify(void)
{
    xmemdebugsnapshot after;
    xrtMemDebugSnapshot(&after);
    if (productBefore.LiveCount != after.LiveCount || productBefore.LiveBytes != after.LiveBytes)
        xrtMemDebugVisitLive(productLive, NULL);
    assert(productBefore.LiveCount == after.LiveCount && productBefore.LiveBytes == after.LiveBytes);
    assert(after.AllocCount - productBefore.AllocCount == after.FreeCount - productBefore.FreeCount);
    assert(productBefore.DoubleFreeCount == after.DoubleFreeCount && productBefore.InvalidFreeCount == after.InvalidFreeCount);
    assert(productBefore.UseAfterFreeCount == after.UseAfterFreeCount);
}
