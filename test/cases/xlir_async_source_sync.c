#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_FUTURE
#define XRT_MODULE_THREAD
#ifndef XRT_MODULE_WAIT
#define XRT_MODULE_WAIT
#endif
#include "lib/xrt/xrt.h"
#include "release/include/xl_clock.h"
#include "src/runtime/xl6_code_owner.h"
#include "xlir_async_source_sync.h"
#include <assert.h>
#include <stdatomic.h>
static atomic_uint leases, watches;
static atomic_bool worker_fault;
void xlir_async_test_pause(unsigned long long* started)
{
    if (!*started) *started = xl6DeadlineAfterMillisecondsV1(5000);
    assert(!xl6DeadlineExpiredV1((XL6_DeadlineV1)*started));
    xrtThreadYield();
}
static int acquire(void* context)
{
    (void)context; atomic_fetch_add_explicit(&leases, 1, memory_order_acq_rel); return 1;
}
static void release(void* context)
{
    (void)context;
    /* Fault injection belongs to the thread which armed it. Observe and clear
     * it on that worker after frame disposal, before publishing retirement. */
    if (xrtMemDebugFailTriggered()) atomic_store(&worker_fault, true);
    xrtMemDebugFailClear();
    assert(atomic_fetch_sub_explicit(&leases, 1, memory_order_acq_rel) > 0);
}
const XL6_CodeOwner* xlir_async_test_code_owner(void)
{
    static const XL6_CodeOwner owner = {sizeof(XL6_CodeOwner), NULL, acquire, release};
    return &owner;
}
void xlir_async_test_wait_retired(void)
{
    unsigned long long started = 0;
    /* A module code-owner lease is also held by live classes, closures and
     * returned payloads. It is therefore not a continuation-retirement
     * counter. Queue idleness becomes true only after the worker has run the
     * frame release tail and dropped its execution credit. */
    while (!xlir_async_test_execution_idle()) xlir_async_test_pause(&started);
}
void xlir_async_test_begin(void)
{
    xlir_async_test_wait_retired(); assert(!atomic_load(&worker_fault));
    atomic_store(&watches, 0);
}
bool xlir_async_test_take_fault(void)
{
    bool detected;
    xlir_async_test_wait_retired();
    detected = atomic_exchange(&worker_fault, false) || xrtMemDebugFailTriggered();
    xrtMemDebugFailClear(); return detected;
}
void xlir_async_test_assert_owner_released(void)
{
    assert(atomic_load_explicit(&leases, memory_order_acquire) == 0);
}
xfuturewatchresult xlir_async_test_watch_add(xfuture* future, xfuturewatch* watch)
{
    xfuturewatchresult result = xrtFutureWatchAdd(future, watch);
    if (result != XFUTURE_WATCH_ERROR) atomic_fetch_add_explicit(&watches, 1, memory_order_release);
    return result;
}
void xlir_async_test_wait_watches(unsigned count)
{
    unsigned long long started = 0;
    while (atomic_load_explicit(&watches, memory_order_acquire) < count) xlir_async_test_pause(&started);
}
