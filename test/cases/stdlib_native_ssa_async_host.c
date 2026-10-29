#include <stdbool.h>
#include <stdint.h>
#include <stdatomic.h>
static _Atomic unsigned cleanupCount;
static void afterInit(int);
#define XLIR_MULTI_AWAIT_AFTER_INIT(ok) afterInit(ok)
#include "xlir_multi_await_host_prelude.h"
#ifndef XLIR_NATIVE_SSA_SOURCE
#error "Pass verified generated native SSA C"
#endif
#include XLIR_NATIVE_SSA_SOURCE
#include "xlir_multi_await_host_support.h"
static int calls;
static int64_t futureValue=41;
void xlir_spill_cleanup_seen(void)
{atomic_fetch_add_explicit(&cleanupCount,1,memory_order_release);}
static void afterInit(int ok)
{(void)ok;assert(atomic_exchange_explicit(&cleanupCount,0,memory_order_acq_rel)==1);}
xfuture* xlir_make_i64_future(void)
{
    ++calls;
#if defined(XLIR_SSA_REJECT)
    return xlir_multi_await_future(&futureValue,XLIR_TEST_REJECT);
#elif defined(XLIR_SSA_CANCEL)
    return xlir_multi_await_future(&futureValue,XLIR_TEST_CANCEL);
#elif defined(XLIR_SSA_CLOSE)
    return xlir_multi_await_future(&futureValue,XLIR_TEST_CLOSE);
#else
    return xlir_multi_await_future(&futureValue,XLIR_TEST_RESOLVE);
#endif
}
int main(void)
{
#if defined(XLIR_SSA_REJECT)
    int result=xlir_multi_await_check(false,&calls,1,XERR_NONE);
#elif defined(XLIR_SSA_CANCEL)
    int result=xlir_multi_await_check(false,&calls,1,XERR_CANCELLED);
#elif defined(XLIR_SSA_CLOSE)
    int result=xlir_multi_await_check(false,&calls,1,XERR_CLOSED);
#else
    int result=xlir_multi_await_check(true,&calls,1,XERR_NONE);
#endif
    assert(!result);puts("native SSA: finally and ownership balanced");return result;
}
