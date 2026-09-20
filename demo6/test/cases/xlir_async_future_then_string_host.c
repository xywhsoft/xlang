/* The suite generates this file immediately before compiling this host. */
#include <stdbool.h>
#include "../../build/test/xlir_async_future_then_string_generated.c"

typedef struct xpromise xpromise;

extern xpromise* xrtPromiseCreate(xfuture**, void*);
extern bool xrtPromiseResolve(xpromise*, void*);
extern bool xrtPromiseReject(xpromise*, const struct xerror*);
extern bool xrtPromiseCancel(xpromise*);
extern void xrtPromiseDestroy(xpromise*);

static const char* xlir_async_then_string_value = "hello";

xfuture* xlir_make_string_future(void)
{
    xfuture* future = NULL;
    xpromise* promise = xrtPromiseCreate(&future, NULL);
#ifdef XLIR_THEN_STRING_REJECT
    xerror* error = xrtErrorCreate(15, "xlir-test", 1, "expected string continuation failure");
    if ( error != NULL ) {
        bool rejected = promise != NULL && xrtPromiseReject(promise, error);
        xrtErrorFree(error);
        xrtPromiseDestroy(promise);
        return rejected ? future : NULL;
    }
#endif
#ifdef XLIR_THEN_STRING_CANCEL
    if ( promise != NULL ) {
        bool cancelled = xrtPromiseCancel(promise);
        xrtPromiseDestroy(promise);
        return cancelled ? future : NULL;
    }
#endif
#ifdef XLIR_THEN_STRING_CLOSE
    if ( promise != NULL ) {
        xrtPromiseDestroy(promise);
        return future;
    }
#endif
    if ( promise == NULL || !xrtPromiseResolve(promise, &xlir_async_then_string_value) ) {
        xrtPromiseDestroy(promise);
        return NULL;
    }
    xrtPromiseDestroy(promise);
    return future;
}

int main(void)
{
#if defined(XLIR_THEN_STRING_REJECT) || defined(XLIR_THEN_STRING_CANCEL) || defined(XLIR_THEN_STRING_CLOSE)
    return xlir_module_init() == 0 ? 0 : 1;
#else
    return xlir_module_init() == 1 ? 0 : 1;
#endif
}
