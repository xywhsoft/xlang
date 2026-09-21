#include <stdint.h>
#include "../../build/test/xlir_closure_future_capture_generated.c"
#define XLIR_TEST_LAUNCH_FUTURE
#include "xlir_launch_test.h"

static int64_t xlir_closure_future_capture_value = 42;

xfuture* xlir_make_i64_future(void)
{
    xfuture* future = NULL;
    xpromise* promise = xrtPromiseCreate(&future, NULL);
    if ( promise == NULL ||
         !xrtPromiseResolve(promise, &xlir_closure_future_capture_value) ) {
        xrtPromiseDestroy(promise);
        return NULL;
    }
    xrtPromiseDestroy(promise);
    return future;
}

int main(void)
{
    int64_t value = 42;
    xfuture* future = NULL;
    xpromise* promise = NULL;
    xfuture* result = NULL;
    int wait_result;
    const xerror* error;
    void* result_value;
    int exit_code = 1;

    if (!xlir_module_init()) {
        fprintf(stderr, "closure future capture module initialization failed\n");
        goto finish;
    }
    promise = xrtPromiseCreate(&future, NULL);
    if (promise == NULL || !xrtPromiseResolve(promise, &value)) {
        fprintf(stderr, "closure future capture source creation failed\n");
        goto finish;
    }
    xrtPromiseDestroy(promise);
    promise = NULL;
    result = xlir_test_observe_future_launch(xlir_async_start_2(future));
    xrtFutureDestroy(future);
    future = NULL;
    if (result == NULL) {
        fprintf(stderr, "closure future capture returned no result future\n");
        goto finish;
    }
    wait_result = xrtFutureWait(result);
    error = xrtFutureError(result);
    result_value = xrtFutureValue(result);
    if ( wait_result != 0 || result_value == NULL || *(int64_t*)result_value != 42 ) {
        fprintf(stderr,
                "closure future capture failed: wait=%d state=%d kind=%d domain=%s message=%s value=%p\n",
                wait_result,
                (int)xrtFutureState(result),
                error != NULL ? (int)xrtErrorKind(error) : -1,
                error != NULL && xrtErrorDomain(error) != NULL ? xrtErrorDomain(error) : "",
                error != NULL && xrtErrorMessage(error) != NULL ? xrtErrorMessage(error) : "",
                result_value);
        goto finish;
    }
    exit_code = 0;

finish:
    xrtPromiseDestroy(promise);
    xrtFutureDestroy(future);
    xrtFutureDestroy(result);
    xlir_module_fini();
    if (xrtGetError() != NULL) {
        error = xrtGetError();
        fprintf(stderr,
                "closure future capture module finalization failed: kind=%d domain=%s message=%s\n",
                (int)xrtErrorKind(error),
                xrtErrorDomain(error) != NULL ? xrtErrorDomain(error) : "",
                xrtErrorMessage(error) != NULL ? xrtErrorMessage(error) : "");
        xrtClearError();
        exit_code = 1;
    }
    return exit_code;
}
