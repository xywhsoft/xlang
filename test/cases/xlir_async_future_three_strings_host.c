/* The suite generates this file immediately before compiling this host. */
#include <stdbool.h>
#ifndef XLIR_MULTI_AWAIT_GENERATED_SOURCE
#define XLIR_MULTI_AWAIT_GENERATED_SOURCE "../../build/test/xlir_async_future_three_strings_generated.c"
#endif
#include "xlir_multi_await_host_prelude.h"
#include XLIR_MULTI_AWAIT_GENERATED_SOURCE
#include "xlir_multi_await_host_support.h"

/* A language string Future carries Data/Size, not the former char* ABI.
 * Embedded NUL exercises all three observations and both owned concats. */
static xlir_string xlir_future_three_strings_value = {"he\0llo", 6};
static int xlir_future_three_strings_calls = 0;

xfuture* xlir_make_string_future(void)
{
    xlir_future_three_strings_calls += 1;
#ifdef XLIR_STRING_THIRD_REJECT
    if (xlir_future_three_strings_calls==3) return xlir_multi_await_future(&xlir_future_three_strings_value,XLIR_TEST_REJECT);
#endif
#ifdef XLIR_STRING_THIRD_CANCEL
    if (xlir_future_three_strings_calls==3) return xlir_multi_await_future(&xlir_future_three_strings_value,XLIR_TEST_CANCEL);
#endif
#ifdef XLIR_STRING_THIRD_CLOSE
    if (xlir_future_three_strings_calls==3) return xlir_multi_await_future(&xlir_future_three_strings_value,XLIR_TEST_CLOSE);
#endif
    return xlir_multi_await_future(&xlir_future_three_strings_value,XLIR_TEST_RESOLVE);
}

int main(void)
{
#if defined(XLIR_STRING_THIRD_REJECT)
    return xlir_multi_await_check(false, &xlir_future_three_strings_calls, 3, XERR_NONE);
#elif defined(XLIR_STRING_THIRD_CANCEL)
    return xlir_multi_await_check(false, &xlir_future_three_strings_calls, 3, XERR_CANCELLED);
#elif defined(XLIR_STRING_THIRD_CLOSE)
    return xlir_multi_await_check(false, &xlir_future_three_strings_calls, 3, XERR_CLOSED);
#else
    return xlir_multi_await_check(true, &xlir_future_three_strings_calls, 3, XERR_NONE);
#endif
}
