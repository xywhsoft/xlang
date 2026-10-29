#ifndef XLIR_STRING_FUTURE_HOST_SUPPORT_H
#define XLIR_STRING_FUTURE_HOST_SUPPORT_H
#include "xlir_multi_await_host_prelude.h"
#include XLIR_STRING_FUTURE_GENERATED_SOURCE
#include "xlir_multi_await_host_support.h"

/* Raw future<string> payload is the complete language Data/Size record, not
 * the address of a C string pointer. Static storage outlives both activations.
 * Every source/concat expectation includes the byte AFTER embedded NUL. */
static xlir_string xlir_string_future_value={"he\0llo",6};
static int xlir_string_future_calls;

xfuture* xlir_make_string_future(void)
{
    XLIR_TestCompletion mode=XLIR_TEST_RESOLVE;
    ++xlir_string_future_calls;
    assert(xlir_string_future_calls<=XLIR_STRING_FUTURE_COUNT);
    if (xlir_string_future_calls==XLIR_STRING_FUTURE_COUNT) {
#if defined(XLIR_STRING_FUTURE_REJECT)
        mode=XLIR_TEST_REJECT;
#elif defined(XLIR_STRING_FUTURE_CANCEL)
        mode=XLIR_TEST_CANCEL;
#elif defined(XLIR_STRING_FUTURE_CLOSE)
        mode=XLIR_TEST_CLOSE;
#endif
    }
    return xlir_multi_await_future(&xlir_string_future_value,mode);
}
int main(void)
{
#if defined(XLIR_STRING_FUTURE_REJECT)
    return xlir_multi_await_check(false,&xlir_string_future_calls,XLIR_STRING_FUTURE_COUNT,XERR_NONE);
#elif defined(XLIR_STRING_FUTURE_CANCEL)
    return xlir_multi_await_check(false,&xlir_string_future_calls,XLIR_STRING_FUTURE_COUNT,XERR_CANCELLED);
#elif defined(XLIR_STRING_FUTURE_CLOSE)
    return xlir_multi_await_check(false,&xlir_string_future_calls,XLIR_STRING_FUTURE_COUNT,XERR_CLOSED);
#else
    return xlir_multi_await_check(true,&xlir_string_future_calls,XLIR_STRING_FUTURE_COUNT,XERR_NONE);
#endif
}
#endif
