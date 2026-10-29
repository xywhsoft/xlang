#define xl6ForeignCallBegin protocol_scope_begin
#include "../../build/test/xlir_protocol_call_outcome.c"
#undef xl6ForeignCallBegin
#include "../../build/test/xlir_protocol_call_outcome.bind.h"
#include <assert.h>
#include <stdio.h>
static xpromise* pendingPromise;
static int64_t payload = 42;
static bool failScopeBegin;
static int nativeMode;
extern bool xl6ForeignCallBegin(xl6_foreign_error_scope*);
bool protocol_scope_begin(xl6_foreign_error_scope* scope)
{
    if (failScopeBegin) {
        failScopeBegin = false;
        xrtSetErrorTake(xrtErrorCreate(XERR_STATE, "protocol-outcome", 78, "scope binding failed"));
        return false;
    }
    return xl6ForeignCallBegin(scope);
}
extern bool xrtFutureDone(const xfuture*);
extern bool xrtPromiseResolve(xpromise*, void*);
void protocol_outcome_raise(int64_t code)
{
    xl6_error_result failure = {false, NULL};
    failure.error = xrtErrorCreate(XERR_STATE, "protocol-outcome", (int)code, "protocol operation failed");
    assert(failure.error); xl6ErrorScopePropagateFailure(failure);
}
void* protocol_outcome_native_open(void)
{
    int64_t* value = (int64_t*)xrtMalloc(sizeof(*value));
    assert(value); *value = 81; return value;
}
void protocol_outcome_native_close(void* pointer) { xrtFree(pointer); }
int64_t protocol_outcome_native_take(void* pointer, xvalue* value)
{
    int64_t result = *(int64_t*)pointer;
    if (nativeMode) protocol_outcome_raise(81);
    xrtValueRelease(value);
    return result;
}
const char* protocol_outcome_native_text(void* pointer)
{
    assert(pointer);
    if (nativeMode) protocol_outcome_raise(82);
    return "native-text";
}
void protocol_outcome_native_mode(int64_t mode) { nativeMode = (int)mode; }
void protocol_outcome_arm_scope_failure(void) { assert(!failScopeBegin); failScopeBegin = true; }
xfuture* protocol_outcome_future(void)
{
    xfuture* result = NULL;
    assert(!pendingPromise); pendingPromise = xrtPromiseCreate(&result, NULL);
    assert(pendingPromise && result); return result;
}
void protocol_outcome_cycle(void)
{
    int witness, mode;
    assert(xlir_module_init() && !xrtGetError());
    assert(protocol_outcome_check() == 0);
    if (xrtGetError()) fprintf(stderr, "protocol check: %d: %s\n", xrtErrorCode(xrtGetError()), xrtErrorMessage(xrtGetError()));
    assert(!xrtGetError());
    protocol_outcome_uncaught();
    assert(xrtGetError() && xrtErrorCode(xrtGetError()) == 73);
    xrtErrorFree(xrtTakeError());
    assert(protocol_outcome_native_check() == 0 && !xrtGetError() && !failScopeBegin);
    protocol_outcome_native_uncaught(false);
    assert(xrtGetError() && xrtErrorCode(xrtGetError()) == 81);
    xrtErrorFree(xrtTakeError());
    protocol_outcome_native_uncaught(true);
    assert(xrtGetError() && xrtErrorCode(xrtGetError()) == 78 && !failScopeBegin);
    xrtErrorFree(xrtTakeError());
    for (witness = 0; witness < 2; ++witness) for (mode = 0; mode < 3; ++mode) {
        const xerror* error;
        protocol_outcome_start(witness != 0);
        assert(pendingPromise && !xrtFutureDone(PROTOCOL_PENDING));
        if (mode == 1) protocol_outcome_fail();
        if (mode == 2) {
            xerror* failure = xrtErrorCreate(XERR_STATE, "protocol-outcome", 77, "await failed");
            assert(failure && xrtPromiseReject(pendingPromise, failure)); xrtErrorFree(failure);
        } else assert(xrtPromiseResolve(pendingPromise, &payload));
        xrtPromiseDestroy(pendingPromise); pendingPromise = NULL;
        assert(xrtFutureWait(PROTOCOL_PENDING) == 0 && xrtFutureDone(PROTOCOL_PENDING));
        error = xrtFutureError(PROTOCOL_PENDING);
        if (mode != 0) assert(error && xrtErrorCode(error) == (mode == 1 ? (witness ? 79 : 76) : 77));
        else {
            const int64_t* value = (const int64_t*)xrtFutureValue(PROTOCOL_PENDING);
            if (error) fprintf(stderr, "protocol async: %s\n", xrtErrorMessage(error));
            assert(!error && value && *value == (witness ? 43 : 32));
        }
        xrtFutureDestroy(PROTOCOL_PENDING); PROTOCOL_PENDING = NULL;
        assert(!xrtGetError());
    }
    xlir_module_fini(); assert(!xrtGetError());
}
