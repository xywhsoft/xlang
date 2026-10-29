/* The suite generates this file immediately before compiling this host. */
#include <stdbool.h>
#include <stdint.h>
#ifndef XLIR_MULTI_AWAIT_GENERATED_SOURCE
#define XLIR_MULTI_AWAIT_GENERATED_SOURCE "../../build/test/xlir_async_future_three_awaits_mixed_generated.c"
#endif
#include "xlir_multi_await_host_prelude.h"
#include XLIR_MULTI_AWAIT_GENERATED_SOURCE
#include "xlir_multi_await_host_support.h"

static int64_t xlir_three_await_mixed_value = 41;
static int xlir_three_await_mixed_calls;

xfuture* xlir_make_i64_future(void)
{
    ++xlir_three_await_mixed_calls;
    return xlir_multi_await_future(&xlir_three_await_mixed_value,XLIR_TEST_RESOLVE);
}

int main(void)
{
    return xlir_multi_await_check(true, &xlir_three_await_mixed_calls, 3, XERR_NONE);
}
