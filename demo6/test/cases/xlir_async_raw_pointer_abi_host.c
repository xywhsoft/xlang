/* The admission gate generates this translation unit before compiling us. */
#include <stdbool.h>
#include "../../build/test/xlir_async_raw_pointer_abi_generated.c"

typedef struct xpromise xpromise;

extern xpromise* xrtPromiseCreate(xfuture**, void*);
extern bool xrtPromiseResolve(xpromise*, void*);
extern void xrtPromiseDestroy(xpromise*);

static int xlir_raw_pointer_marker;
static void* xlir_raw_pointer_slot = &xlir_raw_pointer_marker;

xfuture* xlir_make_raw_pointer_future(void)
{
    xfuture* future = NULL;
    xpromise* promise = xrtPromiseCreate(&future, NULL);
    if ( promise == NULL ||
         !xrtPromiseResolve(promise, &xlir_raw_pointer_slot) ) {
        if ( promise != NULL ) xrtPromiseDestroy(promise);
        return NULL;
    }
    xrtPromiseDestroy(promise);
    return future;
}

bool xlir_raw_pointer_matches(void* value)
{
    return value == &xlir_raw_pointer_marker;
}

int main(void)
{
    return xlir_module_init() == 1 ? 0 : 1;
}
