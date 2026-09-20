/* Legacy Cgen emits the process entry; this file supplies its extern only. */
#include <stdbool.h>
#include <stdint.h>
#include "__xl_runtime/xl_runtime.h"

static int64_t xlir_async_direct_call_value = 41;
static const char* xlir_async_direct_string_value = "hello";

xfuture* xlir_make_i64_future(void)
{
    xfuture* future = NULL;
    xpromise* promise = xrtPromiseCreate(&future, NULL);
    if ( promise == NULL || !xrtPromiseResolve(promise, &xlir_async_direct_call_value) ) {
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
    if ( promise == NULL || !xrtPromiseResolve(promise, &xlir_async_direct_string_value) ) {
        xrtPromiseDestroy(promise);
        return NULL;
    }
    xrtPromiseDestroy(promise);
    return future;
}
