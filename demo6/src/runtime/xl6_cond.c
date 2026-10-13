#define XRT_MODULE_COND
#define XRT_MODULE_MUTEX
#define XRUNTIME_MODULE_RUNTIME_VALUE_TYPE
#include "lib/xruntime/xruntime.h"
#include "src/runtime/xl6_mutex_internal.h"
#include "src/runtime/xl6_cond_internal.h"
typedef struct CondOwner { XL6_SyncOwner base; xcond storage; } CondOwner;
XL6_SYNC_CREATE(Cond,XL6_SYNC_COND_V1,CondOwner,xrtCondInit,xrtCondUnit,NULL)
XL6_SYNC_METHOD(CondSignal,XL6_SYNC_COND_V1,CondOwner,xrtCondSignal,false)
XL6_SYNC_METHOD(CondBroadcast,XL6_SYNC_COND_V1,CondOwner,xrtCondBroadcast,false)
static bool wait(const xvalue* cond,uint64_t condId,const xvalue* mutex,uint64_t mutexId,bool timed,int64_t milliseconds)
{
    xl6_error_scope work={0}; if (!xl6ErrorScopeBegin(&work)) abort();
    XL6_SyncOwner* c=xl6SyncOwnerBeginNative(cond,condId,XL6_SYNC_COND_V1,false);
    XL6_SyncOwner* m=c ? xl6SyncOwnerBeginNative(mutex,mutexId,XL6_SYNC_MUTEX_V1,false) : NULL;
    uint64_t timeout=milliseconds<=0 ? 0 :
        (uint64_t)milliseconds>UINT64_MAX/1000u ? UINT64_MAX : (uint64_t)milliseconds*1000u;
    bool ok=c && m && (timed ? xrtCondWaitFor(&((CondOwner*)c)->storage,&((XL6_MutexOwner*)m)->storage,timeout)
        : xrtCondWait(&((CondOwner*)c)->storage,&((XL6_MutexOwner*)m)->storage))==XWAIT_OK;
    xl6SyncOwnerEndNative(m); xl6SyncOwnerEndNative(c);
    xl6_error_result result=xl6ErrorScopeEnd(&work); xrtErrorFree(result.error); return ok;
}
bool xl6CondWaitV1(const xvalue* c,uint64_t ci,const xvalue* m,uint64_t mi) { return wait(c,ci,m,mi,false,0); }
bool xl6CondWaitTimeoutV1(const xvalue* c,uint64_t ci,const xvalue* m,uint64_t mi,int64_t ms) { return wait(c,ci,m,mi,true,ms); }
xwaitresult xl6CondWaitUntilNative(const xvalue* cond,uint64_t condId,const xvalue* mutex,uint64_t mutexId,xdeadline deadline)
{
    xl6_error_scope work={0};if(!xl6ErrorScopeBegin(&work))abort();
    XL6_SyncOwner* c=xl6SyncOwnerBeginNative(cond,condId,XL6_SYNC_COND_V1,false);
    XL6_SyncOwner* m=c?xl6SyncOwnerBeginNative(mutex,mutexId,XL6_SYNC_MUTEX_V1,false):NULL;
    xwaitresult waited=c && m?xrtCondWaitUntil(&((CondOwner*)c)->storage,&((XL6_MutexOwner*)m)->storage,deadline):XWAIT_ERROR;
    if(waited==XWAIT_ERROR || xrtGetError())xl6ErrorScopeCaptureFailure(&work);
    xl6SyncOwnerEndNative(m);xl6SyncOwnerEndNative(c);
    xl6_error_result outcome=xl6ErrorScopeEnd(&work);
    if(!outcome.ok){xl6ErrorScopePropagateFailure(outcome);return XWAIT_ERROR;}
    return waited;
}
