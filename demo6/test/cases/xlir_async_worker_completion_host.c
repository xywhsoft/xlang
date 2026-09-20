#include <assert.h>
#include <stddef.h>
extern void* worker_completion_malloc(size_t);
// Use the real allocator after the worker-entry barrier. Fault cases arm XRT's
// thread-local allocation failure, never synthesize a NULL allocation result.
#define xrtMalloc worker_completion_malloc
#include "../../build/test/xlir_async_worker_completion.c"
#undef xrtMalloc
#include "../../build/test/xlir_async_worker_completion.h.bind.h"
#define XLIR_TEST_LAUNCH_FUTURE
#include "xlir_launch_test.h"
extern int worker_completion_enter_body(void), worker_completion_drop_mode(int64_t);
extern void* xrtFutureValue(const xfuture*);
void worker_completion_body(bool fail)
{
    int mode = worker_completion_enter_body();
    assert(fail == (mode != 0));
    if (mode) {
        xl6_error_result error = {false, mode == 1 ? xrtErrorCreate(XERR_STATE, "worker-completion", 31, "body failure") : NULL};
        if (mode == 1) assert(error.error);
        xl6ErrorScopePropagateFailure(error);
    }
}
void worker_completion_drop(int64_t id)
{
    int mode = worker_completion_drop_mode(id);
    if (mode) {
        xl6_error_result error = {false, mode == 1 ? xrtErrorCreate(XERR_STATE, "worker-completion", (int)(90 + id), "drop failure") : NULL};
        if (mode == 1) assert(error.error);
        xl6ErrorScopePropagateFailure(error);
    }
}
void worker_completion_prepare(void) { assert(xlir_module_init() && !xrtGetError()); }
void worker_completion_finish(void) { xlir_module_fini(); assert(!xrtGetError()); }
xfuture* worker_completion_launch(unsigned kind, int mode)
{
    xl6_error_scope scope = {0}; xl6_error_result outcome;
    xvalue *first, *unused; xfuture* result = NULL;
    assert(xl6ErrorScopeBegin(&scope));
    first = worker_completion_new(7); unused = worker_completion_new(9); assert(first && unused);
    switch (kind) {
    case 0: result = xlir_test_observe_future_launch(WORKER_COMPLETION_0(first, unused, mode != 0)); break;
    case 1: result = xlir_test_observe_future_launch(WORKER_COMPLETION_1(first, unused, mode != 0)); break;
    case 2: result = xlir_test_observe_future_launch(WORKER_COMPLETION_2(first, unused, mode != 0)); break;
    case 3: result = xlir_test_observe_future_launch(WORKER_COMPLETION_3(first, unused, mode != 0)); break;
    case 4: result = xlir_test_observe_future_launch(WORKER_COMPLETION_4(first, unused, mode != 0)); break;
    case 5: result = xlir_test_observe_future_launch(WORKER_COMPLETION_5(first, unused, mode != 0)); break;
    default: assert(0);
    }
    // The worker cannot pass its entry barrier before these caller references
    // are gone. The generated frame/worker, not the test, is the final owner.
    xrtValueRelease(first); if (kind != 5) xrtValueRelease(unused);
    outcome = xl6ErrorScopeEnd(&scope); assert(outcome.ok && !outcome.error && result); return result;
}
void worker_completion_check(unsigned kind, xfuture* result)
{
    void* value = xrtFutureValue(result);
    if (kind == 1) { assert(value == NULL); return; }
    assert(value);
    if (kind == 0 || kind == 5) assert(*(int64_t*)value == 7);
    else if (kind == 2) assert(*(char**)value && !strcmp(*(char**)value, "worker-result"));
    else if (kind == 3) assert(*(xvalue**)value);
    else if (kind == 4) {
        xlir_closure* closure = *(xlir_closure**)value; int64_t number = 0;
        assert(closure && ((bool (*)(void*, int64_t*))closure->invoke)(closure->environment, &number) && number == 17);
    }
}
