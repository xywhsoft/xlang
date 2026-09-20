#include <assert.h>
#include "../../build/test/xlir_async_result_commit.c"
#include "../../build/test/xlir_async_result_commit.h.bind.h"
#define XLIR_TEST_LAUNCH_FUTURE
#include "xlir_launch_test.h"
#define XLIR_ASYNC_SOURCE_HOST
#include "xlir_async_source_sync.h"
extern int result_commit_dropped(void);
extern bool xrtPromiseResolve(xpromise*, void*);
void result_commit_drop(void)
{
    int raising = result_commit_dropped();
    if (raising) {
        xl6_error_result result = {false, raising == 1 ? xrtErrorCreate(XERR_STATE, "result commit", 93, "rollback destructor failure") : NULL};
        if (raising == 1) assert(result.error);
        xl6ErrorScopePropagateFailure(result);
    }
}
void result_commit_prepare(void) { xlir_async_test_bind(); assert(xlir_module_init() && !xrtGetError()); }
void result_commit_finish(void) { xlir_async_test_fini(); }
xfuture* result_commit_launch(unsigned kind, xfuture* input, bool early, xerror* error)
{
    xlir_async_test_begin();
    switch (kind) {
    case 0: return xlir_test_observe_future_launch(RESULT_COMMIT_0(input, early));
    case 1: return xlir_test_observe_future_launch(RESULT_COMMIT_1(input, early));
    case 2: return xlir_test_observe_future_launch(RESULT_COMMIT_2(input, early));
    case 3: return xlir_test_observe_future_launch(RESULT_COMMIT_3(input, early));
    case 4: return xlir_test_observe_future_launch(RESULT_COMMIT_4(input, early, error));
    default: assert(0); return NULL;
    }
}
void result_commit_check(unsigned kind, xfuture* result, xfuture* input, xerror* error)
{
    void* value = xrtFutureValue(result); assert(value);
    if (kind == 0) assert(*(int64_t*)value == 17);
    else if (kind == 1) assert(*(char**)value && !strcmp(*(char**)value, "owned result"));
    else if (kind == 2) assert(*(xvalue**)value != NULL);
    else if (kind == 3) assert(*(xlir_closure**)value != NULL);
    else assert(*(xerror**)value == error);
    (void)input;
    assert(!xrtGetError());
}
void result_commit_complete(xpromise* promise, int64_t* value)
{
    xl6_error_scope scope = {0}; xl6_error_result result;
    assert(xl6ErrorScopeBegin(&scope) && xrtPromiseResolve(promise, value));
    result = xl6ErrorScopeEnd(&scope);
    assert(result.ok && !result.error && !xrtGetError());
}
