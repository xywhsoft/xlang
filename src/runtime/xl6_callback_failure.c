#include "src/common/xl6_xrt.h"
#include "xl6_callback_failure.h"
#include "xl6_deferred_cleanup.h"
#include "xl6_foreign_view.h"
#include <stddef.h>
#include <stdlib.h>

/* This node and Run code are resident, never stored in a generated module.
 * xerror owns only copied diagnostic text and an immutable resident Cause DAG.
 * No closure, native publisher, descriptor or generated destructor is hidden
 * in this mailbox. Its extra real error reference is an external graph root;
 * the exact xrtErrorOwnership adapter already accounts for every Cause edge.
 * Moving that reference does not create or discard any RC slot. */
static xatomic32 mailboxLock;
static xerror* firstError;
static uint64_t pendingCount,totalCount;
static bool drain(void);
static XL6_DeferredCleanupV1 deferred={.Run=drain};

static void lock(void)
{while(xrtAtomic32Exchange(&mailboxLock,1,XMEMORY_ACQUIRE))xrtThreadYield();}
static void unlock(void)
{xrtAtomic32Store(&mailboxLock,0,XMEMORY_RELEASE);}
static bool aligned(const void* p,size_t alignment)
{return p && (uintptr_t)p%alignment==0;}
static xl6_error_result take(uint64_t* count)
{
    lock();
    xl6_error_result value={pendingCount==0,firstError};
    *count=pendingCount;firstError=NULL;pendingCount=0;
    unlock();return value;
}
void xl6CallbackFailureRecordV1(xl6_error_result failure)
{
    if(failure.ok)abort();
    xerror* discard=NULL;
    lock();
    if(!pendingCount)firstError=failure.error;else discard=failure.error;
    if(pendingCount!=UINT64_MAX)++pendingCount;
    if(totalCount!=UINT64_MAX)++totalCount;
    unlock();
    xrtErrorFree(discard);
    xl6DeferredCleanupScheduleV1(&deferred);
}
bool xl6CallbackFailureTakeV1(xl6_error_result* output,uint64_t* count)
{
    if(!aligned(output,_Alignof(xl6_error_result)) || !aligned(count,_Alignof(uint64_t)) ||
       ((uintptr_t)count>=(uintptr_t)output && (uintptr_t)count-(uintptr_t)output<sizeof(*output)) ||
       ((uintptr_t)output>=(uintptr_t)count && (uintptr_t)output-(uintptr_t)count<sizeof(*count))){
        xrtSetErrorKind(XERR_ARGUMENT);return false;
    }
    uint64_t n;xl6_error_result value=take(&n);*output=value;*count=n;return true;
}
bool xl6CallbackFailureCountV1(uint64_t* pending,uint64_t* total)
{
    if(!aligned(pending,_Alignof(uint64_t)) || !aligned(total,_Alignof(uint64_t)) || pending==total){
        xrtSetErrorKind(XERR_ARGUMENT);return false;
    }
    lock();uint64_t p=pendingCount,t=totalCount;unlock();*pending=p;*total=t;return true;
}
static bool drain(void)
{
    /* Leave retains native tail storage until the publisher's physical return.
     * Also preserve a batch if this boundary already owns another first error;
     * cleanup must never acknowledge an unrelated callback failure silently. */
    if(xl6ForeignCallbackActiveV1() || !xl6ErrorScopeCanAcceptDeferredFailureV1()){
        xl6DeferredCleanupScheduleV1(&deferred);return true;
    }
    uint64_t ignored;xl6_error_result result=take(&ignored);
    if(!result.ok)xrtSetErrorTake(result.error);
    return result.ok;
}
