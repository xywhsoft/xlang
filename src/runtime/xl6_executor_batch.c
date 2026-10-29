#include "xl6_text_value.h"
#include "xl6_execution_language.h"
#include "xl6_execution_resource_internal.h"
#include "xl6_array_admission.h"
#include "src/api/xl_foreign_error.h"

void xl6ExecutorSubmitBatchLanguageV1(XL6_NullaryVoidArrayContextV1 context,void* raw,const xvalue* values)
{
    xl6_error_scope work={0};if(!xl6ErrorScopeBegin(&work)){
        xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});return;}
    XL6_ExecutionScopeV1 execution={0};XL6_Sequence* array=NULL;XL6_ExecutorPreparedBatchV1 batch={0};
    bool ok=context.arrayIdentity&&context.signatureIdentity&&xl6ExecutionResourceEnterV1(raw,true,&execution);
    if(!context.arrayIdentity||!context.signatureIdentity)xrtSetErrorKind(XERR_ARGUMENT);
    if(ok&&!execution.executor){xrtSetErrorKind(XERR_TYPE);ok=false;}
    if(ok){
        array=xl6SequenceBeginRoleV1(values,XL6_SEQUENCE_ARRAY_V1,context.arrayIdentity,context.signatureIdentity,
            3,sizeof(xvalue*),xrtTypeValue());
        ok=array&&xl6SequenceCallablePolicyV1(array,context.signatureIdentity);
        if(array&&!ok)xrtSetErrorKind(XERR_TYPE);
    }
    if(ok){
        xtypedarray* storage=xl6SequenceStorageV1(array);size_t count=xrtTypedArrayCount(storage);
        /* A verified callable-reference recipe owns contiguous xvalue* slots,
         * NOT Closure*. Prepare retains boxes into independent owned jobs. */
        ok=xl6ExecutorPrepareBoxedBatchV1(((XL6_ExecutionResourceV1*)raw)->failures,
            (xvalue* const*)storage->Storage.Data,count,context.signatureIdentity,&batch);
    }
    if(!ok||xrtGetError()||work.failed)xl6ErrorScopeCaptureFailure(&work);
    if(array)xl6SequenceEndV1(array);
    if(xrtGetError()||work.failed)xl6ErrorScopeCaptureFailure(&work);
    /* No source operation pin crosses native admission. There are no borrowed
     * slots, generated adapters or source schemas in the accepted records. */
    if(!work.failed)ok=xl6ExecutorCommitBoxedBatchV1(execution.executor,&batch);
    else ok=xl6ExecutorDiscardBoxedBatchV1(&batch);
    if(!ok||xrtGetError()||work.failed)xl6ErrorScopeCaptureFailure(&work);
    if(execution.resource)xl6ExecutionResourceLeaveV1(&execution);
    xl6_error_result outcome=xl6ErrorScopeEnd(&work);
    if(!outcome.ok)xl6ErrorScopePropagateFailure(outcome);else xrtErrorFree(outcome.error);
}
