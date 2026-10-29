#define xl6ContinuationExecutorReserveOwned dependency_reserve
#include "xlir_async_dependency_cycle.c"
#include "xlir_async_dependency_cycle.bind.h"
#undef xl6ContinuationExecutorReserveOwned
#include <assert.h>
extern XL6_ContinuationQueue* dependency_queue(void);
extern const XL6_CodeOwner* dependency_owner(void);
void dependency_prepare(void) {assert(xlir_module_init()&&!xrtGetError());}
void dependency_finish(void) {xlir_module_fini();assert(!xrtGetError());}
XL6_ContinuationAdmission dependency_reserve(XL6_ContinuationExecutor* executor,XL6_ContinuationTicket* ticket,
    XL6_ContinuationProc run,XL6_ContinuationProc release,void* data,const XL6_CodeOwner* owner)
{
    (void)executor;(void)owner;
    return xl6ContinuationReserveOwned(dependency_queue(),ticket,run,release,data,dependency_owner());
}
xfuture* dependency_start(unsigned id,xfuture* boot,xfuture* cleanup,bool waitCleanup)
{
    xl6_error_scope scope={0};assert(xl6ErrorScopeBegin(&scope));
    xlir_result_future output=DEPENDENCY_START(id,boot,cleanup,waitCleanup);
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    assert(output.ok&&output.value&&outcome.ok&&!outcome.error&&!xrtGetError());return output.value;
}
