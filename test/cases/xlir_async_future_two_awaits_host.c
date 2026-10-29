/* The suite generates this file immediately before compiling this host. */
#include <stdbool.h>
#include <stdint.h>
#ifndef XLIR_MULTI_AWAIT_GENERATED_SOURCE
#define XLIR_MULTI_AWAIT_GENERATED_SOURCE "../../build/test/xlir_async_future_two_awaits_generated.c"
#endif
#include "xlir_multi_await_host_prelude.h"
#include XLIR_MULTI_AWAIT_GENERATED_SOURCE
#include "xlir_multi_await_host_support.h"

static int64_t xlir_two_await_value = 41;
static int xlir_two_await_calls = 0;

xfuture* xlir_make_i64_future(void)
{
    xlir_two_await_calls += 1;
#ifdef XLIR_CHAIN_SECOND_REJECT
    if (xlir_two_await_calls==2) return xlir_multi_await_future(&xlir_two_await_value,XLIR_TEST_REJECT);
#endif
    return xlir_multi_await_future(&xlir_two_await_value,XLIR_TEST_RESOLVE);
}

int main(void)
{
#ifdef XLIR_CHAIN_SECOND_REJECT
    return xlir_multi_await_check(false, &xlir_two_await_calls, 2, XERR_NONE);
#else
    return xlir_multi_await_check(true, &xlir_two_await_calls, 2, XERR_NONE);
#endif
}
