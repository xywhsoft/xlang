/* The host controls actual Promise completion relative to resident Watch and
 * Commit. The generated Queue/Array/closure code is not replaced or modified. */
#include <stdbool.h>
#include <stdint.h>
#include <stdatomic.h>
static _Atomic unsigned queueCleanupSeen;
static void queueAfterInit(int);
#define XLIR_MULTI_AWAIT_AFTER_INIT(ok) queueAfterInit(ok)
#ifndef XLIR_MULTI_AWAIT_GENERATED_SOURCE
#error "Pass the actual generated Queue continuation C"
#endif
#include "xlir_multi_await_host_prelude.h"
#include XLIR_MULTI_AWAIT_GENERATED_SOURCE
#include "xlir_multi_await_host_support.h"
static int64_t queueFutureValue=41;
static int queueFutureCalls;
void xlir_queue_cleanup_seen(void)
{ atomic_fetch_add_explicit(&queueCleanupSeen,1,memory_order_release); }
static void queueAfterInit(int ok)
{
    (void)ok;
    unsigned count=atomic_exchange_explicit(&queueCleanupSeen,0,memory_order_acq_rel);
    assert(count==1); /* Includes the failure paths, before module fini. */
}
xfuture* xlir_make_i64_future(void)
{
    ++queueFutureCalls;
    if (queueFutureCalls==3) {
#if defined(XLIR_CHAIN_THIRD_REJECT)
        return xlir_multi_await_future(&queueFutureValue,XLIR_TEST_REJECT);
#elif defined(XLIR_CHAIN_THIRD_CANCEL)
        return xlir_multi_await_future(&queueFutureValue,XLIR_TEST_CANCEL);
#elif defined(XLIR_CHAIN_THIRD_CLOSE)
        return xlir_multi_await_future(&queueFutureValue,XLIR_TEST_CLOSE);
#endif
    }
    return xlir_multi_await_future(&queueFutureValue,XLIR_TEST_RESOLVE);
}
int main(void)
{
#if defined(XLIR_CHAIN_THIRD_REJECT)
    int status=xlir_multi_await_check(false,&queueFutureCalls,3,XERR_NONE);
#elif defined(XLIR_CHAIN_THIRD_CANCEL)
    int status=xlir_multi_await_check(false,&queueFutureCalls,3,XERR_CANCELLED);
#elif defined(XLIR_CHAIN_THIRD_CLOSE)
    int status=xlir_multi_await_check(false,&queueFutureCalls,3,XERR_CLOSED);
#else
    int status=xlir_multi_await_check(true,&queueFutureCalls,3,XERR_NONE);
#endif
    assert(status==0);
    puts("Queue owned string/bytes/Array/closure across three real awaits and terminal cleanup balanced");
    return status;
}
