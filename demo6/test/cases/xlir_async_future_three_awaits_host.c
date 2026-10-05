/* The suite generates this file immediately before compiling this host. */
#include <stdbool.h>
#include <stdint.h>
#ifndef XLIR_MULTI_AWAIT_GENERATED_SOURCE
#define XLIR_MULTI_AWAIT_GENERATED_SOURCE "../../build/test/xlir_async_future_three_awaits_generated.c"
#endif
#include "xlir_multi_await_host_prelude.h"
#include XLIR_MULTI_AWAIT_GENERATED_SOURCE
#include "xlir_multi_await_host_support.h"

static int64_t xlir_three_await_value = 41;
static int xlir_three_await_calls = 0;

xfuture* xlir_make_i64_future(void)
{
    xlir_three_await_calls += 1;
#ifdef XLIR_CHAIN_THIRD_REJECT
    if (xlir_three_await_calls==3) return xlir_multi_await_future(&xlir_three_await_value,XLIR_TEST_REJECT);
#endif
#ifdef XLIR_CHAIN_THIRD_CANCEL
    if (xlir_three_await_calls==3) return xlir_multi_await_future(&xlir_three_await_value,XLIR_TEST_CANCEL);
#endif
#ifdef XLIR_CHAIN_THIRD_CLOSE
    if (xlir_three_await_calls==3) return xlir_multi_await_future(&xlir_three_await_value,XLIR_TEST_CLOSE);
#endif
    return xlir_multi_await_future(&xlir_three_await_value,XLIR_TEST_RESOLVE);
}

int main(void)
{
#if defined(XLIR_CHAIN_THIRD_REJECT)
    return xlir_multi_await_check(false, &xlir_three_await_calls, 3, XERR_NONE);
#elif defined(XLIR_CHAIN_THIRD_CANCEL)
    return xlir_multi_await_check(false, &xlir_three_await_calls, 3, XERR_CANCELLED);
#elif defined(XLIR_CHAIN_THIRD_CLOSE)
    return xlir_multi_await_check(false, &xlir_three_await_calls, 3, XERR_CLOSED);
#else
    return xlir_multi_await_check(true, &xlir_three_await_calls, 3, XERR_NONE);
#endif
}
