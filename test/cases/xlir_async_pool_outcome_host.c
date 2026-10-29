#include "release/include/xl_clock.h"
#include "xlir_async_pool_outcome.c"
#include "xlir_async_pool_outcome.h.bind.h"
#include <assert.h>
#define XLIR_TEST_LAUNCH_FUTURE
#include "xlir_launch_test.h"
extern xfuturestate xrtFutureState(const xfuture*);
extern void* xrtFutureValue(const xfuture*);
extern const xerror* xrtFutureError(const xfuture*);
unsigned pool_outcome_watch_size(void) { return (unsigned)sizeof(xfuturewatch); }
typedef char pool_watch_storage_abi[(sizeof(xfuturewatch) == 64) ? 1 : -1];
int pool_outcome_init(void) { return xlir_module_init(); }
void pool_outcome_fini(void)
{
    XL6_DeadlineV1 deadline=xl6DeadlineAfterMillisecondsV1(5000);
    while(!xlir_continuation_quiesce(false)){
        assert(!xrtGetError()&&!xl6DeadlineExpiredV1(deadline));xrtThreadYield();
    }
    xlir_module_fini();
}
xfuture* pool_outcome_start(unsigned kind, int64_t mode)
{
    switch (kind) {
    case 0: return xlir_test_observe_future_launch(POOL_SCALAR(mode));
    case 1: return xlir_test_observe_future_launch(POOL_TEXT(mode));
    case 2: return xlir_test_observe_future_launch(POOL_OWNER(mode));
    default: return xlir_test_observe_future_launch(POOL_READY(mode));
    }
}
