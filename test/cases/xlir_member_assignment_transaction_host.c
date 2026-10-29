#include <assert.h>
#include <stdio.h>
#include "../../build/test/xlir_member_assignment_transaction.c"
#include "../../build/test/xlir_member_assignment_transaction.bind.h"
static xpromise* pendingPromise;
static int64_t payload;
extern bool xrtFutureDone(const xfuture*);
extern bool xrtPromiseResolve(xpromise*, void*);
void member_fail(void)
{
    xl6_error_result failure = {false, NULL};
    failure.error = xrtErrorCreate(XERR_STATE, "member-assignment", 73, "operand failed");
    assert(failure.error); xl6ErrorScopePropagateFailure(failure);
}
xfuture* member_future(void)
{
    xfuture* result = NULL;
    assert(!pendingPromise);
    pendingPromise = xrtPromiseCreate(&result, NULL);
    assert(pendingPromise && result); return result;
}
void member_prepare(void) { assert(xlir_module_init()); assert(member_defaults() == 0 && !xrtGetError()); }
void member_finish(void) { xlir_module_fini(); assert(!pendingPromise && !xrtGetError()); }
void member_sync_run(int mode, int present, int stage)
{
    int value = present ? 8 : stage == 0 ? 17 : -1;
    int calls = present || stage == 3 ? 0 : stage == 2 ? 2 : 1;
    int caught = stage == 3 || (!present && stage == 1);
    int reads = caught ? 1 : 2;
    int expected = value * 1000 + reads * 100 + calls * 10 + caught;
    int actual = (int)member_sync(mode, present != 0, stage);
    if (actual != expected) fprintf(stderr, "member mode=%d present=%d stage=%d: %d expected %d\n",
        mode, present, stage, actual, expected);
    if (xrtGetError()) fprintf(stderr, "member failure at %d: %s\n", xrtErrorLine(xrtGetError()), xrtErrorMessage(xrtGetError()));
    assert(actual == expected && !xrtGetError());
}
void member_regular_run(void)
{
    assert(member_regular(0) == 0 && !xrtGetError());
    assert(member_regular(1) == 0 && !xrtGetError());
    assert(member_regular(3) == 0 && !xrtGetError());
    assert(member_word(0) == 0 && !xrtGetError());
    assert(member_word(1) == 0 && !xrtGetError());
    assert(member_word(3) == 0 && !xrtGetError());
    assert(member_static() == 0 && !xrtGetError());
}
void member_async_run(int mode, int present, int reject)
{
    const xerror* error;
    member_start(present != 0, mode);
    assert(pendingPromise && xrtFutureDone(MEMBER_PENDING) == (present != 0));
    member_replace();
    if (reject) {
        xerror* failure = xrtErrorCreate(XERR_STATE, "member-assignment", 71, "await failed");
        assert(failure && xrtPromiseReject(pendingPromise, failure)); xrtErrorFree(failure);
    } else { payload = 17; assert(xrtPromiseResolve(pendingPromise, &payload)); }
    xrtPromiseDestroy(pendingPromise); pendingPromise = NULL;
    assert(xrtFutureWait(MEMBER_PENDING) == 0 && xrtFutureDone(MEMBER_PENDING));
    error = xrtFutureError(MEMBER_PENDING);
    if (reject && !present) assert(error && xrtErrorCode(error) == 71);
    else {
        const int64_t* value = (const int64_t*)xrtFutureValue(MEMBER_PENDING);
        assert(!error && value && *value == (present ? 8 : 17));
    }
    xrtFutureDestroy(MEMBER_PENDING); MEMBER_PENDING = NULL;
    assert(!xrtGetError());
}
void member_generator_run(int mode, int present, int stop, int stage)
{
    int calls = !stop && !present;
    int caught = calls && stage == 1;
    int result = present ? 9 : 0;
    int expected, actual;
    if (!stop && !caught) result = result * 100 + (present ? 9 : 18);
    expected = result * 1000 + (!stop ? 100 : 0) + calls * 10 + caught;
    actual = (int)member_generator(mode, present != 0, stop != 0, stage);
    assert(actual == expected && !xrtGetError());
}
