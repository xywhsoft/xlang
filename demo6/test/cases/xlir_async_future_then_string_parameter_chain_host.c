/* The suite generates this file immediately before compiling this host. */
#include <stdbool.h>
#include "../../build/test/xlir_async_future_then_string_parameter_chain_generated.c"

typedef struct xpromise xpromise;

extern xpromise* xrtPromiseCreate(xfuture**, xcancel*);
extern bool xrtPromiseResolve(xpromise*, void*);
extern bool xrtPromiseReject(xpromise*, const struct xerror*);
extern bool xrtPromiseCancel(xpromise*);
extern void xrtPromiseDestroy(xpromise*);

static const char* xlir_async_then_string_parameter_chain_value = "hello";

xfuture* xlir_make_string_future(void)
{
    xfuture* future = NULL;
    xpromise* promise = xrtPromiseCreate(&future, NULL);
#ifdef XLIR_STRING_PARAMETER_CHAIN_REJECT
    xerror* error = xrtErrorCreate(17, "xlir-test", 18, "expected direct parameter-chain failure");
    if ( error != NULL ) {
        bool rejected = promise != NULL && xrtPromiseReject(promise, error);
        xrtErrorFree(error);
        xrtPromiseDestroy(promise);
        return rejected ? future : NULL;
    }
#endif
#ifdef XLIR_STRING_PARAMETER_CHAIN_CANCEL
    if ( promise != NULL ) {
        bool cancelled = xrtPromiseCancel(promise);
        xrtPromiseDestroy(promise);
        return cancelled ? future : NULL;
    }
#endif
#ifdef XLIR_STRING_PARAMETER_CHAIN_CLOSE
    if ( promise != NULL ) {
        xrtPromiseDestroy(promise);
        return future;
    }
#endif
    if ( promise == NULL || !xrtPromiseResolve(promise, &xlir_async_then_string_parameter_chain_value) ) {
        xrtPromiseDestroy(promise);
        return NULL;
    }
    xrtPromiseDestroy(promise);
    return future;
}

int main(void)
{
#if defined(XLIR_STRING_PARAMETER_CHAIN_REJECT) || defined(XLIR_STRING_PARAMETER_CHAIN_CANCEL) || defined(XLIR_STRING_PARAMETER_CHAIN_CLOSE)
    return xlir_module_init() == 0 ? 0 : 1;
#else
    return xlir_module_init() == 1 ? 0 : 1;
#endif
}
