/* The suite generates this file immediately before compiling this host. */
#include <stdbool.h>
#include <stdint.h>
#include "../../build/test/xlir_async_future_three_awaits_generated.c"

typedef struct xpromise xpromise;

extern xpromise* xrtPromiseCreate(xfuture**, void*);
extern bool xrtPromiseResolve(xpromise*, void*);
extern bool xrtPromiseReject(xpromise*, const struct xerror*);
extern bool xrtPromiseCancel(xpromise*);
extern void xrtPromiseDestroy(xpromise*);

static int64_t xlir_three_await_value = 41;
static int xlir_three_await_calls = 0;

xfuture* xlir_make_i64_future(void)
{
    xfuture* future = NULL;
    xpromise* promise = xrtPromiseCreate(&future, NULL);
    xerror* error = NULL;
    xlir_three_await_calls += 1;
#ifdef XLIR_CHAIN_THIRD_REJECT
    if ( xlir_three_await_calls == 3 ) error = xrtErrorCreate(15, "xlir-test", 1, "expected chain failure");
#endif
#ifdef XLIR_CHAIN_THIRD_CANCEL
    if ( xlir_three_await_calls == 3 ) {
        bool cancelled = promise != NULL && xrtPromiseCancel(promise);
        xrtPromiseDestroy(promise);
        return cancelled ? future : NULL;
    }
#endif
#ifdef XLIR_CHAIN_THIRD_CLOSE
    if ( xlir_three_await_calls == 3 ) {
        xrtPromiseDestroy(promise);
        return future;
    }
#endif
    if ( error != NULL ) {
        bool rejected = promise != NULL && xrtPromiseReject(promise, error);
        xrtErrorFree(error);
        xrtPromiseDestroy(promise);
        return rejected ? future : NULL;
    }
    if ( promise == NULL || !xrtPromiseResolve(promise, &xlir_three_await_value) ) {
        xrtPromiseDestroy(promise);
        return NULL;
    }
    xrtPromiseDestroy(promise);
    return future;
}

int main(void)
{
#if defined(XLIR_CHAIN_THIRD_REJECT) || defined(XLIR_CHAIN_THIRD_CANCEL) || defined(XLIR_CHAIN_THIRD_CLOSE)
    return xlir_module_init() == 0 ? 0 : 1;
#else
    return xlir_module_init() == 1 ? 0 : 1;
#endif
}
