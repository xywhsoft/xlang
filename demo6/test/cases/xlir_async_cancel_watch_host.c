#include <assert.h>
#include <stdio.h>
#ifndef XLIR_CANCEL_WATCH_GENERATED_C
#define XLIR_CANCEL_WATCH_GENERATED_C "../../build/test/xlir_async_cancel_watch.c"
#define XLIR_CANCEL_WATCH_GENERATED_BIND "../../build/test/xlir_async_cancel_watch.h.bind.h"
#endif
#include XLIR_CANCEL_WATCH_GENERATED_C
#include XLIR_CANCEL_WATCH_GENERATED_BIND
#define XLIR_TEST_LAUNCH_FUTURE
#include "xlir_launch_test.h"
#define XLIR_ASYNC_SOURCE_HOST
#include "xlir_async_source_sync.h"
extern int cancel_watch_dropped(int), cancel_watch_finalized(void), cancel_watch_ambient(void);
extern bool xrtPromiseResolve(xpromise*, void*);
void cancel_watch_drop(int64_t id)
{
    int mode = cancel_watch_dropped((int)id);
    if (mode) {
        xl6_error_result failure = {false, mode == 1 ? xrtErrorCreate(XERR_STATE, "cancel watch", 90 + (int)id, "frame drop failed") : NULL};
        if (mode == 1) assert(failure.error);
        xl6ErrorScopePropagateFailure(failure);
    }
}
void cancel_watch_finally(void)
{
    int mode = cancel_watch_finalized();
    if (mode) {
        xl6_error_result failure = {false, mode == 1 ? xrtErrorCreate(XERR_STATE, "cancel watch", 77, "finally failed") : NULL};
        if (mode == 1) assert(failure.error);
        xl6ErrorScopePropagateFailure(failure);
    }
}
void cancel_watch_prepare(void) { xlir_async_test_bind(); assert(xlir_module_init() && !xrtGetError()); }
bool cancel_watch_execution_idle(void) { return xl6ContinuationExecutorIsIdle(xlir_continuation_executor); }
void cancel_watch_finish(void) { xlir_async_test_fini(); }
xfuture* cancel_watch_launch(xvalue* first, xvalue* second, xfuture* input, xfuture* cleanup, int mode)
{
    xl6_error_scope scope = {0}; xl6_error_result outcome; xfuture* result;
    xlir_async_test_begin();
    assert(xl6ErrorScopeBegin(&scope)); result = xlir_test_observe_future_launch(CANCEL_WATCH_RUN(first, second, input, cleanup, mode));
    outcome = xl6ErrorScopeEnd(&scope); assert(outcome.ok && !outcome.error && !xrtGetError()); return result;
}
void cancel_watch_produce(xpromise* promise, unsigned mode, int64_t* value, xerror* error)
{
    xl6_error_scope scope = {0}; xl6_error_result outcome; bool completed;
    int ambient = cancel_watch_ambient();
    assert(xl6ErrorScopeBegin(&scope));
    if (ambient) {
        xl6_error_result prior = {false, ambient == 1 ? xrtErrorCreate(XERR_STATE, "ambient", 31, "unrelated producer failure") : NULL};
        if (ambient == 1) assert(prior.error);
        xl6ErrorScopePropagateFailure(prior);
    }
    completed = mode == 0 ? xrtPromiseResolve(promise, value) : mode == 1 ? xrtPromiseReject(promise, error) : mode == 2 ? xrtPromiseCancel(promise) : xrtPromiseClose(promise);
    assert(completed); outcome = xl6ErrorScopeEnd(&scope);
    if (ambient) {
        assert(!outcome.ok && !xrtGetError());
        if (ambient == 1) assert(outcome.error && xrtErrorCode(outcome.error) == 31);
        else assert(!outcome.error);
        xrtErrorFree(outcome.error);
    } else {
        if (!outcome.ok) fprintf(stderr, "cancel watch producer failed: code=%d\n", outcome.error ? xrtErrorCode(outcome.error) : -1);
        assert(outcome.ok && !outcome.error && !xrtGetError());
    }
}
