#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_FUTURE
#define XRT_MODULE_THREAD
#ifndef XRT_MODULE_WAIT
#define XRT_MODULE_WAIT
#endif
#include "lib/xrt/xrt.h"
#include "release/include/xl_clock.h"
#include <assert.h>
#include <stdatomic.h>
static xmemdebugsnapshot before;
static atomic_int watch_count, fail_watch, with_error, blocking, entered;
static atomic_int cleanups;
static xfuture* unblock;
static xpromise* unblock_promise;
static int payload;
void dispatch_memory_begin(void)
{
    assert(xrtMemDebugEnable(true)); xrtClearError(); xrtMemDebugSnapshot(&before);
    atomic_store(&watch_count, 0); atomic_store(&fail_watch, 0);
    atomic_store(&with_error, 0); atomic_store(&blocking, 0); atomic_store(&entered, 0);
    atomic_store(&cleanups, 0);
}
/* Observe the generated failure-unwind boundary, before any language finally.
 * XRT requires INTERNAL when publishing a diagnostic-free failure, but no
 * diagnostic may be fabricated while entering cleanup. */
xerror* dispatch_take_error(void)
{
    xerror* error = xrtTakeError();
    if (atomic_load(&fail_watch) && !atomic_load(&with_error) && !atomic_load(&cleanups))
        assert(error == NULL);
    return error;
}
void dispatch_cleanup(void) { atomic_fetch_add(&cleanups, 1); }
int dispatch_cleanup_count(void) { return atomic_load(&cleanups); }
void dispatch_memory_end(void)
{
    xmemdebugsnapshot after;
    if (unblock) { xrtFutureDestroy(unblock); xrtPromiseDestroy(unblock_promise); unblock = NULL; unblock_promise = NULL; }
    xrtClearError(); xrtMemDebugSnapshot(&after);
    assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
    assert(after.AllocCount - before.AllocCount == after.FreeCount - before.FreeCount);
}
void dispatch_arm_watch(int count, int error)
{ atomic_store(&fail_watch, count); atomic_store(&with_error, error); }
xfuturewatchresult dispatch_watch_add(xfuture* future, xfuturewatch* watch)
{
    int count = atomic_fetch_add_explicit(&watch_count, 1, memory_order_acq_rel) + 1;
    if (count == atomic_load(&fail_watch)) {
        if (atomic_load(&with_error)) xrtSetErrorTake(xrtErrorCreate(XERR_STATE, "dispatch", 62, "registration failed"));
        return XFUTURE_WATCH_ERROR;
    }
    return xrtFutureWatchAdd(future, watch);
}
void dispatch_wait_watches(int count)
{
    XL6_DeadlineV1 deadline = xl6DeadlineAfterMillisecondsV1(5000);
    while (atomic_load_explicit(&watch_count, memory_order_acquire) < count) {
        assert(!xl6DeadlineExpiredV1(deadline)); xrtThreadYield();
    }
}
void dispatch_enable_blocking(void)
{
    unblock_promise = xrtPromiseCreate(&unblock, NULL); assert(unblock_promise);
    atomic_store(&blocking, 1);
}
void dispatch_wait_blocked(void)
{
    XL6_DeadlineV1 deadline = xl6DeadlineAfterMillisecondsV1(5000);
    while (!atomic_load_explicit(&entered, memory_order_acquire)) {
        assert(!xl6DeadlineExpiredV1(deadline)); xrtThreadYield();
    }
}
int64_t dispatch_probe(int64_t value)
{
    if (atomic_load(&blocking)) {
        if (value == 1) {
            atomic_store_explicit(&entered, 1, memory_order_release);
            assert(xrtFutureWaitFor(unblock, 5000000) == XWAIT_OK);
        } else { assert(value == 2); assert(xrtPromiseResolve(unblock_promise, &payload)); }
    }
    return value;
}
