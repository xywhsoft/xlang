#include <assert.h>
#include <stdio.h>
#include "../../build/test/xlir_owned_activation.c"
#include "../../build/test/xlir_owned_activation.bind.h"
#define XLIR_TEST_LAUNCH_FUTURE
#define XLIR_TEST_LAUNCH_GENERATOR
#include "xlir_launch_test.h"
extern bool xrtMemDebugFailAfter(uint64_t);
extern bool xrtMemDebugFailTriggered(void);
extern void xrtMemDebugFailClear(void);
extern int xrtTaskPoolWait(xtaskpool*);
extern bool xrtTaskPoolClose(xtaskpool*);
extern bool xrtTaskPoolDestroy(xtaskpool*);
extern bool xrtPromiseResolve(xpromise*, void*);
extern bool xrtFutureDone(const xfuture*);
extern void owned_capture_record_drop(void);
extern void owned_capture_record_body(void);
extern unsigned owned_capture_drop_count(void);
#define drops owned_capture_drop_count()
static unsigned faults, suspensions;
static int raisingDrop, raisingBody;
static int64_t payload = 3;
void owned_capture_drop(int64_t id)
{
    owned_capture_record_drop();
    if (raisingDrop) {
        xl6_error_result error = {false, xrtErrorCreate(XERR_STATE, "owned-capture", (int)(100 + id), "destroy failure")};
        assert(error.error); xl6ErrorScopePropagateFailure(error);
    }
}
void owned_capture_body(bool fail)
{
    owned_capture_record_body();
    if (fail || raisingBody) {
        xl6_error_result error = {false, xrtErrorCreate(XERR_STATE, "owned-capture", 81, "body failure")};
        assert(error.error); xl6ErrorScopePropagateFailure(error);
    }
}
xfuture* owned_capture_input(void)
{
    xfuture* result = NULL; xpromise* promise = xrtPromiseCreate(&result, NULL);
    assert(promise && result && xrtPromiseResolve(promise, &payload)); xrtPromiseDestroy(promise); return result;
}
static void clear_error(void) { xrtErrorFree(xrtTakeError()); }
static void future_result(xfuture* result, xfuturestate state, int code, int64_t value)
{
    const xerror* error;
    assert(result && xrtFutureWait(result) == 0 && xrtFutureState(result) == state);
    error = xrtFutureError(result);
    if (code) assert(error && xrtErrorCode(error) == code);
    else assert(!error);
    if (state == XFUTURE_RESOLVED && value >= 0) {
        const int64_t* item = (const int64_t*)xrtFutureValue(result);
        assert(item && *item == value);
    }
    xrtFutureDestroy(result); assert(!xrtGetError());
}
void owned_capture_prepare(void) { assert(xlir_module_init() && !xrtGetError()); }
static void restart_module(void)
{
    xlir_module_fini(); assert(!xrtGetError());
    assert(xlir_async_pool == NULL);
    assert(xlir_module_initialized == 0);
    owned_capture_prepare();
}
void owned_capture_finish(void)
{
    xlir_module_fini(); assert(!xrtGetError());
    assert(xlir_async_pool == NULL);
    assert(xlir_module_initialized == 0);
}
void owned_capture_cycle(int smoke)
{
    unsigned before; int mode;
    for (mode = 0; mode < 4; ++mode) {
        xlir_generator* generator; xlir_generator_step step; int64_t value;
        before = drops; raisingBody = mode == 2; raisingDrop = mode >= 2;
        generator = xlir_test_observe_generator_launch(CAPTURE_ownedGenerator("key", owned_capture_new(4), owned_capture_new(5)));
        assert(generator && drops == before && !xrtGetError());
        if (mode != 1) {
            step = xlir_generator_next(generator);
            if (mode == 2) assert(!step.ok && !step.value && xrtErrorCode(xrtGetError()) == 81);
            else {
                assert(step.ok && step.value && xlir_generator_item_get(generator, &value, sizeof(value), false) && value == 7);
                step = xlir_generator_next(generator); assert(step.ok && step.value);
                step = xlir_generator_next(generator);
                if (mode == 3) assert(!step.ok && xrtErrorCode(xrtGetError()) == 105);
                else assert(step.ok && !step.value);
            }
        }
        clear_error(); xlir_generator_destroy(generator); clear_error();
        assert(drops == before + 2); raisingBody = raisingDrop = 0;
    }
    for (mode = 0; mode < 2; ++mode) {
        xfuture* result;
        before = drops; raisingDrop = mode;
        result = xlir_test_observe_future_launch(CAPTURE_ownedReady(owned_capture_new(4), owned_capture_new(5)));
        future_result(result, mode ? XFUTURE_FAILED : XFUTURE_RESOLVED, mode ? 105 : 0, 7);
        assert(drops == before + 2); raisingDrop = 0;
    }
    if (!smoke) for (mode = 0; mode < 2; ++mode) {
        xvalue *first = owned_capture_new(4), *unused = owned_capture_new(5);
        xfuture* result;
        before = drops; raisingDrop = mode;
        assert(xl6TaskRuntimeV1Get()->PoolIsQuiescent(xlir_async_pool) && xrtMemDebugFailAfter(0));
        result = xlir_test_observe_future_launch(CAPTURE_ownedPool("key", first, unused, false));
        assert(xrtMemDebugFailTriggered()); xrtMemDebugFailClear(); ++faults;
        assert(!result && drops == before + 2 && xrtGetError());
        assert(xrtErrorCode(xrtGetError()) != 104 && xrtErrorCode(xrtGetError()) != 105);
        clear_error(); raisingDrop = 0;
    }
    for (mode = 0; mode < 3; ++mode) {
        xfuture* result;
        before = drops; raisingDrop = mode != 0;
        result = xlir_test_observe_future_launch(CAPTURE_ownedPool("key", owned_capture_new(4), owned_capture_new(5), mode == 2));
        assert(result && xrtFutureWait(result) == 0);
        future_result(result, mode ? XFUTURE_FAILED : XFUTURE_RESOLVED, mode == 2 ? 81 : mode ? 105 : 0, -1);
        assert(drops == before + 2); raisingDrop = 0;
    }
    for (mode = 0; mode < 6; ++mode) {
        xfuture *input = NULL, *result; xpromise* promise = xrtPromiseCreate(&input, NULL);
        before = drops; assert(promise && input);
        result = xlir_test_observe_future_launch(CAPTURE_ownedMachine("key", owned_capture_new(4), owned_capture_new(5), input, mode == 1));
        assert(result && !xrtFutureDone(result) && drops == before && !xrtGetError()); ++suspensions;
        raisingDrop = mode == 1 || mode == 5;
        if (mode == 2) {
            xerror* error = xrtErrorCreate(XERR_STATE, "owned-capture", 82, "input failure");
            assert(error && xrtPromiseReject(promise, error)); xrtErrorFree(error);
        } else if (mode == 3) assert(xrtPromiseCancel(promise));
        else if (mode == 4) assert(xrtPromiseClose(promise));
        else assert(xrtPromiseResolve(promise, &payload));
        xrtPromiseDestroy(promise);
        future_result(result, mode == 3 ? XFUTURE_CANCELLED : mode == 4 ? XFUTURE_CLOSED :
            mode ? XFUTURE_FAILED : XFUTURE_RESOLVED, mode == 1 ? 81 : mode == 2 ? 82 : mode == 5 ? 105 : 0, 10);
        xrtFutureDestroy(input); assert(drops == before + 2 && !xrtGetError()); raisingDrop = 0;
    }
    /* The source call-site must also perform the transfer correctly. */
    before = drops; owned_capture_keep(); assert(!xrtGetError());
    restart_module(); assert(drops == before + 8);
    if (!smoke) {
        unsigned family, failIndex, destruct;
        /* Warm the lazy pool through a real accepted language activation. */
        future_result(xlir_test_observe_future_launch(CAPTURE_ownedPool("key", owned_capture_new(4), owned_capture_new(5), false)),
            XFUTURE_RESOLVED, 0, -1);
        for (family = 0; family < 4; ++family) for (failIndex = 0; failIndex < (family <= 1 ? 3u : 2u); ++failIndex)
        for (destruct = 0; destruct < 2; ++destruct) {
            xvalue *first = owned_capture_new(4), *unused = owned_capture_new(5);
            xfuture *input = NULL, *result = NULL; xpromise* promise = NULL; xlir_generator* generator = NULL;
            if (family == 3) { promise = xrtPromiseCreate(&input, NULL); assert(promise && input); }
            before = drops; raisingDrop = (int)destruct;
            assert(first && unused && xrtMemDebugFailAfter(failIndex));
            if (family == 0) generator = xlir_test_observe_generator_launch(CAPTURE_ownedGenerator("key", first, unused));
            else if (family == 1) result = xlir_test_observe_future_launch(CAPTURE_ownedReady(first, unused));
            else if (family == 2) result = xlir_test_observe_future_launch(CAPTURE_ownedPool("key", first, unused, false));
            else result = xlir_test_observe_future_launch(CAPTURE_ownedMachine("key", first, unused, input, false));
            assert(xrtMemDebugFailTriggered()); xrtMemDebugFailClear(); ++faults;
            assert(!generator && !result && drops == before + 2);
            assert(xrtGetError() && xrtErrorCode(xrtGetError()) != 104 && xrtErrorCode(xrtGetError()) != 105);
            clear_error(); raisingDrop = 0;
            if (promise) { xrtPromiseDestroy(promise); xrtFutureDestroy(input); }
            assert(!xrtGetError());
        }
    }
    restart_module();
}
unsigned owned_capture_faults(void) { return faults; }
unsigned owned_capture_suspensions(void) { return suspensions; }
