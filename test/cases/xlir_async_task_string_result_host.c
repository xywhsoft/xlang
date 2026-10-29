/* The suite generates this file immediately before compiling this host. */
#include <stdbool.h>
#include <string.h>
#include "../../build/test/xlir_async_task_string_result_generated.c"
#define XLIR_TEST_LAUNCH_FUTURE
#include "xlir_launch_test.h"

extern int xrtFutureWait(xfuture*);
extern void* xrtFutureValue(const xfuture*);

int main(void)
{
    xfuture* future = xlir_test_observe_future_launch(xlir_async_start_1());
    char** result;
    int ok;
    if ( future == NULL ) return 1;
    if ( xrtFutureWait(future) != 0 ) { xrtFutureDestroy(future); return 1; }
    result = (char**)xrtFutureValue(future);
    ok = result != NULL && *result != NULL && strcmp(*result, "XLIR") == 0;
    xrtFutureDestroy(future);
    return ok ? 0 : 1;
}
