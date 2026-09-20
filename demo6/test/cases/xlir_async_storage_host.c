#define xl6ContinuationExecutorReserveOwned async_storage_reserve
#ifndef XLIR_ASYNC_STORAGE_SOURCE
#define XLIR_ASYNC_STORAGE_SOURCE "xlir_async_storage.c"
#endif
#include XLIR_ASYNC_STORAGE_SOURCE
#include "xlir_async_storage.bind.h"
#undef xl6ContinuationExecutorReserveOwned
extern XL6_ContinuationQueue* async_storage_queue(void);
XL6_ContinuationAdmission async_storage_reserve(XL6_ContinuationExecutor* executor,
    XL6_ContinuationTicket* ticket,XL6_ContinuationProc run,XL6_ContinuationProc release,void* data,const XL6_CodeOwner* owner)
{
    (void)executor; return xl6ContinuationReserveOwned(async_storage_queue(),ticket,run,release,data,owner);
}
xfuture* async_storage_generated_create(void) { return ASYNC_STORAGE_START(); }
