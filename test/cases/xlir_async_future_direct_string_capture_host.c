/* The suite generates this file immediately before compiling this host. */
#include <stdbool.h>
#include <windows.h>
#include "../../build/test/xlir_async_future_direct_string_capture_generated.c"

/* future<string> carries the complete language view, not a char* payload. */
static xstrview xlir_async_direct_string_value = { "hello", 5 };

static DWORD WINAPI xlir_resolve_string_future(void* data)
{
    xpromise* promise = (xpromise*)data;
    bool resolved;
    /* Force decorateProduced to return to its caller and release the original
     * argument owner before the continuation observes its copied local. */
    Sleep(10);
    resolved = promise != NULL &&
        xrtPromiseResolve(promise, &xlir_async_direct_string_value);
    xrtPromiseDestroy(promise);
    return resolved ? 0 : 1;
}

xfuture* xlir_make_string_future(void)
{
    xfuture* future = NULL;
    xpromise* promise = xrtPromiseCreate(&future, NULL);
    HANDLE resolver;
    if ( promise == NULL ) return NULL;
    resolver = CreateThread(
        NULL, 0, xlir_resolve_string_future, promise, 0, NULL);
    if ( resolver == NULL ) {
        xrtPromiseDestroy(promise);
        return NULL;
    }
    CloseHandle(resolver);
    return future;
}

int main(void)
{
    return xlir_module_init() == 1 ? 0 : 1;
}
