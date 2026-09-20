/* The suite generates this file immediately before compiling this host. */
#include <stdbool.h>
#include <stdint.h>
#include "../../build/test/xlir_async_future_then_xvalue_local_parameter_generated.c"
#define XLIR_TEST_LAUNCH_FUTURE
#include "xlir_launch_test.h"

typedef struct xpromise xpromise;
typedef struct xerror xerror;

extern xpromise* xrtPromiseCreate(xfuture**, void*);
extern bool xrtPromiseResolve(xpromise*, void*);
extern bool xrtPromiseReject(xpromise*, const struct xerror*);
extern bool xrtPromiseCancel(xpromise*);
extern void xrtPromiseDestroy(xpromise*);
extern xerror* xrtErrorCreate(int, const char*, int, const char*);
extern void xrtErrorFree(xerror*);
extern int xrtFutureWait(xfuture*);
extern xfuturestate xrtFutureState(const xfuture*);
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
    xvalue* supplied = xrtValueInt(5);
    xvalue* value;
    void* raw;
    int64_t number = 0;
    if ( input == NULL || supplied == NULL ) return 21;
    promise = xrtPromiseCreate(&source, NULL);
#ifdef XLIR_XVALUE_LOCAL_PARAMETER_REJECT
    {
        xerror* error = xrtErrorCreate(15, "xlir-test", 1, "expected dynamic continuation failure");
        bool rejected = promise != NULL && error != NULL && xrtPromiseReject(promise, error);
        xrtErrorFree(error);
        xrtPromiseDestroy(promise);
        if ( !rejected ) { xrtValueRelease(input); xrtValueRelease(supplied); return 22; }
    }
#elif defined(XLIR_XVALUE_LOCAL_PARAMETER_CANCEL)
    {
        bool cancelled = promise != NULL && xrtPromiseCancel(promise);
        xrtPromiseDestroy(promise);
        if ( !cancelled ) { xrtValueRelease(input); xrtValueRelease(supplied); return 23; }
    }
#else
    if ( promise == NULL || !xrtPromiseResolve(promise, &input) ) {
        xrtPromiseDestroy(promise);
        xrtValueRelease(input);
        xrtValueRelease(supplied);
        return 1;
    }
    xrtPromiseDestroy(promise);
#endif
    pending = xlir_test_observe_future_launch(xlir_async_start_1(source, supplied));
    xrtFutureDestroy(source);
    xrtValueRelease(supplied);
#if defined(XLIR_XVALUE_LOCAL_PARAMETER_REJECT) || defined(XLIR_XVALUE_LOCAL_PARAMETER_CANCEL)
    if ( pending == NULL ) {
        xrtValueRelease(input);
        return 11;
    }
    if ( xrtFutureWait(pending) != 0 ) {
        xrtFutureDestroy(pending);
        xrtValueRelease(input);
        return 12;
    }
    if (
#if defined(XLIR_XVALUE_LOCAL_PARAMETER_REJECT)
         xrtFutureState(pending) != XFUTURE_FAILED ) {
#else
         xrtFutureState(pending) != XFUTURE_CANCELLED ) {
#endif
        xrtFutureDestroy(pending);
        xrtValueRelease(input);
        return 13;
    }
    xrtFutureDestroy(pending);
    xrtValueRelease(input);
    return 0;
#else
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
#endif
}
