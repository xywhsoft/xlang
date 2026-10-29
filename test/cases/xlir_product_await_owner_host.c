#include <assert.h>
#ifndef XLIR_PRODUCT_AWAIT_SOURCE
#define XLIR_PRODUCT_AWAIT_SOURCE "../../build/test/xlir_product_await_owner.c"
#define XLIR_PRODUCT_AWAIT_BIND "../../build/test/xlir_product_await_owner.h.bind.h"
#endif
#include XLIR_PRODUCT_AWAIT_SOURCE
#include XLIR_PRODUCT_AWAIT_BIND
extern bool xrtFutureDone(const xfuture*);
extern bool xrtPromiseResolve(xpromise*, void*);
static xfuture* inputs[2];
static unsigned dropped[21], caught, finalized, assigned;
static int failDrop, withoutDiagnostic, secondaryFailures;
static int64_t observed;
static void fail(int64_t code)
{
    xl6_error_result failure = {false, NULL};
    if (!withoutDiagnostic || code == 99) {
        failure.error = xrtErrorCreate(XERR_STATE, "product-await", code, "injected cleanup failure");
        assert(failure.error);
    }
    xl6ErrorScopePropagateFailure(failure);
}
void product_await_drop(int64_t id)
{
    assert(id > 0 && id <= 20 && ++dropped[id] == 1);
    if (id == failDrop) fail(77);
    else if (secondaryFailures && (id == 1 || id == 2)) fail(99);
}
void product_await_caught(int64_t code) { ++caught; observed = code; }
void product_await_finally(void) { ++finalized; }
void product_await_assigned(int64_t first, int64_t second) { assert(first == 1 && second == 2); ++assigned; }
xfuture* product_await_input(int64_t which) { assert(which == 0 || which == 1); return xrtFutureRef(inputs[which]); }
void product_await_prepare(void)
{
    assert(xlir_module_init() && !xrtGetError());
    xrtFutureDestroy(PRODUCT_AWAIT_PENDING); PRODUCT_AWAIT_PENDING = NULL;
}
void product_await_finish(void) { xlir_module_fini(); assert(!xrtGetError()); }
static void complete(xpromise* promise, unsigned mode, int64_t* index, int64_t code)
{
    if (mode == 0) assert(xrtPromiseResolve(promise, index));
    else if (mode == 1) {
        xerror* error = xrtErrorCreate(XERR_STATE, "product-await", code, "input failed");
        assert(error && xrtPromiseReject(promise, error)); xrtErrorFree(error);
    } else if (mode == 2) assert(xrtPromiseCancel(promise));
    else assert(xrtPromiseClose(promise));
}
unsigned product_await_cycle(unsigned firstMode, unsigned secondMode, int drop, int noDiagnostic)
{
    xpromise *first, *second;
    int64_t firstIndex = 0, secondIndex = 1;
    const int64_t* result;
    int shouldFail = firstMode || secondMode || drop;
    /* XRT's canonical cancellation/closure diagnostics both have code 1;
     * diagnostic-free destructor failure is independently represented by 0. */
    int expectedCode = firstMode ? (firstMode == 1 ? 71 : 1) : drop == 10 ? (noDiagnostic ? 0 : 77)
        : secondMode ? (secondMode == 1 ? 72 : 1) : (noDiagnostic ? 0 : 77);
    unsigned suspensions = 1;
    memset(dropped, 0, sizeof(dropped)); caught = finalized = assigned = 0; observed = -1;
    failDrop = drop; withoutDiagnostic = noDiagnostic; secondaryFailures = shouldFail;
    first = xrtPromiseCreate(&inputs[0], NULL); second = xrtPromiseCreate(&inputs[1], NULL);
    assert(first && second && inputs[0] && inputs[1]);
    product_await_start();
    assert(PRODUCT_AWAIT_PENDING && !xrtFutureDone(PRODUCT_AWAIT_PENDING) && !caught && !finalized && !assigned);
    assert(!dropped[1] && !dropped[2] && !dropped[10] && !dropped[20]);
    complete(first, firstMode, &firstIndex, 71); xrtPromiseDestroy(first);
    if (!firstMode && drop != 10) {
        assert(!xrtFutureDone(PRODUCT_AWAIT_PENDING) && dropped[10] == 1 && !dropped[20]);
        assert(!dropped[1] && !dropped[2] && !caught && !finalized && !assigned); ++suspensions;
    }
    complete(second, secondMode, &secondIndex, 72); xrtPromiseDestroy(second);
    assert(xrtFutureWait(PRODUCT_AWAIT_PENDING) == 0);
    result = (const int64_t*)xrtFutureValue(PRODUCT_AWAIT_PENDING);
    if (xrtFutureState(PRODUCT_AWAIT_PENDING) != XFUTURE_RESOLVED || !result || *result != (shouldFail ? 23 : 0) ||
        (shouldFail && observed != expectedCode))
        fprintf(stderr, "product await first=%u second=%u drop=%d noDiagnostic=%d state=%d value=%lld caught=%u code=%lld finally=%u assigned=%u\n",
            firstMode, secondMode, drop, noDiagnostic, (int)xrtFutureState(PRODUCT_AWAIT_PENDING),
            (long long)(result ? *result : -1), caught, (long long)observed, finalized, assigned);
    assert(xrtFutureState(PRODUCT_AWAIT_PENDING) == XFUTURE_RESOLVED && result && *result == (shouldFail ? 23 : 0));
    assert(caught == (unsigned)(shouldFail != 0) && finalized == 1 && assigned == (unsigned)!shouldFail);
    if (shouldFail) assert(observed == expectedCode);
    assert(dropped[1] == 1 && dropped[2] == 1 && dropped[10] == 1 && dropped[20] == 1);
    xrtFutureDestroy(PRODUCT_AWAIT_PENDING); PRODUCT_AWAIT_PENDING = NULL;
    xrtFutureDestroy(inputs[0]); xrtFutureDestroy(inputs[1]); inputs[0] = inputs[1] = NULL;
    if (xrtGetError()) fprintf(stderr, "product await leaked error first=%u second=%u drop=%d noDiagnostic=%d kind=%d code=%d: %s\n",
        firstMode, secondMode, drop, noDiagnostic, (int)xrtErrorKind(xrtGetError()), xrtErrorCode(xrtGetError()), xrtErrorMessage(xrtGetError()));
    assert(!xrtGetError()); return suspensions;
}
