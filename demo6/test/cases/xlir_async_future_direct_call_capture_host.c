/* The suite generates this file immediately before compiling this host. */
#include <stdbool.h>
#include <stdint.h>
#include "../../build/test/xlir_async_future_direct_call_capture_generated.c"

typedef struct xpromise xpromise;

extern xpromise* xrtPromiseCreate(xfuture**, xcancel*);
extern bool xrtPromiseResolve(xpromise*, void*);
extern bool xrtPromiseReject(xpromise*, const struct xerror*);
extern void xrtPromiseDestroy(xpromise*);

static int64_t xlir_async_direct_call_capture_value = 41;

xfuture* xlir_make_i64_future(void)
{
    xfuture* future = NULL;
    xpromise* promise = xrtPromiseCreate(&future, NULL);
#ifdef XLIR_ASYNC_DIRECT_CALL_REJECT
    xerror* error = xrtErrorCreate(15, "xlir-test", 1, "expected direct-call failure");
    if ( error != NULL ) {
        bool rejected = promise != NULL && xrtPromiseReject(promise, error);
        xrtErrorFree(error);
        xrtPromiseDestroy(promise);
        return rejected ? future : NULL;
    }
#endif
    if ( promise == NULL || !xrtPromiseResolve(promise, &xlir_async_direct_call_capture_value) ) {
        xrtPromiseDestroy(promise);
        return NULL;
    }
    xrtPromiseDestroy(promise);
    return future;
}

int main(void)
{
#ifdef XLIR_ASYNC_DIRECT_CALL_REJECT
    return xlir_module_init() == 0 ? 0 : 1;
#else
    return xlir_module_init() == 1 ? 0 : 1;
#endif
}
