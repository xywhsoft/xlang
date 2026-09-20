/* The suite generates this file immediately before compiling this host. */
#include <stdbool.h>
#include <stdint.h>
#include "../../build/test/xlir_async_future_then_xvalue_generated.c"
#define XLIR_TEST_LAUNCH_FUTURE
#include "xlir_launch_test.h"

typedef struct xpromise xpromise;

extern xpromise* xrtPromiseCreate(xfuture**, void*);
extern bool xrtPromiseResolve(xpromise*, void*);
extern void xrtPromiseDestroy(xpromise*);
extern int xrtFutureWait(xfuture*);
extern void* xrtFutureValue(const xfuture*);
extern bool xrtValueGetInt(const xvalue*, int64_t*);

xfuture* xlir_test_unreachable_xvalue_future(void)
{
    return NULL;
}

int main(void)
{
    xfuture* source = NULL;
    xfuture* pending;
    xpromise* promise;
    xvalue* input = xrtValueInt(41);
    xvalue* offset = xrtValueInt(5);
    xvalue* value;
    void* raw;
    int64_t number = 0;
    if ( input == NULL || offset == NULL ) return 1;
    promise = xrtPromiseCreate(&source, NULL);
    if ( promise == NULL || !xrtPromiseResolve(promise, &input) ) {
        xrtPromiseDestroy(promise);
        xrtValueRelease(input);
        xrtValueRelease(offset);
        return 1;
    }
    xrtPromiseDestroy(promise);
    pending = xlir_test_observe_future_launch(xlir_async_start_1(source, offset));
    xrtFutureDestroy(source);
    xrtValueRelease(offset);
    if ( pending == NULL || xrtFutureWait(pending) != 0 ) {
        xrtFutureDestroy(pending);
        xrtValueRelease(input);
        return 1;
    }
    raw = xrtFutureValue(pending);
    value = raw != NULL ? *(xvalue**)raw : NULL;
    if ( value == NULL || !xrtValueGetInt(value, &number) || number != 46 ) {
        xrtFutureDestroy(pending);
        xrtValueRelease(input);
        return 1;
    }
    xrtFutureDestroy(pending);
    xrtValueRelease(input);
    return 0;
}
