#include <assert.h>
#include <windows.h>
// Observe the resident boundary without replacing its physical cursor. Arm
// the real allocator only after the real resident/XRT advance has completed.
#include "src/runtime/xl6_value_iterator.h"
const XL6_ValueIteratorRuntimeV2* iteration_projection_iterator_runtime(void);
XL6_ValueIterator* iteration_projection_collection_create(const struct xvalue*);
#define xl6ValueIteratorRuntimeV2Get iteration_projection_iterator_runtime
#define xl6CollectionValueIteratorCreateV1 iteration_projection_collection_create
#include "xlir_dynamic_iteration_allocation.c"
#undef xl6ValueIteratorRuntimeV2Get
#undef xl6CollectionValueIteratorCreateV1
#include "xlir_dynamic_iteration_allocation.h.bind.h"
#define XLIR_TEST_LAUNCH_FUTURE
#define XLIR_TEST_LAUNCH_GENERATOR
#include "xlir_launch_test.h"
extern int iteration_projection_drop_mode(int);
extern bool xrtPromiseResolve(xpromise*, void*);
void iteration_projection_drop(int64_t id)
{
    int mode = iteration_projection_drop_mode((int)id);
    if (mode) {
        xl6_error_result failure = {false, mode == 1 ? xrtErrorCreate(XERR_STATE, "iteration projection", 90 + (int)id, "cursor item destructor failed") : NULL};
        if (mode == 1) assert(failure.error);
        xl6ErrorScopePropagateFailure(failure);
    }
}
void iteration_projection_prepare(void) { assert(xlir_module_init() && !xrtGetError()); }
void iteration_projection_finish(void) { xlir_module_fini(); assert(!xrtGetError()); }
bool iteration_projection_idle(void) { return xl6ContinuationExecutorIsIdle(xlir_continuation_executor); }
void iteration_projection_wait_idle(void)
{
    /* A terminal Future guarantees semantic retirement, not the executor's
     * final physical reference/return tail. No producer starts another case
     * until this real execution credit has been returned. */
    ULONGLONG deadline=GetTickCount64()+10000;
    while(!xl6ContinuationExecutorIsIdle(xlir_continuation_executor)){
        assert(GetTickCount64()<deadline);SwitchToThread();
    }
}
xfuture* iteration_projection_launch(int kind, xfuture* input)
{
    xl6_error_scope scope = {0}; xl6_error_result outcome; xfuture* result;
    assert(xl6ErrorScopeBegin(&scope)); result = xlir_test_observe_future_launch(ITERATION_PROJECTION_AWAIT(kind, input));
    outcome = xl6ErrorScopeEnd(&scope);
    assert(outcome.ok && !outcome.error && result && !xrtGetError()); return result;
}
void iteration_projection_produce(xpromise* promise, int mode, int64_t* value)
{
    xl6_error_scope scope = {0}; xl6_error_result outcome;
    xerror* original = mode == 1 ? xrtErrorCreate(XERR_STATE, "iteration producer", 31, "pre-existing producer failure") : NULL;
    assert(mode != 1 || original); assert(xl6ErrorScopeBegin(&scope));
    if (mode) { xl6_error_result failure = {false, original}; xl6ErrorScopePropagateFailure(failure); }
    assert(xrtPromiseResolve(promise, value)); outcome = xl6ErrorScopeEnd(&scope);
    assert(outcome.ok == (mode == 0) && outcome.error == original && !xrtGetError());
    if (outcome.error) xrtErrorFree(outcome.error);
}
void* iteration_projection_generator(int kind) { return xlir_test_observe_generator_launch(ITERATION_PROJECTION_YIELD(kind)); }
int iteration_projection_next(void* handle, int64_t* value)
{
    xlir_generator_step step = xlir_generator_next((xlir_generator*)handle);
    assert(step.ok && !xrtGetError());
    if (step.value) assert(xlir_generator_item_get((xlir_generator*)handle, value, sizeof(*value), false));
    return step.value;
}
void iteration_projection_close(void* handle)
{ xlir_generator_destroy((xlir_generator*)handle); assert(!xrtGetError()); }
