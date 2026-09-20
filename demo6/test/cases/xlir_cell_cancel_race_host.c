#include <assert.h>
#define xrtFutureWatchAdd cell_cancel_watch_add
#include "../../build/test/xlir_cell_cancel_race.c"
#undef xrtFutureWatchAdd
#include "../../build/test/xlir_cell_cancel_race.h.bind.h"
#define XLIR_TEST_LAUNCH_FUTURE
#include "xlir_launch_test.h"
extern xfuturewatchresult xrtFutureWatchAdd(xfuture*, xfuturewatch*);
extern void cell_cancel_watch_registered(xfuture*, int);
xfuturewatchresult cell_cancel_watch_add(xfuture* source, xfuturewatch* watch)
{
    // Use the real transport; only delay the registration caller after the
    // actual PENDING result. Never fabricate readiness or touch opaque data.
    xfuturewatchresult status = xrtFutureWatchAdd(source, watch);
    cell_cancel_watch_registered(source, (int)status); return status;
}
extern int cell_cancel_dropped(void);
extern bool xrtPromiseResolve(xpromise*, void*);
extern const xerror* cell_cancel_producer_error(void);
void cell_cancel_drop(void)
{
    int mode = cell_cancel_dropped();
    if (mode) {
        xl6_error_result failure = {false, mode == 1 ? xrtErrorCreate(XERR_STATE, "cell cancel", 91, "cell destructor failed") : NULL};
        if (mode == 1) assert(failure.error);
        xl6ErrorScopePropagateFailure(failure);
    }
}
void cell_cancel_prepare(void) { assert(xlir_module_init() && !xrtGetError()); }
void cell_cancel_finish(void) { xlir_module_fini(); assert(!xrtGetError()); }
xfuture* cell_cancel_launch(xfuture* input, xfuture* cleanup, bool handle)
{
    xl6_error_scope scope = {0}; xl6_error_result outcome; xfuture* result;
    assert(xl6ErrorScopeBegin(&scope)); result = xlir_test_observe_future_launch(CELL_CANCEL_RUN(input, cleanup, handle));
    outcome = xl6ErrorScopeEnd(&scope); assert(result && outcome.ok && !outcome.error && !xrtGetError()); return result;
}
void cell_cancel_produce(xpromise* promise, int64_t* value, int ambient)
{
    xl6_error_scope scope = {0}; xl6_error_result outcome;
    const xerror* expected = ambient == 1 ? cell_cancel_producer_error() : NULL;
    assert(xl6ErrorScopeBegin(&scope));
    if (ambient) {
        xl6_error_result prior = {false, expected ? xrtErrorRef(expected) : NULL};
        if (ambient == 1) assert(prior.error);
        xl6ErrorScopePropagateFailure(prior);
    }
    assert(xrtPromiseResolve(promise, value)); outcome = xl6ErrorScopeEnd(&scope);
    assert(!xrtGetError());
    if (ambient) {
        assert(!outcome.ok);
        if (ambient == 1) assert(outcome.error == expected && xrtErrorCode(outcome.error) == 31);
        else assert(!outcome.error);
        xrtErrorFree(outcome.error);
    } else assert(outcome.ok && !outcome.error);
}
