#include "../../build/test/xlir_dynamic_member_get.c"
#include "../../build/test/xlir_dynamic_member_get.bind.h"
#include <assert.h>
#include <stdio.h>
static xpromise* pendingPromise;
static int64_t payload = 42;
extern bool xrtFutureDone(const xfuture*);
extern bool xrtPromiseResolve(xpromise*, void*);
xfuture* dynamic_member_future(void)
{
    xfuture* result = NULL;
    assert(!pendingPromise); pendingPromise = xrtPromiseCreate(&result, NULL);
    assert(pendingPromise && result); return result;
}
void dynamic_member_cycle(void)
{
    int mode;
    assert(xlir_module_init() && !xrtGetError());
    assert(dynamic_member_check() == 0 && !xrtGetError());
    for (mode = 0; mode < 3; ++mode) {
        const xerror* error;
        dynamic_member_start();
        assert(pendingPromise && !xrtFutureDone(MEMBER_PENDING));
        if (mode == 1) dynamic_member_fail();
        if (mode == 2) {
            xerror* failure = xrtErrorCreate(XERR_STATE, "dynamic-member", 71, "await failed");
            assert(failure && xrtPromiseReject(pendingPromise, failure)); xrtErrorFree(failure);
        } else assert(xrtPromiseResolve(pendingPromise, &payload));
        xrtPromiseDestroy(pendingPromise); pendingPromise = NULL;
        assert(xrtFutureWait(MEMBER_PENDING) == 0 && xrtFutureDone(MEMBER_PENDING));
        error = xrtFutureError(MEMBER_PENDING);
        if (mode != 0) assert(error && (mode != 2 || xrtErrorCode(error) == 71));
        else {
            const int64_t* value = (const int64_t*)xrtFutureValue(MEMBER_PENDING);
            if (error) fprintf(stderr, "dynamic member: %s\n", xrtErrorMessage(error));
            assert(!error && value && *value == 42);
        }
        xrtFutureDestroy(MEMBER_PENDING); MEMBER_PENDING = NULL;
        assert(!xrtGetError());
    }
    xlir_module_fini(); assert(!xrtGetError());
}
