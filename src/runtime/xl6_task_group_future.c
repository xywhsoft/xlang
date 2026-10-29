#include "src/runtime/xl6_text_value.h"
#include "src/runtime/xl6_future_callback.h"
#include "src/api/xl_task_group_bridge.h"
#include "src/api/xl_foreign_error.h"

/* One consumer owns one output Promise. Cancelling a join must not cancel the
 * group's shared completion Future or another joiner. There is no language
 * capture or generated-code address in this permanent resident callback. */
static void taskGroupReady(const xfutureresult* input,xpromise* output,xcancel* cancel,void* context)
{
    xerror* prior=xrtTakeError();
    xl6FutureCallbackRuntimeV4Get()->Cleanup(context);
    if(xrtCancelRequested(cancel) || (input && input->State==XFUTURE_CANCELLED))(void)xrtPromiseCancel(output);
    else if(input && input->State==XFUTURE_FAILED && input->Error)(void)xrtPromiseReject(output,input->Error);
    else if(input && input->State==XFUTURE_RESOLVED)(void)xrtPromiseResolve(output,NULL);
    else (void)xrtPromiseClose(output);
    xrtClearError();xrtSetErrorTake(prior);
}
xfuture* xl6_xrt_task_group_join_async(void* group)
{
    if(!group){xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    if(!xl6TaskGroupClose(group) && xrtGetError())return NULL;
    xfuture* shared=xl6TaskGroupFuture(group);if(!shared)return NULL;
    static const XL6_CaptureLayoutV2 empty={sizeof(empty),1,0,NULL};
    const XL6_FutureCallbackRuntimeV1* callbacks=xl6FutureCallbackRuntimeV1Get();
    void* frame=xl6FutureCallbackRuntimeV4Get()->Allocate(NULL,1,taskGroupReady,&empty);
    xfuture* result=frame?callbacks->Start(frame,shared):NULL;
    if(frame && !result){
        xerror* failure=xrtTakeError();callbacks->Discard(frame);
        xrtClearError();xrtSetErrorTake(failure);
    }
    xrtFutureDestroy(shared);
    if(!result)xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});
    return result;
}
