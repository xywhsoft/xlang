/* Legacy Cgen emits the process entry; this file supplies only its extern. */
#include <stdbool.h>
#include <stdint.h>
#include "__xl_runtime/xl_runtime.h"

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
