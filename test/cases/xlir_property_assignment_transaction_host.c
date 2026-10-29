#include "release/include/xl_clock.h"
#include <assert.h>
#include <stdio.h>
#include "../../build/test/xlir_property_assignment_transaction.c"
#include "../../build/test/xlir_property_assignment_transaction.bind.h"
#include "test/support/xlir_property_await_barrier.h"
static xpromise* pendingPromise;
static xfuture* pendingInput;
static int64_t payload = 4;
extern bool xrtFutureDone(const xfuture*);
extern bool xrtPromiseResolve(xpromise*, void*);
void property_fail(int64_t code)
{
    xl6_error_result failure = {false, NULL};
    failure.error = xrtErrorCreate(XERR_STATE, "property-transaction", (int)code, "operation failed");
    assert(failure.error); xl6ErrorScopePropagateFailure(failure);
}
xfuture* property_future(void)
{
    xfuture* result = NULL;
    assert(!pendingPromise && !pendingInput); pendingPromise = xrtPromiseCreate(&result, NULL);
    assert(pendingPromise && result);
    pendingInput = xrtFutureRef(result); assert(pendingInput); return result;
}
static void checked(int64_t result, int expected, const char* name, int mode, int stage)
{
    if (xrtGetError()) fprintf(stderr, "%s mode %d stage %d at %d: %s\n", name, mode, stage,
        xrtErrorLine(xrtGetError()), xrtErrorMessage(xrtGetError()));
    if (result != expected) fprintf(stderr, "%s mode %d stage %d: %lld != %d\n", name, mode, stage, (long long)result, expected);
    assert(result == expected && !xrtGetError());
}
static int order(int kind, int mode, int stage)
{
    const int compound[] = {1, 2, 3, 4, 5, 6, 7};
    const int assign[] = {4, 1, 2, 6, 7};
    const int get[] = {1, 2, 3};
    const int* steps = kind == 0 ? compound : kind == 1 ? assign : get;
    size_t i, count = kind == 0 ? 7 : kind == 1 ? 5 : 3;
    int value = 0;
    for (i = 0; i < count; ++i) {
        if (kind == 0 && mode != 3 && steps[i] == 5) continue;
        value = value * 10 + steps[i];
        if (stage == steps[i]) return value * 10 + 1;
    }
    return value * 10;
}
void property_prepare(void) { assert(xlir_module_init() && !xrtGetError()); }
void property_idle(void)
{
    uint64_t started = xl6MonotonicMillisecondsV1();
    assert(xlir_continuation_executor);
    while (!xl6ContinuationExecutorIsIdle(xlir_continuation_executor)) {
        assert(xl6MonotonicMillisecondsV1() - started < 5000);
        xrtSleep(1);
    }
}
void property_finish(void) { xlir_module_fini(); assert(!pendingPromise && !pendingInput && !xrtGetError()); }
void property_cycle(void)
{
    int stage, mode, reject;
    const int asyncStages[] = {0, 3, 6, 7};
    checked(property_base_virtual(), 0, "base virtual", 0, 0);
    checked(property_nullable_return(), 0, "normalized nullable return", 0, 0);
    for (mode = 0; mode < 4; ++mode) for (stage = 0; stage < 8; ++stage) {
        checked(property_compound(mode, stage), order(0, mode, stage), "compound", mode, stage);
        checked(property_get(mode, stage), order(2, mode, stage), "get", mode, stage);
        if (mode < 3) checked(property_set(mode, stage), order(1, mode, stage), "set", mode, stage);
    }
    for (mode = 0; mode < 3; ++mode) checked(property_cleanup(mode), 0, "cleanup", mode, 8);
    for (mode = 0; mode < 4; ++mode) for (stage = 0; stage < 4; ++stage) for (reject = 0; reject < 2; ++reject) {
        const xerror* error;
        int failingGetter = asyncStages[stage] == 3;
        property_start(mode, asyncStages[stage]);
        xlirTestWaitForPropertyAwait(pendingInput, PROPERTY_PENDING, failingGetter != 0);
        assert(pendingPromise && xrtFutureDone(PROPERTY_PENDING) == (failingGetter != 0));
        property_replace();
        if (reject) {
            xerror* failure = xrtErrorCreate(XERR_STATE, "property-transaction", 71, "await failed");
            assert(failure && xrtPromiseReject(pendingPromise, failure)); xrtErrorFree(failure);
        } else assert(xrtPromiseResolve(pendingPromise, &payload));
        xrtPromiseDestroy(pendingPromise); pendingPromise = NULL;
        assert(xrtFutureWait(PROPERTY_PENDING) == 0 && xrtFutureDone(PROPERTY_PENDING));
        error = xrtFutureError(PROPERTY_PENDING);
        if (failingGetter || reject || asyncStages[stage] >= 6)
            assert(error && xrtErrorCode(error) == (failingGetter ? 73 : reject ? 71 : 73));
        else {
            const int64_t* value = (const int64_t*)xrtFutureValue(PROPERTY_PENDING);
            if (error) fprintf(stderr, "async mode %d: %s\n", mode, xrtErrorMessage(error));
            assert(!error && value && *value == 7);
        }
        xrtFutureDestroy(PROPERTY_PENDING); PROPERTY_PENDING = NULL;
        xrtFutureDestroy(pendingInput); pendingInput = NULL;
        assert(!xrtGetError());
    }
}
