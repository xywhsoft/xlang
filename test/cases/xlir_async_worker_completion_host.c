#include "release/include/xl_clock.h"
#include <assert.h>
#include <stddef.h>
#define XRT_DECLARATIONS 1
#include "src/common/xl6_xrt.h"
extern void* worker_completion_malloc(size_t);
// Use the real allocator after the worker-entry barrier. Fault cases arm XRT's
// thread-local allocation failure, never synthesize a NULL allocation result.
#define xrtMalloc worker_completion_malloc
#ifndef XLIR_WORKER_COMPLETION_SOURCE
#define XLIR_WORKER_COMPLETION_SOURCE "../../build/test/xlir_async_worker_completion.c"
#endif
#include XLIR_WORKER_COMPLETION_SOURCE
#undef xrtMalloc
#ifndef XLIR_WORKER_COMPLETION_BINDING
#define XLIR_WORKER_COMPLETION_BINDING "../../build/test/xlir_async_worker_completion.h.bind.h"
#endif
#include XLIR_WORKER_COMPLETION_BINDING
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
void worker_completion_drain_discarded(void)
{
    /* ONLY the no-observer cohort: producers are serialized and every body
     * has completed its tested lexical cleanup. Dropping a Future is not an
     * await or a terminal publication barrier. Wait for actual idle before
     * requesting module retirement; never retry a failed checked teardown. */
    XL6_DeadlineV1 deadline=xl6DeadlineAfterMillisecondsV1(15000);
    while(!xl6ContinuationExecutorIsIdle(xlir_continuation_executor)){
        assert(!xl6DeadlineExpiredV1(deadline));xrtThreadYield();
    }
}
void worker_completion_finish(void)
{
    xl6_error_scope scope = {0}; xl6_error_result outcome;
    assert(xl6ErrorScopeBegin(&scope)); xlir_module_fini(); outcome = xl6ErrorScopeEnd(&scope);
    if(!outcome.ok||outcome.error||xlir_module_initialized!=0||xrtGetError())
        fprintf(stderr,"queued worker fini: ok=%d kind=%d code=%d initialized=%d ambient=%d message=%s\n",
            (int)outcome.ok,(int)xrtErrorKind(outcome.error),xrtErrorCode(outcome.error),xlir_module_initialized,
            (int)xrtErrorKind(xrtGetError()),outcome.error?xrtErrorMessage(outcome.error):"none");
    /* A diagnostic-free failure is still a failure. Do not silently proceed
     * into a second cycle with an admission-closed, unretired module pool. */
    assert(outcome.ok && !outcome.error && xlir_module_initialized == 0 && !xrtGetError());
}
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
    xrtValueRelease(first); xrtValueRelease(unused);
    outcome = xl6ErrorScopeEnd(&scope);
    if (!outcome.ok || outcome.error || !result)
        fprintf(stderr, "worker launch kind=%u mode=%d ok=%u diagnostic=%d/%d result=%p message=%s\n",
            kind, mode, (unsigned)outcome.ok, outcome.error ? xrtErrorKind(outcome.error) : 0,
            outcome.error ? xrtErrorCode(outcome.error) : 0, (void*)result,
            outcome.error ? xrtErrorMessage(outcome.error) : "none");
    assert(outcome.ok && !outcome.error && result); return result;
}
void worker_completion_check(unsigned kind, xfuture* result)
{
    void* value = xrtFutureValue(result);
    if (kind == 1) { assert(value == NULL); return; }
    assert(value);
    if (kind == 0 || kind == 5) assert(*(int64_t*)value == 7);
    else if (kind == 2) {
        const xstrview* text=value;
        assert(text->Data && text->Size==13 && !memcmp(text->Data,"worker\0result",13) && !text->Data[text->Size]);
    }
    else if (kind == 3) assert(*(xvalue**)value);
    else if (kind == 4) {
        xlir_closure* closure = *(xlir_closure**)value; int64_t number = 0;
        assert(closure && ((bool (*)(void*, int64_t*))closure->invoke)(closure->environment, &number) && number == 17);
    }
}
