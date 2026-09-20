#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "../../build/test/xlir_pending_exception_control_bind.h"
#include "../../build/test/xlir_pending_exception_control.c"
void pending_control_oom(void);
extern bool xrtPromiseResolve(xpromise*, void*);
static int fault, lane, liveMask, observed, raised, deferredCalls;
static int64_t order;
static xfuture* deferredFuture;
void control_open(int64_t id)
{
    int bit = 1 << (int)(id - 1);
    assert(id >= 1 && id <= 4 && !(liveMask & bit)); liveMask |= bit;
}
void control_drop(int64_t id)
{
    int bit = 1 << (int)(id - 1);
    assert(id >= 1 && id <= 4 && (liveMask & bit));
    liveMask &= ~bit; order = order * 10 + id;
    if (fault >= 4 && raised && id != 1) {
        xerror* error = xrtErrorCreate(15, "control-drop", 100 + (int)id, "secondary cleanup failure");
        assert(error != NULL); xrtSetErrorTake(error);
    }
}
static void control_raise(int code)
{
    xl6_error_result outcome = {false, NULL};
    if (fault == 0) return;
    raised = 1;
    if (fault == 3) pending_control_oom();
    else if (fault == 1 || fault == 4) {
        outcome.error = xrtErrorCreate(15, "control-primary", code, "pending exception");
        assert(outcome.error != NULL);
    }
    xl6ErrorScopePropagateFailure(outcome);
}
void control_primary(void) { control_raise(777); }
void control_secondary(void) { control_raise(888); }
void control_observe(int64_t code, int64_t stage)
{
    int expected = lane == 6 ? 999 : fault == 1 || fault == 4 ? (lane == 3 ? 888 : 777) : 0;
    int expectedLive = lane == 3 ? 5 : 1;
    int expectedStage = lane == 3 ? 1 : lane == 7 ? 3 : 2;
    if (code != expected || liveMask != expectedLive || stage != expectedStage)
        fprintf(stderr, "control observe lane=%d fault=%d code=%lld expected=%d live=%d stage=%lld order=%lld\n",
            lane, fault, (long long)code, expected, liveMask, (long long)stage, (long long)order);
    assert(code == expected && liveMask == expectedLive && stage == expectedStage); ++observed;
}
xfuture* control_deferred(void)
{
    assert(deferredFuture != NULL && xrtFutureState(deferredFuture) == XFUTURE_PENDING);
    assert(liveMask == 5 && order == 2); ++deferredCalls;
    return xrtFutureRef(deferredFuture);
}
static int64_t control_run_async(void)
{
    static int64_t seven = 7;
    xfuture* result;
    xpromise* promise = xrtPromiseCreate(&deferredFuture, NULL);
    int64_t value;
    assert(promise != NULL && deferredFuture != NULL);
    result = PENDING_CONTROL_ASYNC_START();
    /* Nothing has completed the input: these assertions prove a real yield
     * back to the host while the pending diagnostic is frame-owned. */
    assert(result != NULL && deferredCalls == 1 && xrtFutureState(result) == XFUTURE_PENDING);
    assert(liveMask == 5 && order == 2 && xrtGetError() == NULL);
    if (lane == 6) {
        xerror* error = xrtErrorCreate(15, "control-await", 999, "explicit await failure");
        assert(error != NULL && xrtPromiseReject(promise, error)); xrtErrorFree(error);
    } else assert(xrtPromiseResolve(promise, &seven));
    assert(xrtFutureWait(result) == 0 && xrtFutureState(result) == XFUTURE_RESOLVED);
    assert(xrtFutureValue(result) != NULL); value = *(const int64_t*)xrtFutureValue(result);
    xrtPromiseDestroy(promise); xrtFutureDestroy(deferredFuture); deferredFuture = NULL;
    xrtFutureDestroy(result); return value;
}
void pending_control_case(int index, int mode)
{
    xl6_error_scope boundary = {0};
    xl6_error_result outcome;
    int64_t result = -1, expectedOrder, expectedResult;
    int expectedFailure, expectedCode, expectedObserved;
    lane = index; fault = mode; liveMask = observed = raised = deferredCalls = 0; order = 0;
    assert(xrtGetError() == NULL && xlir_module_init() && xl6ErrorScopeBegin(&boundary));
    switch (lane) {
    case 0: result = control_return(); break;
    case 1: result = control_break(); break;
    case 2: result = control_continue(); break;
    case 3: result = control_caught(); break;
    case 4: result = control_nested(); break;
    case 5: case 6: result = control_run_async(); break;
    case 7: result = control_generator(); break;
    default: assert(0);
    }
    outcome = xl6ErrorScopeEnd(&boundary);
    expectedFailure = lane <= 2 ? fault >= 4 : lane <= 4 ? fault != 0 : 0;
    expectedCode = lane <= 2 ? 103 : fault == 1 || fault == 4 ? (lane == 4 ? 888 : 777) : 0;
    expectedObserved = lane == 6 ? 1 : lane == 3 || lane == 5 || lane == 7 ? fault != 0 : 0;
    expectedOrder = lane == 4 ? 2431 : lane == 2 && fault < 4 ? 23231 : 231;
    expectedResult = lane == 2 ? 2 : lane == 7 ? (fault == 0 ? 16 : 7) :
        lane == 6 || (lane == 5 && fault != 0) ? 111 : 9;
    if ((int)outcome.ok != !expectedFailure || liveMask != 0 || order != expectedOrder ||
        observed != expectedObserved || (!expectedFailure && result != expectedResult) ||
        (expectedFailure && xrtErrorCode(outcome.error) != expectedCode))
        fprintf(stderr, "control lane=%d fault=%d result=%lld ok=%d code=%d live=%d order=%lld expected=%lld observed=%d\n",
            lane, fault, (long long)result, (int)outcome.ok, xrtErrorCode(outcome.error), liveMask,
            (long long)order, (long long)expectedOrder, observed);
    assert((int)outcome.ok == !expectedFailure && liveMask == 0 && order == expectedOrder && observed == expectedObserved);
    assert(expectedFailure ? xrtErrorCode(outcome.error) == expectedCode : result == expectedResult && outcome.error == NULL);
    xrtErrorFree(outcome.error);
    xlir_module_fini(); assert(xrtGetError() == NULL);
}
