/* Legacy emits a process entry and needs the public runtime declarations.
 * XLIR's differential host preincludes the generated module, which already
 * owns its minimal runtime declarations; including xruntime again would
 * deliberately violate that standalone generated-C ABI. */
#include <stdbool.h>
#include <stdint.h>
#ifndef XLIR_GENERATED_MODULE_PREINCLUDED
#include "__xl_runtime/xl_runtime.h"
#else
extern bool xrtPromiseResolve(xpromise*, void*);
#endif

static int64_t xlir_async_generic_differential_scalar = 41;
static const char* xlir_async_generic_differential_string = "hello";

xfuture* xlir_make_i64_future(void)
{
    xfuture* future = NULL;
    xpromise* promise = xrtPromiseCreate(&future, NULL);
    if ( promise == NULL || !xrtPromiseResolve(
             promise, &xlir_async_generic_differential_scalar) ) {
        xrtPromiseDestroy(promise);
        return NULL;
    }
    xrtPromiseDestroy(promise);
    return future;
}

xfuture* xlir_make_string_future(void)
{
    xfuture* future = NULL;
    xpromise* promise = xrtPromiseCreate(&future, NULL);
    if ( promise == NULL || !xrtPromiseResolve(
             promise, &xlir_async_generic_differential_string) ) {
        xrtPromiseDestroy(promise);
        return NULL;
    }
    xrtPromiseDestroy(promise);
    return future;
}
