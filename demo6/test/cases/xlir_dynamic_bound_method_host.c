#define xrtCallResultSetTake bound_result_set_take
#include "../../build/test/xlir_dynamic_bound_method.c"
#undef xrtCallResultSetTake
#include "../../build/test/xlir_dynamic_bound_method.bind.h"
#include <assert.h>
#include <stdio.h>
static xpromise* pendingPromise;
static int64_t payload = 42;
static int failTransport;
extern bool xrtCallResultSetTake(xrtcallresult*, size_t, xvalue**);
extern bool xrtMemDebugFailAfter(uint64_t);
extern bool xrtMemDebugFailTriggered(void);
extern void xrtMemDebugFailClear(void);
extern bool xrtValueObjectRemove(xvalue*, xlir_string_view);
bool bound_result_set_take(xrtcallresult* result, size_t index, xvalue** value)
{
    if (failTransport) {
        failTransport = 0;
        xrtSetErrorTake(xrtErrorCreate(XERR_STATE, "bound-transport", 79, "bound result transport failed"));
        return false;
    }
    return xrtCallResultSetTake(result, index, value);
}
extern bool xrtFutureDone(const xfuture*);
extern bool xrtPromiseResolve(xpromise*, void*);
xfuture* dynamic_bound_future(void)
{
    xfuture* result = NULL;
    assert(!pendingPromise); pendingPromise = xrtPromiseCreate(&result, NULL);
    assert(pendingPromise && result); return result;
}
static xvalue* bound_make_from_export(void)
{
    xvalue* result;
    dynamic_bound_prepare();
    if (xrtGetError()) return NULL;
    result = BOUND_FAULT_CALLABLE;
    BOUND_FAULT_CALLABLE = xrtValueRetain(xrtValueNull());
    return result;
}
static void bound_factory_faults(int allocationFaults)
{
    int index;
    xvalue* callable; xvalue* argument; xvalue* arguments[1];
    xrtcallframe frame = {0}; xrtcallresult result = {0}; int64_t number;
    /* Native object storage can contain a present null member with the same
     * name as a registered method. It must not be treated as an absent key. */
    assert(xrtValueObjectSetNew(BOUND_FAULT_RECEIVER, xrtStrView("read"), xrtValueRetain(xrtValueNull())));
    callable = bound_make_from_export();
    assert(callable && xrtValueType(callable) == 0 && !xrtGetError());
    xrtValueRelease(callable);
    assert(xrtValueObjectRemove(BOUND_FAULT_RECEIVER, xrtStrView("read")));
    if (allocationFaults) {
        for (index = 0; index < 3; ++index) {
            assert(xrtMemDebugFailAfter((uint64_t)index));
            callable = bound_make_from_export();
            assert(xrtMemDebugFailTriggered()); xrtMemDebugFailClear();
            assert(callable == NULL && xrtGetError());
            xrtErrorFree(xrtTakeError());
        }
    }
    callable = bound_make_from_export(); argument = xrtValueInt(4);
    assert(callable && argument && !xrtGetError());
    arguments[0] = argument; frame.ArgumentCount = 1; frame.Arguments = arguments;
    for (index = 0; index < (allocationFaults ? 3 : 2); ++index) {
        bool success;
        failTransport = index == 1;
        if (index == 2) assert(xrtMemDebugFailAfter(0));
        success = xrtValueInvoke(callable, &frame, &result);
        if (index == 2) { assert(xrtMemDebugFailTriggered()); xrtMemDebugFailClear(); }
        assert(!failTransport);
        if (index == 0) {
            assert(success && xrtCallResultCount(&result) == 1 && !xrtGetError());
            assert(xrtValueGetInt(xrtCallResultGet(&result, 0), &number) && number == 59);
        } else {
            const xerror* error = xrtGetError();
            assert(!success && error && xrtCallResultCount(&result) == 0);
            if (index == 1) {
                while (error && xrtErrorCode(error) != 79) error = xrtErrorCause(error);
                assert(error && xrtErrorCode(error) == 79);
            }
            xrtErrorFree(xrtTakeError());
        }
        xrtCallResultUnit(&result);
        assert(xrtValueGetInt(argument, &number) && number == 4);
    }
    xrtValueRelease(argument); xrtValueRelease(callable);
    assert(!xrtGetError());
}
void dynamic_bound_cycle(int allocationFaults)
{
    int mode;
    assert(xlir_module_init() && !xrtGetError());
    {
        int64_t result = dynamic_bound_check();
        if (xrtGetError()) fprintf(stderr, "bound method check: %s\n", xrtErrorMessage(xrtGetError()));
        assert(result == 0 && !xrtGetError());
    }
    bound_factory_faults(allocationFaults);
    for (mode = 0; mode < 3; ++mode) {
        const xerror* error;
        dynamic_bound_start();
        assert(pendingPromise && !xrtFutureDone(BOUND_PENDING));
        if (mode == 1) dynamic_bound_fail();
        if (mode == 2) {
            xerror* failure = xrtErrorCreate(XERR_STATE, "dynamic-member", 71, "await failed");
            assert(failure && xrtPromiseReject(pendingPromise, failure)); xrtErrorFree(failure);
        } else assert(xrtPromiseResolve(pendingPromise, &payload));
        xrtPromiseDestroy(pendingPromise); pendingPromise = NULL;
        assert(xrtFutureWait(BOUND_PENDING) == 0 && xrtFutureDone(BOUND_PENDING));
        error = xrtFutureError(BOUND_PENDING);
        if (mode != 0) assert(error && (mode != 2 || xrtErrorCode(error) == 71));
        else {
            const int64_t* value = (const int64_t*)xrtFutureValue(BOUND_PENDING);
            if (error) fprintf(stderr, "dynamic bound method: %s\n", xrtErrorMessage(error));
            assert(!error && value && *value == 115);
        }
        xrtFutureDestroy(BOUND_PENDING); BOUND_PENDING = NULL;
        assert(!xrtGetError());
    }
    xlir_module_fini(); assert(!xrtGetError());
}
