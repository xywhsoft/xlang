/* The suite generates this file immediately before compiling this host. */
#include <stdbool.h>
#include "../../build/test/xlir_async_future_three_strings_generated.c"

typedef struct xpromise xpromise;

extern xpromise* xrtPromiseCreate(xfuture**, void*);
extern bool xrtPromiseResolve(xpromise*, void*);
extern bool xrtPromiseReject(xpromise*, const struct xerror*);
extern bool xrtPromiseCancel(xpromise*);
extern void xrtPromiseDestroy(xpromise*);

static const char* xlir_future_three_strings_value = "hello";
static int xlir_future_three_strings_calls = 0;

xfuture* xlir_make_string_future(void)
{
    xfuture* future = NULL;
    xpromise* promise = xrtPromiseCreate(&future, NULL);
    xerror* error = NULL;
    xlir_future_three_strings_calls += 1;
#ifdef XLIR_STRING_THIRD_REJECT
    if ( xlir_future_three_strings_calls == 3 )
        error = xrtErrorCreate(15, "xlir-test", 1, "expected string chain failure");
#endif
#ifdef XLIR_STRING_THIRD_CANCEL
    if ( xlir_future_three_strings_calls == 3 ) {
        bool cancelled = promise != NULL && xrtPromiseCancel(promise);
        xrtPromiseDestroy(promise);
        return cancelled ? future : NULL;
    }
#endif
#ifdef XLIR_STRING_THIRD_CLOSE
    if ( xlir_future_three_strings_calls == 3 ) {
        xrtPromiseDestroy(promise);
        return future;
    }
#endif
    if ( error != NULL ) {
        bool rejected = promise != NULL && xrtPromiseReject(promise, error);
        xrtErrorFree(error);
        xrtPromiseDestroy(promise);
        return rejected ? future : NULL;
    }
    if ( promise == NULL || !xrtPromiseResolve(promise, &xlir_future_three_strings_value) ) {
        xrtPromiseDestroy(promise);
        return NULL;
    }
    xrtPromiseDestroy(promise);
    return future;
}

int main(void)
{
#if defined(XLIR_STRING_THIRD_REJECT) || defined(XLIR_STRING_THIRD_CANCEL) || defined(XLIR_STRING_THIRD_CLOSE)
    return xlir_module_init() == 0 ? 0 : 1;
#else
    return xlir_module_init() == 1 ? 0 : 1;
#endif
}
