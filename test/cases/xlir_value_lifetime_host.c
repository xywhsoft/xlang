#include <assert.h>
#include "../../build/test/xlir_value_lifetime.c"
#include "../../build/test/xlir_value_lifetime.bind.h"

extern bool xrtFutureDone(const xfuture*);
extern bool xrtPromiseResolve(xpromise*, void*);
static xpromise* keyPromise;
static int64_t keyValue;

void xlir_lifetime_fail(void)
{
    xl6_error_result failure = {false, NULL};
    failure.error = xrtErrorCreate(XERR_STATE, "value-lifetime", 73, "address evaluation failed");
    assert(failure.error != NULL);
    xl6ErrorScopePropagateFailure(failure);
}

void lifetime_write_run(int stage, int kind)
{
    assert(xlir_lifetime_write_failure(stage, kind) == (stage == 0 ? 0 : 73));
    assert(xrtGetError() == NULL);
}

xfuture* xlir_lifetime_key(void)
{
    xfuture* result = NULL;
    assert(keyPromise == NULL);
    keyPromise = xrtPromiseCreate(&result, NULL);
    assert(keyPromise != NULL && result != NULL);
    return result;
}

void lifetime_prepare(void) { assert(xlir_module_init()); }

void lifetime_run(int mode, int reject)
{
    const xerror* failure;
    assert(xlir_lifetime_generator_probe() == 68);
    xlir_lifetime_index_reset();
    xlir_lifetime_index_start(mode);
    /* The key has not completed: this must be a real suspended continuation,
     * not an already-resolved Future or a blocking await in the launch path. */
    assert(keyPromise != NULL && !xrtFutureDone(XLIR_LIFETIME_PENDING));
    xlir_lifetime_index_replace();
    if (reject) {
        xerror* error = xrtErrorCreate(XERR_STATE, "value-lifetime", 71, "key failed after suspension");
        assert(error != NULL && xrtPromiseReject(keyPromise, error));
        xrtErrorFree(error);
    } else {
        keyValue = 0;
        assert(xrtPromiseResolve(keyPromise, &keyValue));
    }
    xrtPromiseDestroy(keyPromise); keyPromise = NULL;
    assert(xrtFutureWait(XLIR_LIFETIME_PENDING) == 0 && xrtFutureDone(XLIR_LIFETIME_PENDING));
    failure = xrtFutureError(XLIR_LIFETIME_PENDING);
    if (reject) assert(failure != NULL && xrtErrorCode(failure) == 71);
    else {
        const int64_t* result = (const int64_t*)xrtFutureValue(XLIR_LIFETIME_PENDING);
        assert(failure == NULL && result != NULL && *result == (mode % 3 == 2 || mode > 9 ? 0 : 17));
    }
    xlir_lifetime_index_check(mode, mode % 3 == 2 && !reject ? 23 : 17, reject != 0);
    /* Close the host-observed result to equalize success/rejection baselines. */
    xrtFutureDestroy(XLIR_LIFETIME_PENDING); XLIR_LIFETIME_PENDING = NULL;
    assert(xrtGetError() == NULL);
}

void lifetime_finish(void) { xlir_module_fini(); assert(keyPromise == NULL && xrtGetError() == NULL); }
