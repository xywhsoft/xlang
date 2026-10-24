#include <assert.h>
#include "../../build/test/xlir_async_frame_completion.c"
#include "../../build/test/xlir_async_frame_completion.h.bind.h"
#define XLIR_TEST_LAUNCH_FUTURE
#include "xlir_launch_test.h"
#define XLIR_ASYNC_SOURCE_HOST
#include "xlir_async_source_sync.h"
extern int frame_completion_dropped(int), frame_completion_failure(void);
extern bool xrtPromiseResolve(xpromise*, void*);
void frame_completion_drop(int64_t id)
{
    int mode = frame_completion_dropped((int)id);
    if (mode) {
        xl6_error_result failure = {false, mode == 1 ? xrtErrorCreate(XERR_STATE, "frame completion", 90 + (int)id, "frame drop failed") : NULL};
        /* Fault injection may also prevent this test diagnostic allocation.
         * Preserve the allocator's actual checked error, rather than assert
         * that allocation succeeded while deliberately exhausting it. */
        if (mode == 1 && !failure.error) {
            failure.error = xrtTakeError();
            assert(failure.error && xrtErrorKind(failure.error) == XERR_MEMORY);
        }
        xl6ErrorScopePropagateFailure(failure);
    }
}
void frame_completion_fail(void)
{
    int mode = frame_completion_failure();
    if (mode) {
        xl6_error_result failure = {false, mode == 2 ? NULL : xrtErrorCreate(
            mode == 3 ? XERR_CANCELLED : mode == 4 ? XERR_CLOSED : XERR_STATE, "frame completion", 73, "body failed")};
        if (mode != 2) assert(failure.error);
        xl6ErrorScopePropagateFailure(failure);
    }
}
void frame_completion_prepare(void) { xlir_async_test_bind(); assert(xlir_module_init() && !xrtGetError()); }
void frame_completion_finish(void) { xlir_async_test_fini(); }
xfuture* frame_completion_launch(xvalue* first, xvalue* second, xfuture* input, bool early)
{
    xl6_error_scope scope = {0}; xl6_error_result outcome; xfuture* result;
    xlir_async_test_begin();
    assert(xl6ErrorScopeBegin(&scope));
    result = xlir_test_observe_future_launch(FRAME_COMPLETION_RUN(first, second, input, early));
    outcome = xl6ErrorScopeEnd(&scope);
    if (result) assert(outcome.ok && !outcome.error && !xrtGetError());
    else {
        assert(!outcome.ok && outcome.error && xrtErrorKind(outcome.error) == XERR_MEMORY);
        xrtSetErrorTake(outcome.error);
    }
    return result;
}
void frame_completion_produce(xpromise* promise, int64_t* value)
{
    xl6_error_scope scope = {0}; xl6_error_result outcome;
    assert(xl6ErrorScopeBegin(&scope) && xrtPromiseResolve(promise, value));
    outcome = xl6ErrorScopeEnd(&scope);
    assert(outcome.ok && !outcome.error && !xrtGetError());
}
