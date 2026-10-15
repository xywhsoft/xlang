#include "src/runtime/xl6_process_live.h"
#include "src/runtime/xl6_future_payload.h"
#include "src/runtime/xl6_future_callback.h"

/* The source owns a native exit-status snapshot, not a language bool box.
 * This resident continuation publishes only readiness with the same physical
 * bool payload ABI as generated async functions. No script callback or child
 * process handle is captured. The Process async frame owns its receiver until
 * it constructs the independent ProcessExitStatus after the await. */
static void processReady(const xfutureresult* input, xpromise* output, xcancel* cancel, void* context)
{
    xerror* prior = xrtTakeError();
    xl6FutureCallbackRuntimeV4Get()->Cleanup(context);
    if(xrtCancelRequested(cancel)) { (void)xrtPromiseCancel(output);goto done; }
    if(input && input->State==XFUTURE_FAILED && input->Error) {
        (void)xrtPromiseReject(output,input->Error);goto done;
    }
    if(input && input->State==XFUTURE_CANCELLED) { (void)xrtPromiseCancel(output);goto done; }
    if (input==NULL || input->State != XFUTURE_RESOLVED || input->Value == NULL) {
        (void)xrtPromiseClose(output);
        goto done;
    }
    bool* ready = xrtMalloc(sizeof(*ready));
    if (ready == NULL) {
        const xerror* error = xrtGetError();
        if (error == NULL || !xrtPromiseReject(output, error)) (void)xrtPromiseClose(output);
        goto done;
    }
    *ready = true;
    if (!xl6PayloadResolveOwned(output, ready, XL6_FUTURE_PAYLOAD_VALUE)) {
        xrtFree(ready);
        (void)xrtPromiseClose(output);
    }
done:
    xrtClearError();
    xrtSetErrorTake(prior);
}

xfuture* xl6ProcessWaitReadyAsyncV1(void* handle)
{
    XL6_ProcessLive* process = xl6ProcessLiveCheck(handle);
    if (process == NULL) return NULL;
    xfuture* shared = xrtProcessWaitAsync(process->native);
    if (shared == NULL) return NULL;
    /* Reuse the resident, certified physical callback frame, not an opaque
     * legacy Then waiter. There are no captures or generated code owners.
     * Nonexclusive Start keeps cancellation local to this consumer: the
     * process-wide shared terminal Future MUST NOT inherit CancelSource. */
    static const XL6_CaptureLayoutV2 empty={sizeof(empty),1,0,NULL};
    const XL6_FutureCallbackRuntimeV1* callbacks=xl6FutureCallbackRuntimeV1Get();
    void* frame=xl6FutureCallbackRuntimeV4Get()->Allocate(NULL,1,processReady,&empty);
    xfuture* result=frame?callbacks->Start(frame,shared):NULL;
    if(frame && !result) {
        xerror* failure=xrtTakeError();callbacks->Discard(frame);
        xrtClearError();xrtSetErrorTake(failure);
    }
    xrtFutureDestroy(shared);
    return result;
}
