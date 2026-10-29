#define xl6ContinuationExecutorReserveOwned async_managed_reserve
#define xl6ContinuationReserveSuccessor async_managed_successor
#include "xlir_async_storage.c"
#include "xlir_async_storage.bind.h"
#undef xl6ContinuationExecutorReserveOwned
#undef xl6ContinuationReserveSuccessor
#include <assert.h>
XL6_ContinuationAdmission xl6ContinuationReserveSuccessor(XL6_ContinuationTicket*,XL6_ContinuationTicket*,
    XL6_ContinuationProc,XL6_ContinuationProc,void*);
extern XL6_ContinuationQueue* async_managed_queue(void);
extern void async_managed_observe(void*);
XL6_ContinuationAdmission async_managed_reserve(XL6_ContinuationExecutor* executor,
    XL6_ContinuationTicket* ticket,XL6_ContinuationProc run,XL6_ContinuationProc release,void* data,const XL6_CodeOwner* owner)
{
    /* Initial ready entry is not an await frame. The two observations below
     * remain the actual source-0/source-1 successor frames. */
    (void)executor;
    return xl6ContinuationReserveOwned(async_managed_queue(),ticket,run,release,data,owner);
}
XL6_ContinuationAdmission async_managed_successor(XL6_ContinuationTicket* previous,XL6_ContinuationTicket* ticket,
    XL6_ContinuationProc run,XL6_ContinuationProc release,void* data)
{
    async_managed_observe(data);return xl6ContinuationReserveSuccessor(previous,ticket,run,release,data);
}
xfuture* async_managed_create(void) { return ASYNC_STORAGE_START(); }
void async_managed_prepare(void) { assert(xlir_module_init() && !xrtGetError()); }
void async_managed_finish(void) { xlir_module_fini(); assert(!xlir_module_initialized && !xrtGetError()); }
