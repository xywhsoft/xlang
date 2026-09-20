/* The suite generates this file immediately before compiling this host. */
#include <stdbool.h>
#include <stdint.h>
#include "../../build/test/xlir_async_future_capture_chain_generated.c"

typedef struct xpromise xpromise;

extern xpromise* xrtPromiseCreate(xfuture**, void*);
extern bool xrtPromiseResolve(xpromise*, void*);
extern bool xrtPromiseReject(xpromise*, const struct xerror*);
extern bool xrtPromiseCancel(xpromise*);
extern void xrtPromiseDestroy(xpromise*);

static int64_t xlir_capture_chain_value = 41;

xfuture* xlir_make_i64_future(void)
{
    xfuture* future = NULL;
    xpromise* promise = xrtPromiseCreate(&future, NULL);
#ifdef XLIR_CAPTURE_CHAIN_REJECT
    xerror* error = xrtErrorCreate(15, "xlir-test", 1, "expected capture-chain failure");
    if ( error != NULL ) {
        bool rejected = promise != NULL && xrtPromiseReject(promise, error);
        xrtErrorFree(error);
        xrtPromiseDestroy(promise);
        return rejected ? future : NULL;
    }
#endif
#ifdef XLIR_CAPTURE_CHAIN_CANCEL
    if ( promise != NULL ) {
        bool cancelled = xrtPromiseCancel(promise);
        xrtPromiseDestroy(promise);
        return cancelled ? future : NULL;
    }
#endif
#ifdef XLIR_CAPTURE_CHAIN_CLOSE
    if ( promise != NULL ) {
        xrtPromiseDestroy(promise);
        return future;
    }
#endif
    if ( promise == NULL || !xrtPromiseResolve(promise, &xlir_capture_chain_value) ) {
        xrtPromiseDestroy(promise);
        return NULL;
    }
    xrtPromiseDestroy(promise);
    return future;
}

int main(void)
{
#if defined(XLIR_CAPTURE_CHAIN_REJECT) || defined(XLIR_CAPTURE_CHAIN_CANCEL) || defined(XLIR_CAPTURE_CHAIN_CLOSE)
    return xlir_module_init() == 0 ? 0 : 1;
#else
    return xlir_module_init() == 1 ? 0 : 1;
#endif
}
