/* Class descriptors and the accepted activation own independent actual code
 * acquisitions. Queue quiescence, not a guessed lease count, separates cases.
 * At final retirement require BOTH all actual acquisitions and queue credits
 * to be returned. The old population and memory assertions stay unchanged. */
#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_FUTURE
#define XRT_MODULE_THREAD
#include "lib/xrt/xrt.h"
#include "src/runtime/xl6_code_owner.h"
#include "xlir_async_source_sync.h"
#include <assert.h>
#include <stdatomic.h>
extern bool cancel_watch_execution_idle(void);
static atomic_uint acquisitions,returns,watches;
static atomic_bool fault;
void xlir_async_test_pause(unsigned long long* started)
{
    if(!*started)*started=xrtDeadlineAfter(5000000);
    assert(!xrtDeadlineExpired((xdeadline)*started));xrtThreadYield();
}
static int acquire(void* data) {(void)data;atomic_fetch_add_explicit(&acquisitions,1,memory_order_acq_rel);return 1;}
static void release(void* data)
{
    (void)data;if(xrtMemDebugFailTriggered())atomic_store(&fault,true);
    xrtMemDebugFailClear();
    assert(atomic_fetch_add_explicit(&returns,1,memory_order_acq_rel)<atomic_load_explicit(&acquisitions,memory_order_acquire));
}
const XL6_CodeOwner* xlir_async_test_code_owner(void)
{
    static const XL6_CodeOwner owner={sizeof(XL6_CodeOwner),NULL,acquire,release};return &owner;
}
void xlir_async_test_wait_retired(void)
{
    unsigned long long started=0;
    while(atomic_load_explicit(&returns,memory_order_acquire)!=atomic_load_explicit(&acquisitions,memory_order_acquire)||
        !cancel_watch_execution_idle())xlir_async_test_pause(&started);
}
void xlir_async_test_begin(void)
{
    unsigned long long started=0;
    while(!cancel_watch_execution_idle())xlir_async_test_pause(&started);
    assert(!atomic_load(&fault));atomic_store(&watches,0);
}
bool xlir_async_test_take_fault(void)
{
    xlir_async_test_wait_retired();bool detected=atomic_exchange(&fault,false)||xrtMemDebugFailTriggered();
    xrtMemDebugFailClear();return detected;
}
void xlir_async_test_assert_owner_released(void)
{
    assert(atomic_load_explicit(&returns,memory_order_acquire)==
        atomic_load_explicit(&acquisitions,memory_order_acquire));
}
xfuturewatchresult xlir_async_test_watch_add(xfuture* future,xfuturewatch* watch)
{
    xfuturewatchresult result=xrtFutureWatchAdd(future,watch);
    if(result!=XFUTURE_WATCH_ERROR)atomic_fetch_add_explicit(&watches,1,memory_order_release);
    return result;
}
void xlir_async_test_wait_watches(unsigned count)
{
    unsigned long long started=0;
    while(atomic_load_explicit(&watches,memory_order_acquire)<count)xlir_async_test_pause(&started);
}
