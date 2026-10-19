#include "include/xruntime_decl.h"
#include "xl6_execution_worker_exit.h"
#include "xl6_thread_exit.h"
#include "src/api/xl_foreign_error.h"
#include <stdlib.h>
void xl6ExecutionWorkerExitV1(void* raw)
{
    xerror* previous=xrtTakeError();xl6_error_scope cleanup={0};xl6_error_result outcome={false,NULL};
    if(xl6ErrorScopeBegin(&cleanup)){
        if(!xl6ThreadExitCurrentV1())xl6ErrorScopeCaptureFailure(&cleanup);
        outcome=xl6ErrorScopeEnd(&cleanup);
    }else outcome.error=xrtTakeError();
    xl6ExecutionFailuresRecordV1(raw,outcome.ok,outcome.error);xrtClearError();xrtSetErrorTake(previous);
}
void xl6ExecutionWorkerExitDropV1(void* raw,void* context)
{if(context)abort();xl6ExecutionFailuresReleaseV1(raw);}
