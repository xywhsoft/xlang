#include <assert.h>
#include <string.h>
#include <windows.h>
#include "../../build/test/xlir_cell_continuation_outcome.c"
#include "../../build/test/xlir_cell_continuation_outcome.h.bind.h"
#define XLIR_TEST_LAUNCH_FUTURE
#define XLIR_TEST_LAUNCH_GENERATOR
#include "xlir_launch_test.h"
extern void continuation_owner_fault_arm(int);
extern unsigned continuation_owner_fault_end(int);
extern bool xrtPromiseResolve(xpromise*, void*), xrtPromiseReject(xpromise*, const xerror*);
extern bool xrtPromiseCancel(xpromise*), xrtPromiseClose(xpromise*);
extern int cell_continuation_dropped(int);
extern void cell_continuation_fault_finish(int64_t);

void cell_continuation_drop(int64_t id)
{
    int mode = cell_continuation_dropped((int)id);
    if (mode) {
        xl6_error_result failure = {false, mode == 1 ? xrtErrorCreate(XERR_STATE, "cell drop", 90 + (int)id, "cell destructor failed") : NULL};
        if (mode == 1) assert(failure.error);
        xl6ErrorScopePropagateFailure(failure);
    }
}

static void take_word(xlir_generator* generator, int index)
{
    static const xlir_string expected[] = {{"ax",2}, {"axx",3}, {"bx",2}, {"bxx",3}};
    xlir_string item = {0};
    assert(index >= 0 && index < 4);
    assert(xlir_generator_item_get(generator, &item, sizeof(item), true));
    assert(item.Data && item.Size == expected[index].Size &&
        !memcmp(item.Data, expected[index].Data, item.Size));
    xrtFree((void*)item.Data);
}
void cell_continuation_prepare(void) { assert(xlir_module_init() && !xrtGetError()); }
void cell_continuation_finish(void) { xlir_module_fini(); assert(!xrtGetError()); }
void cell_continuation_wait_idle(void)
{
    ULONGLONG deadline = GetTickCount64() + 10000;
    while (!xl6ContinuationExecutorIsIdle(xlir_continuation_executor)) {
        assert(GetTickCount64() < deadline);
        SwitchToThread();
    }
}
void cell_continuation_close(int stop, bool take)
{
    xlir_generator* generator = xlir_test_observe_generator_launch(CELL_CONTINUATION_WORDS()); int i;
    assert(generator && stop >= 0 && stop <= 5);
    for (i = 0; i < stop; ++i) {
        xlir_generator_step step = xlir_generator_next(generator);
        assert(step.ok && step.value == (i < 4));
        if (i < 4 && (take || i + 1 < stop)) take_word(generator, i);
    }
    xlir_generator_destroy(generator); assert(!xrtGetError());
}
unsigned cell_continuation_yield_fault(int steps, int fault)
{
    xlir_generator* generator = xlir_test_observe_generator_launch(CELL_CONTINUATION_WORDS()); xlir_generator_step step;
    unsigned allocations; int i;
    assert(generator && steps >= 0 && steps <= 4);
    for (i = 0; i < steps; ++i) {
        step = xlir_generator_next(generator); assert(step.ok && step.value); take_word(generator, i);
    }
    continuation_owner_fault_arm(fault);
    step = xlir_generator_next(generator); allocations = continuation_owner_fault_end(fault);
    if (fault >= 0) {
        assert(!step.ok && !step.value && xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_MEMORY);
    } else {
        assert(step.ok && step.value == (steps < 4) && !xrtGetError());
        if (step.value) take_word(generator, steps);
    }
    xlir_generator_destroy(generator); xrtClearError(); return allocations;
}
xfuture* cell_continuation_launch(xfuture* first, xfuture* second, bool managed)
{
    xl6_error_scope scope = {0}; xl6_error_result outcome; xfuture* result;
    assert(xl6ErrorScopeBegin(&scope));
    result = managed ? xlir_test_observe_future_launch(CELL_CONTINUATION_MANAGED(first, second)) : xlir_test_observe_future_launch(CELL_CONTINUATION_AWAIT(first, second));
    outcome = xl6ErrorScopeEnd(&scope);
    assert(result && outcome.ok && !outcome.error && !xrtGetError()); return result;
}
void cell_continuation_produce(xpromise* promise, int mode, int64_t* value)
{
    xl6_error_scope scope = {0}; xl6_error_result outcome;
    xerror* error = mode == 1 ? xrtErrorCreate(XERR_STATE, "cell input", 71, "input failed") : NULL;
    assert(mode != 1 || error);
    assert(xl6ErrorScopeBegin(&scope));
    assert(mode == 0 ? xrtPromiseResolve(promise, value) : mode == 1 ? xrtPromiseReject(promise, error)
        : mode == 2 ? xrtPromiseCancel(promise) : mode == 3 && xrtPromiseClose(promise));
    outcome = xl6ErrorScopeEnd(&scope);
    assert(outcome.ok && !outcome.error && !xrtGetError());
    if (error) xrtErrorFree(error);
}
