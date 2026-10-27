/* Intercept only queue admission, never the resident Watch/storage protocol. */
#define xl6ContinuationExecutorReserveOwned cancel_wait_reserve
#define xl6ContinuationReserveSuccessor cancel_wait_successor
#include "xlir_async_cancel_watch.c"
#include "xlir_async_cancel_watch.h.bind.h"
#undef xl6ContinuationExecutorReserveOwned
#undef xl6ContinuationReserveSuccessor
#include <assert.h>
XL6_ContinuationAdmission xl6ContinuationReserveSuccessor(XL6_ContinuationTicket*,XL6_ContinuationTicket*,
    XL6_ContinuationProc,XL6_ContinuationProc,void*);
extern XL6_ContinuationQueue* cancel_wait_queue(void);
extern void cancel_wait_frame(void*);
extern void cancel_wait_before_successor(void);
extern int cancel_watch_dropped(int),cancel_watch_finalized(void);
XL6_ContinuationAdmission cancel_wait_reserve(XL6_ContinuationExecutor* executor,XL6_ContinuationTicket* ticket,
    XL6_ContinuationProc run,XL6_ContinuationProc release,void* data,const XL6_CodeOwner* owner)
{
    /* Initial entry is ready work, not an armed await. Pending frame pins
     * are still recorded at the generated successor boundary below. */
    (void)executor;
    return xl6ContinuationReserveOwned(cancel_wait_queue(),ticket,run,release,data,owner);
}
XL6_ContinuationAdmission cancel_wait_successor(XL6_ContinuationTicket* previous,XL6_ContinuationTicket* ticket,
    XL6_ContinuationProc run,XL6_ContinuationProc release,void* data)
{
    cancel_wait_frame(data);
    XL6_ContinuationAdmission result=xl6ContinuationReserveSuccessor(previous,ticket,run,release,data);
    if(result==XL6_CONTINUATION_ADMITTED)cancel_wait_before_successor();
    return result;
}
void cancel_watch_drop(int64_t id)
{
    int mode=cancel_watch_dropped((int)id);
    if(mode){xl6_error_result failure={false,mode==1?xrtErrorCreate(XERR_STATE,"cancel wait",90+(int)id,"frame drop failed"):NULL};
        if(mode==1)assert(failure.error);
        xl6ErrorScopePropagateFailure(failure);}
}
void cancel_watch_finally(void)
{
    int mode=cancel_watch_finalized();
    if(mode){xl6_error_result failure={false,mode==1?xrtErrorCreate(XERR_STATE,"cancel wait",77,"finally failed"):NULL};
        if(mode==1)assert(failure.error);
        xl6ErrorScopePropagateFailure(failure);}
}
void cancel_wait_prepare(void) {assert(xlir_module_init()&&!xrtGetError());}
void cancel_wait_finish(void) {xlir_module_fini();assert(!xrtGetError());}
xfuture* cancel_wait_launch(xvalue* a,xvalue* b,xfuture* source,xfuture* cleanup,int mode)
{
    xl6_error_scope scope={0};assert(xl6ErrorScopeBegin(&scope));
    xlir_result_future result=CANCEL_WATCH_RUN(a,b,source,cleanup,mode);
    /* Initial entry is queued by the current fixed-worker launcher. Reach
     * the first pending source before testing its armed cancellation path. */
    assert(result.ok&&result.value&&xl6ContinuationRunOne(cancel_wait_queue()));
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    assert(result.ok&&result.value&&outcome.ok&&!outcome.error&&!xrtGetError());return result.value;
}
