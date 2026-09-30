/* The suite generates this file immediately before compiling this host. */
#include <stdbool.h>
#include "../../build/test/xlir_async_future_then_string_local_generated.c"

typedef struct xpromise xpromise;

extern xpromise* xrtPromiseCreate(xfuture**, xcancel*);
extern bool xrtPromiseResolve(xpromise*, void*);
extern void xrtPromiseDestroy(xpromise*);

static const char* xlir_async_then_string_local_value = "hello";

xfuture* xlir_make_string_future(void)
{
    xfuture* future = NULL;
    xpromise* promise = xrtPromiseCreate(&future, NULL);
    if ( promise == NULL || !xrtPromiseResolve(promise, &xlir_async_then_string_local_value) ) {
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
