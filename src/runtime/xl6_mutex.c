#define XRT_MODULE_MUTEX
#define XRUNTIME_MODULE_RUNTIME_VALUE_TYPE
#include "src/common/xl6_xrt.h"
#include "src/runtime/xl6_mutex_internal.h"
static bool nativeUnlock(XL6_SyncOwner* base,unsigned int mode)
{ return mode==1 && xrtMutexUnlock(&((XL6_MutexOwner*)base)->storage); }
XL6_SYNC_CREATE(Mutex,XL6_SYNC_MUTEX_V1,XL6_MutexOwner,xrtMutexInit,xrtMutexUnit,nativeUnlock)
XL6_SYNC_METHOD(MutexLock,XL6_SYNC_MUTEX_V1,XL6_MutexOwner,xrtMutexLock,false)
XL6_SYNC_METHOD(MutexTryLock,XL6_SYNC_MUTEX_V1,XL6_MutexOwner,xrtMutexTryLock,false)
XL6_SYNC_METHOD(MutexUnlock,XL6_SYNC_MUTEX_V1,XL6_MutexOwner,xrtMutexUnlock,true)
