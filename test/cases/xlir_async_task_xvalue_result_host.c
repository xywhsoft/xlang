/* The suite generates this file immediately before compiling this host. */
#include <stdbool.h>
#include "../../build/test/xlir_async_task_xvalue_result_generated.c"
#define XLIR_TEST_LAUNCH_FUTURE
#include "xlir_launch_test.h"

extern int xrtFutureWait(xfuture*);
extern void* xrtFutureValue(const xfuture*);

int main(void)
{
    xfuture* future = xlir_test_observe_future_launch(xlir_async_start_1());
    void* result;
    if ( future == NULL ) return 1;
    if ( xrtFutureWait(future) != 0 ) { xrtFutureDestroy(future); return 1; }
    result = xrtFutureValue(future);
    xrtFutureDestroy(future);
    return result != NULL ? 0 : 1;
}
