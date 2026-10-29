#define xl6ForeignCallBegin setter_scope_begin
#include "../../build/test/xlir_dynamic_member_set.c"
#undef xl6ForeignCallBegin
#include "../../build/test/xlir_dynamic_member_set.bind.h"
#include <assert.h>
#include <stdio.h>
static xpromise* pendingPromise;
static int64_t payload = 42;
static unsigned allocationFaults;
static bool failScopeBegin;
extern bool xl6ForeignCallBegin(xl6_foreign_error_scope*);
bool setter_scope_begin(xl6_foreign_error_scope* scope)
{
    if (failScopeBegin) {
        failScopeBegin = false;
        xrtSetErrorTake(xrtErrorCreate(XERR_STATE, "dynamic-setter", 78, "scope binding failed"));
        return false;
    }
    return xl6ForeignCallBegin(scope);
}
extern bool xrtFutureDone(const xfuture*);
extern bool xrtPromiseResolve(xpromise*, void*);
extern bool xrtMemDebugFailAfter(uint64_t);
extern bool xrtMemDebugFailTriggered(void);
extern void xrtMemDebugFailClear(void);
void dynamic_setter_raise(int64_t code)
{
    xl6_error_result failure = {false, NULL};
    failure.error = xrtErrorCreate(XERR_STATE, "dynamic-setter", (int)code, "setter operation failed");
    assert(failure.error); xl6ErrorScopePropagateFailure(failure);
}
xfuture* dynamic_setter_future(void)
{
    xfuture* result = NULL;
    assert(!pendingPromise); pendingPromise = xrtPromiseCreate(&result, NULL);
    assert(pendingPromise && result); return result;
}
void dynamic_setter_cycle(int smoke)
{
    int mode;
    assert(xlir_module_init() && !xrtGetError());
    assert(dynamic_setter_check() == 0);
    if (xrtGetError()) fprintf(stderr, "setter check: %d: %s\n", xrtErrorCode(xrtGetError()), xrtErrorMessage(xrtGetError()));
    assert(!xrtGetError());
    dynamic_setter_uncaught();
    assert(xrtGetError() && xrtErrorCode(xrtGetError()) == 73);
    xrtErrorFree(xrtTakeError());
    {
        xvalue* receiver = xrtValueObject();
        xvalue* value = xrtValueInt(43);
        assert(receiver && value);
        failScopeBegin = true;
        assert(!SETTER_AMOUNT_DISPATCH(receiver, 1, &value, 0, NULL, NULL));
        assert(!failScopeBegin && xrtGetError() && xrtErrorCode(xrtGetError()) == 78);
        xrtErrorFree(xrtTakeError()); xrtValueRelease(receiver);
    }
    if (!smoke) {
        size_t fail;
        for (fail = 0; fail < 4; ++fail) {
            xvalue* receiver = xrtValueObject();
            xvalue* value = xrtValueInt(43);
            xvalue* result;
            bool triggered;
            assert(receiver && value && xrtMemDebugFailAfter(fail));
            result = SETTER_AMOUNT_DISPATCH(receiver, 1, &value, 0, NULL, NULL);
            triggered = xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
            if (triggered) { ++allocationFaults; assert(result == NULL); }
            else assert(result && !xrtGetError());
            xrtErrorFree(xrtTakeError()); xrtValueRelease(result); xrtValueRelease(receiver);
            assert(!xrtGetError());
        }
    }
    for (mode = 0; mode < 3; ++mode) {
        const xerror* error;
        dynamic_setter_start();
        assert(pendingPromise && !xrtFutureDone(SETTER_PENDING));
        if (mode == 1) dynamic_setter_fail();
        if (mode == 2) {
            xerror* failure = xrtErrorCreate(XERR_STATE, "dynamic-setter", 77, "await failed");
            assert(failure && xrtPromiseReject(pendingPromise, failure)); xrtErrorFree(failure);
        } else assert(xrtPromiseResolve(pendingPromise, &payload));
        xrtPromiseDestroy(pendingPromise); pendingPromise = NULL;
        assert(xrtFutureWait(SETTER_PENDING) == 0 && xrtFutureDone(SETTER_PENDING));
        error = xrtFutureError(SETTER_PENDING);
        if (mode != 0) assert(error && xrtErrorCode(error) == (mode == 1 ? 72 : 77));
        else {
            const int64_t* value = (const int64_t*)xrtFutureValue(SETTER_PENDING);
            if (error) fprintf(stderr, "dynamic setter: %s\n", xrtErrorMessage(error));
            assert(!error && value && *value == 62);
        }
        xrtFutureDestroy(SETTER_PENDING); SETTER_PENDING = NULL;
        assert(!xrtGetError());
    }
    xlir_module_fini(); assert(!xrtGetError());
}
unsigned dynamic_setter_allocation_faults(void) { return allocationFaults; }
