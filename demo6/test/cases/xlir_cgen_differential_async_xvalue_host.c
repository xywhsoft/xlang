/* XLIR emission exposes module initialization rather than a process entry. */
#include <stdbool.h>
#include <stdint.h>
#include "../../build/test/xlir_cgen_differential_generated.c"

typedef struct xpromise xpromise;

extern xpromise* xrtPromiseCreate(xfuture**, xcancel*);
extern bool xrtPromiseResolve(xpromise*, void*);
extern void xrtPromiseDestroy(xpromise*);
extern bool xrtValueGetInt(const xvalue*, int64_t*);

static xvalue* xlir_cgen_differential_async_xvalue = NULL;

bool xlir_check_xvalue_46(xvalue* value)
{
    int64_t number = 0;
    return value != NULL && xrtValueGetInt(value, &number) && number == 46;
}

xfuture* xlir_make_xvalue_future(void)
{
    xfuture* future = NULL;
    xpromise* promise;
    if ( xlir_cgen_differential_async_xvalue == NULL )
        xlir_cgen_differential_async_xvalue = xrtValueInt(41);
    if ( xlir_cgen_differential_async_xvalue == NULL ) return NULL;
    promise = xrtPromiseCreate(&future, NULL);
    if ( promise == NULL || !xrtPromiseResolve(
             promise, &xlir_cgen_differential_async_xvalue) ) {
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
