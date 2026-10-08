#ifndef XL6_MUTEX_INTERNAL_H
#define XL6_MUTEX_INTERNAL_H
#include "src/runtime/xl6_sync_internal.h"
typedef struct XL6_MutexOwner { XL6_SyncOwner base; xmutex storage; } XL6_MutexOwner;
#endif
