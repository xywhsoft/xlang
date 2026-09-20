/* The suite generates this file immediately before compiling this host. */
#include <stdbool.h>
#include <stdint.h>
#include "../../build/test/xlir_async_future_three_awaits_mixed_generated.c"

typedef struct xpromise xpromise;

extern xpromise* xrtPromiseCreate(xfuture**, void*);
extern bool xrtPromiseResolve(xpromise*, void*);
extern void xrtPromiseDestroy(xpromise*);

static int64_t xlir_three_await_mixed_value = 41;

xfuture* xlir_make_i64_future(void)
{
    xfuture* future = NULL;
    xpromise* promise = xrtPromiseCreate(&future, NULL);
    if ( promise == NULL || !xrtPromiseResolve(promise, &xlir_three_await_mixed_value) ) {
        xrtPromiseDestroy(promise);
        return NULL;
    }
    xrtPromiseDestroy(promise);
    return future;
}

int main(void)
{
    return xlir_module_init() == 1 ? 0 : 1;
}
