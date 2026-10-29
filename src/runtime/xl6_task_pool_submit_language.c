#include "xl6_text_value.h"
#include "xl6_task_pool_language.h"
#include "xl6_task_submit_closure.h"
#include "xl6_execution_resource.h"
#include "src/api/xl_foreign_error.h"

xfuture* xl6TaskPoolSubmitCancelLanguageV1(XL6_FutureValueProfileV1 profile,void* raw,
    XL6_CheckedResultClosureBorrowV1 callback,void* cancel)
{
    xl6_error_scope errors={0};if(!xl6ErrorScopeBegin(&errors)){
        xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});return NULL;}
    XL6_ExecutionScopeV1 scope={0};xfuture* future=NULL;
    bool ok=xl6ExecutionResourceEnterV1(raw,true,&scope);
    if(ok&&!scope.pool){xrtSetErrorKind(XERR_TYPE);ok=false;}
    if(ok)future=xl6TaskSubmitClosureV1(scope.pool,profile,callback,cancel);
    if(!future||errors.failed||xrtGetError())xl6ErrorScopeCaptureFailure(&errors);
    if(scope.resource)xl6ExecutionResourceLeaveV1(&scope);
    xl6_error_result outcome=xl6ErrorScopeEnd(&errors);
    if(!outcome.ok){xrtFutureDestroy(future);xl6ErrorScopePropagateFailure(outcome);return NULL;}
    xrtErrorFree(outcome.error);return future;
}
xfuture* xl6TaskPoolSubmitLanguageV1(XL6_FutureValueProfileV1 profile,void* raw,XL6_CheckedResultClosureBorrowV1 callback)
{return xl6TaskPoolSubmitCancelLanguageV1(profile,raw,callback,NULL);}
