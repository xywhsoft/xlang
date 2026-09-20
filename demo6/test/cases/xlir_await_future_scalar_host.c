/* The suite generates this file immediately before compiling this host. */
#include <stdbool.h>
#include "../../build/test/xlir_await_future_scalar_generated.c"

typedef struct xpromise xpromise;

extern xpromise* xrtPromiseCreate(xfuture**, void*);
extern bool xrtPromiseResolve(xpromise*, void*);
extern void xrtPromiseDestroy(xpromise*);

static bool xlir_bool_value = true;
static double xlir_f64_value = 2.5;

static xfuture* xlir_make_resolved_future(void* value)
{
    xfuture* future = NULL;
    xpromise* promise = xrtPromiseCreate(&future, NULL);
    if ( promise == NULL || !xrtPromiseResolve(promise, value) ) {
        xrtPromiseDestroy(promise);
        return NULL;
    }
    xrtPromiseDestroy(promise);
    return future;
}

xfuture* xlir_make_bool_future(void)
{
    return xlir_make_resolved_future(&xlir_bool_value);
}

xfuture* xlir_make_f64_future(void)
{
    return xlir_make_resolved_future(&xlir_f64_value);
}

int main(void)
{
    return xlir_module_init() == 1 ? 0 : 1;
}
