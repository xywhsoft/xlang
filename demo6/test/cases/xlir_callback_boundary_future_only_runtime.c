#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_ERROR
#define XRT_MODULE_FUTURE
#include "lib/xrt/xrt.h"
#include <assert.h>
#include <stdio.h>
extern int callback_probe_init(void);
extern void callback_probe_fini(void);
extern xfuture* callback_probe_start(xfuture*);
static unsigned dropped;
static int raising;
static xerror* cleanup;
static int64_t payload = 10;
void callback_boundary_drop(void) { ++dropped; if (raising) xrtSetError(cleanup); }
xfuture* callback_boundary_input(void)
{
    xfuture* future = NULL; xpromise* promise = xrtPromiseCreate(&future, NULL);
    assert(promise && xrtPromiseResolve(promise, &payload)); xrtPromiseDestroy(promise); return future;
}
int main(void)
{
    unsigned mode, failures = 0;
    xmemdebugsnapshot before, after;
    assert(xrtMemDebugEnable(true)); xrtMemDebugSnapshot(&before);
    cleanup = xrtErrorCreate(XERR_STATE, "callback", 77, "capture destructor failed");
    assert(cleanup && callback_probe_init());
    for (mode = 0; mode < 2; ++mode) {
        xfuture *input = NULL, *result;
        xpromise* promise = xrtPromiseCreate(&input, NULL);
        const xerror* error;
        dropped = 0; raising = (int)mode;
        assert(promise && input); result = callback_probe_start(input);
        assert(result && !xrtFutureDone(result) && !dropped && !xrtGetError());
        assert(xrtPromiseResolve(promise, &payload)); xrtPromiseDestroy(promise);
        assert(xrtFutureWait(result) == 0); error = xrtFutureError(result);
        printf("callback cleanup raising=%u state=%d code=%d drops=%u thread_error=%d\n", mode, (int)xrtFutureState(result),
            error ? xrtErrorCode(error) : 0, dropped, xrtGetError() ? xrtErrorCode(xrtGetError()) : 0);
        if (xrtFutureState(result) != (mode ? XFUTURE_FAILED : XFUTURE_RESOLVED) || dropped != 1 ||
            (mode && (!error || xrtErrorCode(error) != 77)) || xrtGetError()) ++failures;
        xrtClearError(); xrtFutureDestroy(result); xrtFutureDestroy(input);
    }
    callback_probe_fini(); xrtErrorFree(cleanup); xrtClearError();
    xrtMemDebugSnapshot(&after);
    assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
    assert(after.AllocCount - before.AllocCount == after.FreeCount - before.FreeCount);
    return failures ? 1 : 0;
}
