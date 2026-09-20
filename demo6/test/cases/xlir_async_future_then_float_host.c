/* The suite generates this file immediately before compiling this host. */
#include <stdbool.h>
#include <stdint.h>
#include "../../build/test/xlir_async_future_then_float_generated.c"

typedef struct xpromise xpromise;
extern xpromise* xrtPromiseCreate(xfuture**, void*);
extern bool xrtPromiseResolve(xpromise*, void*);
extern void xrtPromiseDestroy(xpromise*);

static double xlir_float_value = 4.0;

xfuture* xlir_make_f64_future(void)
{
    xfuture* future = NULL;
    xpromise* promise = xrtPromiseCreate(&future, NULL);
    if ( promise == NULL || !xrtPromiseResolve(promise, &xlir_float_value) ) {
        xrtPromiseDestroy(promise);
        return NULL;
    }
    xrtPromiseDestroy(promise);
    return future;
}

int main(void)
{
    return xlir_module_init() == 1 && s7 == 10.0 ? 0 : 1;
}
