/* Independently link one genuine resident language family, never all three. */
#include "src/runtime/xl6_sync.h"
#include "release/include/xrt_decl.h"
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdio.h>
#ifndef SYNC_FAMILY
#error SYNC_FAMILY is required
#endif
int main(void)
{
    xl6ProductMemoryBegin(SIZE_MAX);
#if SYNC_FAMILY == 1
    xvalue* value=xl6MutexCreateV1(701); assert(value && xl6MutexLockV1(value,701));
    assert(xl6MutexUnlockV1(value,701));
#elif SYNC_FAMILY == 2
    xvalue* value=xl6CondCreateV1(702); assert(value && xl6CondSignalV1(value,702));
    assert(xl6CondBroadcastV1(value,702));
#elif SYNC_FAMILY == 3
    xvalue* value=xl6RWLockCreateV1(703); assert(value && xl6RWLockWriteV1(value,703));
    assert(xl6RWLockWriteUnlockV1(value,703));
#else
#error unsupported SYNC_FAMILY
#endif
    assert(xl6SyncAdmitsV1(value,700+SYNC_FAMILY,SYNC_FAMILY)); xrtValueRelease(value);
    size_t total=99; assert(xlSyncRetirementSnapshotCheckedV1(NULL,0,&total) && !total);
    assert(!xrtGetError() && !xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
    puts("sync independent native family: genuine calls and OOM balanced"); return 0;
}
