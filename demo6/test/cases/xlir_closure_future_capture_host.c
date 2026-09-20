#include <stdint.h>
#include "../../build/test/xlir_closure_future_capture_generated.c"
#define XLIR_TEST_LAUNCH_FUTURE
#include "xlir_launch_test.h"

typedef struct xpromise xpromise;

extern xpromise* xrtPromiseCreate(xfuture**, void*);
extern bool xrtPromiseResolve(xpromise*, void*);
extern void xrtPromiseDestroy(xpromise*);
extern void xrtFutureDestroy(xfuture*);
extern int xrtFutureWait(xfuture*);
extern void* xrtFutureValue(const xfuture*);

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
    xpromise* promise = xrtPromiseCreate(&future, NULL);
    xfuture* result;
    if ( promise == NULL || !xrtPromiseResolve(promise, &value) ) {
        xrtPromiseDestroy(promise);
        return 1;
    }
    xrtPromiseDestroy(promise);
    result = xlir_test_observe_future_launch(xlir_async_start_2(future));
    xrtFutureDestroy(future);
    if ( result == NULL || xrtFutureWait(result) != 0 ||
         xrtFutureValue(result) == NULL || *(int64_t*)xrtFutureValue(result) != 42 ) {
        xrtFutureDestroy(result);
        return 1;
    }
    xrtFutureDestroy(result);
    return 0;
}
