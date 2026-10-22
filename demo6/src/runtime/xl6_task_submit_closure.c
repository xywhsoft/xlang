#define XRT_MODULE_TASK_POOL
#include "lib/xrt/xrt.h"
#include "xl6_task_submit_closure.h"
#include "xl6_task_result_closure.h"
#include "xl6_task_frame_internal.h"
#include "src/api/xl_foreign_error.h"
xfuture* xl6TaskSubmitClosureV1(XL6_TaskPool* pool,XL6_FutureValueProfileV1 profile,
    XL6_CheckedResultClosureBorrowV1 borrow,xcancel* parent)
{
    xl6_error_scope errors={0};if(!xl6ErrorScopeBegin(&errors)){
        xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});return NULL;}
    void* work=NULL;xfuture* future=NULL;
    if(!pool)xrtSetErrorKind(XERR_ARGUMENT);
    else work=xl6TaskResultClosurePrepareV1(profile,borrow);
    if(work)future=xl6TaskSubmitOwnedPolicyV1(pool,work,parent,xl6FutureValuePolicyV1(profile),true);
    if(!future){
        xl6ErrorScopeCaptureFailure(&errors);
        if(work)xl6TaskFrameDiscardInternalV1(work);
    }
    xl6_error_result outcome=xl6ErrorScopeEnd(&errors);
    if(!outcome.ok){
        xrtFutureDestroy(future);xl6ErrorScopePropagateFailure(outcome);return NULL;}
    xrtErrorFree(outcome.error);return future;
}
