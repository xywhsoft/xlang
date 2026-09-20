#include <assert.h>
#include <stdio.h>
#ifndef XLIR_STORAGE_CONDITIONAL_SOURCE
#define XLIR_STORAGE_CONDITIONAL_SOURCE "../../build/test/xlir_storage_conditional_assign.c"
#define XLIR_STORAGE_CONDITIONAL_BIND "../../build/test/xlir_storage_conditional_assign.bind.h"
#endif
#include XLIR_STORAGE_CONDITIONAL_SOURCE
#include XLIR_STORAGE_CONDITIONAL_BIND
static xpromise* pendingPromise;
static int64_t payload;
static int pointerCalls;
extern bool xrtFutureDone(const xfuture*);
extern bool xrtPromiseResolve(xpromise*, void*);
void storage_fail(void)
{
    xl6_error_result failure = {false, NULL};
    failure.error = xrtErrorCreate(XERR_STATE, "storage-conditional", 73, "RHS failed");
    assert(failure.error); xl6ErrorScopePropagateFailure(failure);
}
void* storage_pointer(void) { ++pointerCalls; return &payload; }
xfuture* storage_future(void)
{
    xfuture* result = NULL;
    assert(!pendingPromise);
    pendingPromise = xrtPromiseCreate(&result, NULL);
    assert(pendingPromise && result); return result;
}
void storage_prepare(void) { assert(xlir_module_init()); }
void storage_finish(void) { xlir_module_fini(); assert(!pendingPromise && !xrtGetError()); }
void storage_sync_run(int mode, int present, int stage)
{
    int expectedValue = present ? 8 : stage == 0 ? 17 : -1;
    int expectedCalls = present ? 0 : stage == 2 ? 2 : 1;
    int expectedCaught = !present && stage == 1;
    int expected = expectedValue * 100 + expectedCalls * 10 + expectedCaught;
    int actual = (int)storage_sync(mode, present != 0, stage);
    if (actual != expected) fprintf(stderr, "storage mode=%d present=%d stage=%d: %d expected %d\n",
        mode, present, stage, actual, expected);
    assert(actual == expected && !xrtGetError());
    pointerCalls = 0;
    assert(storage_scalars() == 8 && pointerCalls == 1 && !xrtGetError());
}
void storage_async_run(int mode, int present, int reject)
{
    const xerror* error;
    storage_start(present != 0, mode);
    assert(pendingPromise && xrtFutureDone(STORAGE_PENDING) == (present != 0));
    storage_replace();
    if (reject) {
        xerror* failure = xrtErrorCreate(XERR_STATE, "storage-conditional", 71, "await failed");
        assert(failure && xrtPromiseReject(pendingPromise, failure)); xrtErrorFree(failure);
    } else {
        payload = 17; assert(xrtPromiseResolve(pendingPromise, &payload));
    }
    xrtPromiseDestroy(pendingPromise); pendingPromise = NULL;
    assert(xrtFutureWait(STORAGE_PENDING) == 0 && xrtFutureDone(STORAGE_PENDING));
    error = xrtFutureError(STORAGE_PENDING);
    if (reject && !present) assert(error && xrtErrorCode(error) == 71);
    else {
        const int64_t* value = (const int64_t*)xrtFutureValue(STORAGE_PENDING);
        assert(!error && value && *value == (present ? 8 : 17));
    }
    xrtFutureDestroy(STORAGE_PENDING); STORAGE_PENDING = NULL;
    assert(!xrtGetError());
}
void storage_generator_run(int mode, int present, int stop, int stage)
{
    int calls = !stop && !present;
    int caught = calls && stage == 1;
    int result = present ? 9 : 0;
    int actual;
    if (!stop && !caught) result = result * 100 + (present ? 9 : 18);
    actual = (int)storage_generator(mode, present != 0, stop != 0, stage);
    assert(actual == result * 100 + calls * 10 + caught && !xrtGetError());
}
