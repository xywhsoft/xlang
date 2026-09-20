#define xl6ContinuationExecutorReserveOwned executor_dependency_reserve
#include "xlir_async_dependency_cycle.c"
#include "xlir_async_dependency_cycle.bind.h"
#undef xl6ContinuationExecutorReserveOwned
#include <assert.h>
extern XL6_ContinuationExecutor* dependency_executor(void);
extern const XL6_CodeOwner* dependency_owner(void);
extern XL6_ContinuationAdmission xl6ContinuationExecutorReserveOwned(XL6_ContinuationExecutor*,XL6_ContinuationTicket*,
    XL6_ContinuationProc,XL6_ContinuationProc,void*,const XL6_CodeOwner*);
XL6_ContinuationAdmission executor_dependency_reserve(XL6_ContinuationExecutor* unused,XL6_ContinuationTicket* ticket,
    XL6_ContinuationProc run,XL6_ContinuationProc release,void* data,const XL6_CodeOwner* unusedOwner)
{
    (void)unused;(void)unusedOwner;
    return xl6ContinuationExecutorReserveOwned(dependency_executor(),ticket,run,release,data,dependency_owner());
}
xfuture* dependency_start(unsigned id,xfuture* boot,xfuture* cleanup,bool waitCleanup)
{
    xl6_error_scope scope={0};assert(xl6ErrorScopeBegin(&scope));
    xlir_result_future output=DEPENDENCY_START(id,boot,cleanup,waitCleanup);
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    assert(output.ok&&output.value&&outcome.ok&&!outcome.error&&!xrtGetError());return output.value;
}
