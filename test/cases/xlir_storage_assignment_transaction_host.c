#include <assert.h>
#include <stdio.h>
#ifndef XLIR_STORAGE_TRANSACTION_SOURCE
#define XLIR_STORAGE_TRANSACTION_SOURCE "../../build/test/xlir_storage_assignment_transaction.c"
#define XLIR_STORAGE_TRANSACTION_BIND "../../build/test/xlir_storage_assignment_transaction.bind.h"
#endif
#include XLIR_STORAGE_TRANSACTION_SOURCE
#include XLIR_STORAGE_TRANSACTION_BIND
static xpromise* pendingPromise;
static int64_t payload;
extern bool xrtFutureDone(const xfuture*);
extern bool xrtPromiseResolve(xpromise*, void*);
void storage_transaction_fail(void)
{
    xl6_error_result failure = {false, NULL};
    failure.error = xrtErrorCreate(XERR_STATE, "storage-transaction", 73, "operation failed");
    assert(failure.error); xl6ErrorScopePropagateFailure(failure);
}
xfuture* storage_transaction_future(void)
{
    xfuture* result = NULL;
    assert(!pendingPromise); pendingPromise = xrtPromiseCreate(&result, NULL);
    assert(pendingPromise && result); return result;
}
static void checked(int64_t result, const char* name, int stage)
{
    if (xrtGetError()) fprintf(stderr, "%s stage %d at %d: %s\n", name, stage,
        xrtErrorLine(xrtGetError()), xrtErrorMessage(xrtGetError()));
    assert(result == 0 && !xrtGetError());
}
void transaction_prepare(void) { assert(xlir_module_init() && !xrtGetError()); }
void transaction_finish(void) { xlir_module_fini(); assert(!pendingPromise && !xrtGetError()); }
void transaction_cycle(void)
{
    int stage, mode;
    for (stage = 0; stage < 3; ++stage) {
        checked(transaction_local(stage), "local", stage);
        checked(transaction_cell(stage), "cell", stage);
        checked(transaction_static(stage), "static", stage);
        checked(transaction_global(stage), "global", stage);
    }
    for (mode = 0; mode < 4; ++mode) for (stage = 0; stage < 3; ++stage) {
        const xerror* error;
        transaction_start(mode);
        assert(pendingPromise && !xrtFutureDone(TRANSACTION_PENDING));
        transaction_replace();
        if (stage == 1) {
            xerror* failure = xrtErrorCreate(XERR_STATE, "storage-transaction", 71, "await failed");
            assert(failure && xrtPromiseReject(pendingPromise, failure)); xrtErrorFree(failure);
        } else {
            payload = mode == 3 && stage == 2 ? -4 : 4;
            assert(xrtPromiseResolve(pendingPromise, &payload));
        }
        xrtPromiseDestroy(pendingPromise); pendingPromise = NULL;
        assert(xrtFutureWait(TRANSACTION_PENDING) == 0 && xrtFutureDone(TRANSACTION_PENDING));
        error = xrtFutureError(TRANSACTION_PENDING);
        if (stage == 1 || (mode == 3 && stage == 2))
            assert(error && xrtErrorCode(error) == (stage == 1 ? 71 : 73));
        else {
            const int64_t* value = (const int64_t*)xrtFutureValue(TRANSACTION_PENDING);
            if (error) fprintf(stderr, "async mode %d: %s\n", mode, xrtErrorMessage(error));
            assert(!error && value && *value == 7);
        }
        xrtFutureDestroy(TRANSACTION_PENDING); TRANSACTION_PENDING = NULL;
        assert(!xrtGetError());
    }
}
