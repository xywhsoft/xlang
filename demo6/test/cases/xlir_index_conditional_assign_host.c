#include <assert.h>
#include <stdio.h>
#include "../../build/test/xlir_index_conditional_assign.c"
#include "../../build/test/xlir_index_conditional_assign.bind.h"

static xpromise* pendingPromise;
static int64_t pendingValue;
extern bool xrtFutureDone(const xfuture*);
extern bool xrtPromiseResolve(xpromise*, void*);
void conditional_fail(void)
{
    xl6_error_result failure = {false, NULL};
    failure.error = xrtErrorCreate(XERR_STATE, "conditional-index", 73, "operand evaluation failed");
    assert(failure.error != NULL); xl6ErrorScopePropagateFailure(failure);
}
xfuture* conditional_future(void)
{
    xfuture* result = NULL;
    assert(pendingPromise == NULL);
    pendingPromise = xrtPromiseCreate(&result, NULL);
    assert(pendingPromise && result); return result;
}
void conditional_prepare(void) { assert(xlir_module_init()); }
void conditional_finish(void) { xlir_module_fini(); assert(!pendingPromise && !xrtGetError()); }
void conditional_sync_run(int mode, int present, int stage)
{
    int sequence = 1, failed = stage == 1;
    int actual;
    if (!failed && mode != 2) { sequence = 12; failed = stage == 2; }
    if (!failed && !present) { sequence = sequence * 10 + 3; failed = stage == 3 || stage == 4; }
    actual = (int)conditional_sync(mode, present != 0, stage);
    if (actual != sequence * 10 + failed)
        fprintf(stderr, "conditional mode=%d present=%d stage=%d: result %d expected %d\n",
            mode, present, stage, actual, sequence * 10 + failed);
    assert(actual == sequence * 10 + failed && !xrtGetError());
    assert(conditional_nonnull() == 2 && !xrtGetError());
}
void conditional_async_run(int mode, int present, int reject)
{
    const xerror* error;
    int suspends = !present || mode == 5;
    conditional_reset(present != 0);
    conditional_start(mode);
    assert(pendingPromise && xrtFutureDone(CONDITIONAL_PENDING) == !suspends);
    conditional_replace();
    if (reject) {
        xerror* failure = xrtErrorCreate(XERR_STATE, "conditional-index", 71, "await failed");
        assert(failure && xrtPromiseReject(pendingPromise, failure)); xrtErrorFree(failure);
    } else {
        pendingValue = mode == 5 ? 0 : 17;
        assert(xrtPromiseResolve(pendingPromise, &pendingValue));
    }
    xrtPromiseDestroy(pendingPromise); pendingPromise = NULL;
    assert(xrtFutureWait(CONDITIONAL_PENDING) == 0 && xrtFutureDone(CONDITIONAL_PENDING));
    error = xrtFutureError(CONDITIONAL_PENDING);
    if (reject && suspends) assert(error && xrtErrorCode(error) == 71);
    else {
        const int64_t* value = (const int64_t*)xrtFutureValue(CONDITIONAL_PENDING);
        assert(!error && value && *value == (present ? 8 : 17));
    }
    assert(conditional_original(mode) == (present ? 8 : (reject ? -1 : 17)));
    xrtFutureDestroy(CONDITIONAL_PENDING); CONDITIONAL_PENDING = NULL;
    assert(!xrtGetError());
}
