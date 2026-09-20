/* The suite generates this file immediately before compiling this host. */
#include <stdbool.h>
#include <stdint.h>
#include "../../build/test/xlir_async_future_two_awaits_generated.c"

typedef struct xpromise xpromise;

extern xpromise* xrtPromiseCreate(xfuture**, void*);
extern bool xrtPromiseResolve(xpromise*, void*);
extern bool xrtPromiseReject(xpromise*, const struct xerror*);
extern void xrtPromiseDestroy(xpromise*);

static int64_t xlir_two_await_value = 41;
static int xlir_two_await_calls = 0;

xfuture* xlir_make_i64_future(void)
{
    xfuture* future = NULL;
    xpromise* promise = xrtPromiseCreate(&future, NULL);
    xerror* error = NULL;
    xlir_two_await_calls += 1;
#ifdef XLIR_CHAIN_SECOND_REJECT
    if ( xlir_two_await_calls == 2 ) error = xrtErrorCreate(15, "xlir-test", 1, "expected chain failure");
#endif
    if ( error != NULL ) {
        bool rejected = promise != NULL && xrtPromiseReject(promise, error);
        xrtErrorFree(error);
        xrtPromiseDestroy(promise);
        return rejected ? future : NULL;
    }
    if ( promise == NULL || !xrtPromiseResolve(promise, &xlir_two_await_value) ) {
        xrtPromiseDestroy(promise);
        return NULL;
    }
    xrtPromiseDestroy(promise);
    return future;
}

int main(void)
{
#ifdef XLIR_CHAIN_SECOND_REJECT
    return xlir_module_init() == 0 ? 0 : 1;
#else
    return xlir_module_init() == 1 ? 0 : 1;
#endif
}
