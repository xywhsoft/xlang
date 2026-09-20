/* The suite generates this file immediately before compiling this host. */
#include <stdbool.h>
#include <stdint.h>
#include "../../build/test/xlir_async_task_xvalue_param_generated.c"
#define XLIR_TEST_LAUNCH_FUTURE
#include "xlir_launch_test.h"

extern int xrtFutureWait(xfuture*);
extern void* xrtFutureValue(const xfuture*);

int main(void)
{
    xvalue* input = xrtValueInt(41);
    xfuture* future;
    void* result;
    if ( input == NULL ) return 1;
    future = xlir_test_observe_future_launch(xlir_async_start_1(input));
    xrtValueRelease(input);
    if ( future == NULL ) return 1;
    if ( xrtFutureWait(future) != 0 ) { xrtFutureDestroy(future); return 1; }
    result = xrtFutureValue(future);
    xrtFutureDestroy(future);
    return result != NULL ? 0 : 1;
}
