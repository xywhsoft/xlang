#define XRT_MODULE_RWLOCK
#define XRUNTIME_MODULE_RUNTIME_VALUE_TYPE
#include "src/common/xl6_xrt.h"
#include "src/runtime/xl6_sync_internal.h"
typedef struct RWLockOwner { XL6_SyncOwner base; xrwlock storage; } RWLockOwner;
static bool nativeUnlock(XL6_SyncOwner* base,unsigned int mode)
{
    xrwlock* lock=&((RWLockOwner*)base)->storage;
    return mode==2 ? xrtRWLockReadUnlock(lock) : mode==3 && xrtRWLockWriteUnlock(lock);
}
XL6_SYNC_CREATE(RWLock,XL6_SYNC_RWLOCK_V1,RWLockOwner,xrtRWLockInit,xrtRWLockUnit,nativeUnlock)
XL6_SYNC_METHOD(RWLockRead,XL6_SYNC_RWLOCK_V1,RWLockOwner,xrtRWLockRead,false)
XL6_SYNC_METHOD(RWLockTryRead,XL6_SYNC_RWLOCK_V1,RWLockOwner,xrtRWLockTryRead,false)
XL6_SYNC_METHOD(RWLockReadUnlock,XL6_SYNC_RWLOCK_V1,RWLockOwner,xrtRWLockReadUnlock,true)
XL6_SYNC_METHOD(RWLockWrite,XL6_SYNC_RWLOCK_V1,RWLockOwner,xrtRWLockWrite,false)
XL6_SYNC_METHOD(RWLockTryWrite,XL6_SYNC_RWLOCK_V1,RWLockOwner,xrtRWLockTryWrite,false)
XL6_SYNC_METHOD(RWLockWriteUnlock,XL6_SYNC_RWLOCK_V1,RWLockOwner,xrtRWLockWriteUnlock,true)
XL6_SYNC_METHOD(RWLockDowngrade,XL6_SYNC_RWLOCK_V1,RWLockOwner,xrtRWLockDowngrade,false)
XL6_SYNC_METHOD(RWLockUpgrade,XL6_SYNC_RWLOCK_V1,RWLockOwner,xrtRWLockUpgrade,false)
