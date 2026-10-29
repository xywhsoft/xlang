#define XRT_MODULE_TASK
#include "src/common/xl6_xrt.h"
#include "xl6_task_result_closure.h"
#include "xl6_task_frame_internal.h"
#include "src/api/xl_foreign_error.h"
#include <stddef.h>

typedef struct Work {
    XL6_Closure* closure;
    bool (*Invoke)(XL6_Closure*,void*);
    XL6_FutureValueProfileV1 profile;
} Work;
static XL6_TaskOutcome run(xcancel* cancel,void* raw,XL6_TaskResult* output)
{
    (void)cancel;Work* work=raw;void* value=NULL;
    if(!xl6FutureValueAllocateV1(work->profile,&value))return XL6_TASK_FAILED;
    const xfuturepayloadownershipv1* policy=xl6FutureValuePolicyV1(work->profile);
    *output=(XL6_TaskResult){value,value?policy->Drop:NULL,NULL};
    XL6_ClosureScope entered={0};const XL6_ClosureRuntimeV1* closures=xl6ClosureRuntimeV1Get();
    closures->Enter(&entered,work->closure);bool ok=work->Invoke(work->closure,value);
    /* Native Task owns partial cells on both outcomes. The common frame covers
     * this call and its code/activity tails; capture Drop settles before the
     * same language output Future can publish. */
    if(!ok)xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});
    closures->Leave(&entered);return ok?XL6_TASK_SUCCESS:XL6_TASK_FAILED;
}
static const XL6_CaptureSlotV2 slots[]={{{XL6_CAPTURE_CLOSURE,offsetof(Work,closure),sizeof(XL6_Closure*)},NULL}};
static const XL6_CaptureLayoutV2 layout={sizeof(layout),sizeof(Work),1,slots};
void* xl6TaskResultClosurePrepareV1(XL6_FutureValueProfileV1 profile,XL6_CheckedResultClosureBorrowV1 borrow)
{
    xl6_error_scope errors={0};if(!xl6ErrorScopeBegin(&errors)){
        xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});return NULL;}
    const XL6_ClosureRuntimeV1* closures=xl6ClosureRuntimeV1Get();Work* work=NULL;
    bool valid=xl6FutureValuePolicyV1(profile)&&borrow.closure&&borrow.Invoke&&borrow.closure->invoke&&
        !borrow.closure->call_parameter_count&&xl6ClosureOwnershipAdapterV1(closures->Ownership(borrow.closure));
    if(!valid)xrtSetErrorKind(XERR_ARGUMENT);
    if(valid){
        work=xl6TaskRuntimeV3Get()->Allocate(borrow.adapterOwner,sizeof(*work),run,&layout);
        if(work){*work=(Work){NULL,borrow.Invoke,profile};work->closure=closures->Retain(borrow.closure);}
    }
    if(!work||!work->closure||errors.failed||xrtGetError()){
        xl6ErrorScopeCaptureFailure(&errors);
        if(work)xl6TaskFrameDiscardInternalV1(work);
        work=NULL;
    }
    xl6_error_result outcome=xl6ErrorScopeEnd(&errors);
    if(!outcome.ok){xl6ErrorScopePropagateFailure(outcome);return NULL;}
    xrtErrorFree(outcome.error);return work;
}
