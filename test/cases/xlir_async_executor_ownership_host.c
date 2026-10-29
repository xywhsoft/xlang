#define xl6ContinuationFrameLaunchV1 executor_dependency_launch
#include "xlir_async_dependency_cycle.c"
#include "xlir_async_dependency_cycle.bind.h"
#undef xl6ContinuationFrameLaunchV1
#include <assert.h>
extern XL6_ContinuationExecutor* dependency_executor(void);
extern const XL6_CodeOwner* dependency_owner(void);
extern xfuture* xl6ContinuationFrameLaunchV1(void*,XL6_ContinuationExecutor*,XL6_ContinuationProc,const XL6_CodeOwner*);
/* Select the host's actual executor before the resident launcher validates
 * its execution domain. Promise/producer/frame admission remains entirely
 * in that real launcher, rather than intercepting an obsolete Cgen reserve. */
xfuture* executor_dependency_launch(void* data,XL6_ContinuationExecutor* unused,
    XL6_ContinuationProc run,const XL6_CodeOwner* unusedOwner)
{
    (void)unused;(void)unusedOwner;
    return xl6ContinuationFrameLaunchV1(data,dependency_executor(),run,dependency_owner());
}
xfuture* dependency_start(unsigned id,xfuture* boot,xfuture* cleanup,bool waitCleanup)
{
    xl6_error_scope scope={0};assert(xl6ErrorScopeBegin(&scope));
    xlir_result_future output=DEPENDENCY_START(id,boot,cleanup,waitCleanup);
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    assert(output.ok&&output.value&&outcome.ok&&!outcome.error&&!xrtGetError());return output.value;
}
