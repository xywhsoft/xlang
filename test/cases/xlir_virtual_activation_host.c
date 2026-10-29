#include <assert.h>
#include <stdio.h>
#include "../../build/test/xlir_virtual_activation.c"
#include "../../build/test/xlir_virtual_activation.bind.h"
extern bool xrtMemDebugFailAfter(uint64_t);
extern bool xrtMemDebugFailTriggered(void);
extern void xrtMemDebugFailClear(void);
extern bool xrtFutureDone(const xfuture*);
extern bool xrtPromiseResolve(xpromise*, void*);
static xpromise* pendingPromise;
static int64_t payload = 4;
static unsigned faults, suspensions;
void activation_fail(int64_t code)
{
    xl6_error_result failure = {false, NULL};
    failure.error = xrtErrorCreate(XERR_STATE, "activation", (int)code, "worker failure");
    assert(failure.error); xl6ErrorScopePropagateFailure(failure);
}
xfuture* activation_future(void)
{
    xfuture* result = NULL;
    assert(!pendingPromise); pendingPromise = xrtPromiseCreate(&result, NULL);
    assert(pendingPromise && result); return result;
}
void activation_prepare(void) { assert(xlir_module_init() && !xrtGetError()); }
void activation_finish(void) { xlir_module_fini(); assert(!pendingPromise && !xrtGetError()); }
void activation_cycle(int smoke)
{
    int mode, outcome;
    assert(activation_generators() == 0 && !xrtGetError());
    assert(activation_delayed() == 0 && !xrtGetError());
    for (mode = 0; mode < 3; ++mode) for (outcome = 0; outcome < 3; ++outcome) {
        const xerror* error;
        int64_t before = ACTIVATION_DROPS;
        activation_start(mode, outcome == 1);
        assert(!xrtGetError() && pendingPromise && !xrtFutureDone(ACTIVATION_PENDING));
        assert(ACTIVATION_DROPS == before); ++suspensions;
        if (outcome == 2) {
            xerror* rejected = xrtErrorCreate(XERR_STATE, "activation", 83, "await failure");
            assert(rejected && xrtPromiseReject(pendingPromise, rejected)); xrtErrorFree(rejected);
        } else assert(xrtPromiseResolve(pendingPromise, &payload));
        xrtPromiseDestroy(pendingPromise); pendingPromise = NULL;
        assert(xrtFutureWait(ACTIVATION_PENDING) == 0 && xrtFutureDone(ACTIVATION_PENDING));
        error = xrtFutureError(ACTIVATION_PENDING);
        if (outcome) assert(error && xrtErrorCode(error) == (outcome == 1 ? 82 : 83));
        else {
            const int64_t* value = (const int64_t*)xrtFutureValue(ACTIVATION_PENDING);
            assert(!error && value && *value == 24);
        }
        xrtFutureDestroy(ACTIVATION_PENDING); ACTIVATION_PENDING = NULL;
        assert(!xrtGetError() && ACTIVATION_DROPS == before + 1);
    }
    if (!smoke) {
        unsigned index, dispatch;
        int64_t bodies = ACTIVATION_BODIES;
        /* Exactly frame, string capture, then generator allocation. The
         * persistent borrowed receiver must survive every partial failure. */
        for (dispatch = 0; dispatch < 3; ++dispatch) for (index = 0; index < 4; ++index) {
            bool triggered;
            ACTIVATION_FAULT_RESULT result = {false};
            assert(xrtMemDebugFailAfter(index));
            if (dispatch == 0) {
                result = ACTIVATION_DIRECT(ACTIVATION_RECEIVER, "capture", false);
            } else if (dispatch == 1) result = ACTIVATION_DISPATCH(ACTIVATION_RECEIVER, "capture", false);
            else result = ACTIVATION_PROTOCOL(ACTIVATION_RECEIVER, "capture", false);
            triggered = xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
            if (index < 3) {
                assert(triggered && !result.ok && !result.value && xrtGetError()); ++faults;
            } else assert(!triggered && result.ok && result.value && !xrtGetError());
            xrtErrorFree(xrtTakeError());
            if (result.ok) xlir_generator_destroy(result.value);
            assert(!xrtGetError() && ACTIVATION_BODIES == bodies);
        }
    }
}
unsigned activation_fault_count(void) { return faults; }
unsigned activation_suspension_count(void) { return suspensions; }
