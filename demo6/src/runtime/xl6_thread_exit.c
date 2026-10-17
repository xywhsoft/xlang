#include "xl6_thread_exit.h"
#include <stddef.h>
#include <stdatomic.h>
static _Atomic(XL6_ThreadLocalExitV1) localExit;
bool xl6ThreadExitRegisterLocalV1(XL6_ThreadLocalExitV1 callback)
{
    XL6_ThreadLocalExitV1 expected = NULL;
    return callback && (atomic_compare_exchange_strong_explicit(&localExit, &expected, callback,
        memory_order_release, memory_order_acquire) || expected == callback);
}
bool xl6ThreadExitCurrentV1(void)
{
    XL6_ThreadLocalExitV1 callback = atomic_load_explicit(&localExit, memory_order_acquire);
    return !callback || callback();
}
