/* The suite generates this file immediately before compiling this host. */
#include <stdbool.h>
#include <stdint.h>
#include "../../build/test/xlir_async_future_direct_xvalue_capture_generated.c"
#define XLIR_TEST_LAUNCH_FUTURE
#include "xlir_launch_test.h"

typedef struct xpromise xpromise;

extern xpromise* xrtPromiseCreate(xfuture**, void*);
extern bool xrtPromiseResolve(xpromise*, void*);
extern void xrtPromiseDestroy(xpromise*);
extern int xrtFutureWait(xfuture*);
extern void* xrtFutureValue(const xfuture*);
extern bool xrtValueGetInt(const xvalue*, int64_t*);

static xvalue* xlir_async_direct_xvalue_source;

xfuture* xlir_make_xvalue_future(void)
{
    xfuture* future = NULL;
    xpromise* promise = xrtPromiseCreate(&future, NULL);
    if ( promise == NULL || !xrtPromiseResolve(promise, &xlir_async_direct_xvalue_source) ) {
        xrtPromiseDestroy(promise);
        return NULL;
    }
    xrtPromiseDestroy(promise);
    return future;
}

int main(void)
{
    xfuture* pending;
    xvalue* supplied;
    xvalue* value;
    void* raw;
    int64_t number = 0;
    xlir_async_direct_xvalue_source = xrtValueInt(41);
    supplied = xrtValueInt(1);
    if ( xlir_async_direct_xvalue_source == NULL || supplied == NULL ) {
        xrtValueRelease(xlir_async_direct_xvalue_source);
        xrtValueRelease(supplied);
        return 1;
    }
    pending = xlir_test_observe_future_launch(xlir_async_start_2(supplied));
    xrtValueRelease(supplied);
    if ( pending == NULL || xrtFutureWait(pending) != 0 ) {
        xrtFutureDestroy(pending);
        xrtValueRelease(xlir_async_direct_xvalue_source);
        return 2;
    }
    raw = xrtFutureValue(pending);
    value = raw != NULL ? *(xvalue**)raw : NULL;
    if ( value == NULL || !xrtValueGetInt(value, &number) || number != 42 ) {
        xrtFutureDestroy(pending);
        xrtValueRelease(xlir_async_direct_xvalue_source);
        return 3;
    }
    xrtFutureDestroy(pending);
    xrtValueRelease(xlir_async_direct_xvalue_source);
    return 0;
}
