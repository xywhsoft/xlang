#include "src/runtime/xl6_process_live.h"
#include "src/runtime/xl6_future_payload.h"

/* The source owns a native exit-status snapshot, not a language bool box.
 * This resident continuation publishes only readiness with the same physical
 * bool payload ABI as generated async functions. No script callback or child
 * process handle is captured. The Process async frame owns its receiver until
 * it constructs the independent ProcessExitStatus after the await. */
static void processReady(const xfutureresult* input, xpromise* output, void* context)
{
    xerror* prior = xrtTakeError();
    (void)context;
    if (input->State != XFUTURE_RESOLVED || input->Value == NULL) {
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
    /* Ordinary Then has one-way cancellation. CancelSource is specifically
     * forbidden: WaitAsync returns the process-wide shared terminal Future. */
    xfuture* result = xrtFutureThen(shared, processReady, NULL);
    xrtFutureDestroy(shared);
    return result;
}
