#include "xl6_text_value.h"
#include "xl6_task_net.h"
#include "xl6_task_frame_internal.h"
#include "xl6_execution_resource.h"
#include "xl6_concurrent_deadline.h"
#include "src/api/xl_foreign_error.h"

static xfuture* submit(XL6_FutureValueProfileV1 profile,void* raw,XL6_CheckedResultClosureBorrowV1 callback,
    void* cancel,uint64_t affinity,bool delayed,uint64_t deadline)
{
    xl6_error_scope errors={0};if(!xl6ErrorScopeBegin(&errors)){
        xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});return NULL;}
    XL6_ExecutionScopeV1 scope={0};void* work=NULL;xfuture* future=NULL;
    bool ok=xl6ExecutionResourceEnterV1(raw,true,&scope);
    if(ok&&!scope.engine){xrtSetErrorKind(XERR_TYPE);ok=false;}
    if(ok)work=xl6TaskResultClosurePrepareV1(profile,callback);
    if(work)future=xl6TaskNetSubmitOwnedPolicyV1(scope.engine,affinity,work,cancel,
        xl6FutureValuePolicyV1(profile),delayed,deadline);
    if(!future||errors.failed||xrtGetError())xl6ErrorScopeCaptureFailure(&errors);
    if(!future&&work)xl6TaskFrameDiscardInternalV1(work);
    /* Keep the physical engine entry through both submission and rollback. */
    if(scope.resource)xl6ExecutionResourceLeaveV1(&scope);
    xl6_error_result outcome=xl6ErrorScopeEnd(&errors);
    if(!outcome.ok){xrtFutureDestroy(future);xl6ErrorScopePropagateFailure(outcome);return NULL;}
    xrtErrorFree(outcome.error);return future;
}
xfuture* xl6NetEngineSubmitCancelV1(XL6_FutureValueProfileV1 profile,void* engine,
    XL6_CheckedResultClosureBorrowV1 callback,void* cancel,uint64_t affinity)
{return submit(profile,engine,callback,cancel,affinity,false,0);}
xfuture* xl6NetEngineSubmitV1(XL6_FutureValueProfileV1 profile,void* engine,
    XL6_CheckedResultClosureBorrowV1 callback,uint64_t affinity)
{return xl6NetEngineSubmitCancelV1(profile,engine,callback,NULL,affinity);}
xfuture* xl6NetEngineSubmitAfterCancelV1(XL6_FutureValueProfileV1 profile,void* engine,int64_t millis,
    XL6_CheckedResultClosureBorrowV1 callback,void* cancel,uint64_t affinity)
{
    XL6_DeadlineV1 deadline;
    if(millis<0||!xl6ConcurrentDeadline(millis,&deadline)){
        if(millis<0)xrtSetErrorKind(XERR_RANGE);
        xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});return NULL;}
    return submit(profile,engine,callback,cancel,affinity,true,deadline);
}
xfuture* xl6NetEngineSubmitAfterV1(XL6_FutureValueProfileV1 profile,void* engine,int64_t millis,
    XL6_CheckedResultClosureBorrowV1 callback,uint64_t affinity)
{return xl6NetEngineSubmitAfterCancelV1(profile,engine,millis,callback,NULL,affinity);}
xfuture* xl6NetEngineSubmitUntilCancelV1(XL6_FutureValueProfileV1 profile,void* engine,uint64_t deadline,
    XL6_CheckedResultClosureBorrowV1 callback,void* cancel,uint64_t affinity)
{return submit(profile,engine,callback,cancel,affinity,true,deadline);}
xfuture* xl6NetEngineSubmitUntilV1(XL6_FutureValueProfileV1 profile,void* engine,uint64_t deadline,
    XL6_CheckedResultClosureBorrowV1 callback,uint64_t affinity)
{return xl6NetEngineSubmitUntilCancelV1(profile,engine,deadline,callback,NULL,affinity);}
