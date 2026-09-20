/* The suite generates this file immediately before compiling this host. */
#include <stdbool.h>
#include "../../build/test/xlir_async_future_direct_call_catch_generated.c"

typedef struct xpromise xpromise;

extern xpromise* xrtPromiseCreate(xfuture**, void*);
extern bool xrtPromiseReject(xpromise*, const struct xerror*);
extern void xrtPromiseDestroy(xpromise*);

xfuture* xlir_make_i64_future(void)
{
    xfuture* future = NULL;
    xpromise* promise = xrtPromiseCreate(&future, NULL);
    xerror* error = xrtErrorCreate(15, "xlir-test", 1, "expected direct-call failure");
    bool rejected = promise != NULL && error != NULL && xrtPromiseReject(promise, error);
    xrtErrorFree(error);
    xrtPromiseDestroy(promise);
    return rejected ? future : NULL;
}

int main(void)
{
    return xlir_module_init() == 1 ? 0 : 1;
}
