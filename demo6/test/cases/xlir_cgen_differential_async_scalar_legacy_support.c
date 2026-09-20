/* Legacy Cgen emits the process entry; this file supplies only its extern. */
#include <stdbool.h>
#include <stdint.h>
#include "__xl_runtime/xl_runtime.h"

static int64_t xlir_cgen_differential_async_scalar_value = 41;

xfuture* xlir_make_i64_future(void)
{
    xfuture* future = NULL;
    xpromise* promise = xrtPromiseCreate(&future, NULL);
    if ( promise == NULL || !xrtPromiseResolve(
             promise, &xlir_cgen_differential_async_scalar_value) ) {
        xrtPromiseDestroy(promise);
        return NULL;
    }
    xrtPromiseDestroy(promise);
    return future;
}
