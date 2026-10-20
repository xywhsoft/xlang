#include <assert.h>
#ifndef OWNER_CALLS_GENERATED_C
#define OWNER_CALLS_GENERATED_C "../../build/test/xlir_continuation_owner_calls.c"
#endif
#ifndef OWNER_CALLS_BINDING_H
#define OWNER_CALLS_BINDING_H "../../build/test/xlir_continuation_owner_calls.h.bind.h"
#endif
#include OWNER_CALLS_GENERATED_C
#include OWNER_CALLS_BINDING_H
#define XLIR_TEST_LAUNCH_FUTURE
#define XLIR_TEST_LAUNCH_GENERATOR
#include "xlir_launch_test.h"
#define XLIR_ASYNC_SOURCE_HOST
#include "xlir_async_source_sync.h"
void owner_calls_prepare(void){xlir_async_test_bind();assert(xlir_module_init()&&!xrtGetError());}
void owner_calls_finish(void){xlir_async_test_fini();}
extern void continuation_owner_fault_arm(int);
extern unsigned continuation_owner_fault_end(int);
unsigned owner_calls_generator_fault(int fault)
{
    xlir_generator* generator = xlir_test_observe_generator_launch(OWNER_CALLS_GENERATOR(xlir_literal_view("left"), xlir_literal_view("right"), false));
    xlir_generator_step step; unsigned allocations;
    assert(generator); continuation_owner_fault_arm(fault);
    step = xlir_generator_next(generator); allocations = continuation_owner_fault_end(fault);
    if (fault >= 0) assert(!step.ok && !step.value && xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_MEMORY);
    else assert(step.ok && step.value && !xrtGetError());
    xlir_generator_destroy(generator); xrtClearError();
    return allocations;
}
xfuture* owner_calls_alias(xfuture* input, xfuture* second, bool first) { xlir_async_test_begin();return xlir_test_observe_future_launch(OWNER_CALLS_ALIAS(xlir_literal_view("owner"), input, second, first)); }
xfuture* owner_calls_pair(xfuture* input, bool fail) { xlir_async_test_begin();return xlir_test_observe_future_launch(OWNER_CALLS_PAIR(xlir_literal_view("left"), xlir_literal_view("right"), input, fail)); }
extern bool xrtPromiseResolve(xpromise*, void*);
void owner_calls_complete_checked(xpromise* promise, unsigned mode, int64_t* value, xerror* error)
{
    xl6_error_scope scope = {0}; xl6_error_result result;
    assert(xl6ErrorScopeBegin(&scope));
    if (mode == 0) assert(xrtPromiseResolve(promise, value));
    else if (mode == 1) assert(xrtPromiseReject(promise, error));
    else if (mode == 2) assert(xrtPromiseCancel(promise));
    else assert(xrtPromiseClose(promise));
    result = xl6ErrorScopeEnd(&scope);
    assert(result.ok && result.error == NULL && !xrtGetError());
}
void owner_calls_generator(bool fail, int stop)
{
    xlir_generator* generator = xlir_test_observe_generator_launch(OWNER_CALLS_GENERATOR(xlir_literal_view("left"), xlir_literal_view("right"), fail));
    int count = 0, failed = 0; assert(generator);
    for (;;) {
        xlir_generator_step step = xlir_generator_next(generator); xlir_string item = {0};
        if (!step.ok) {
            assert(fail && stop != 1 && count == 1 && xrtGetError() && xrtErrorCode(xrtGetError()) == 93);
            failed = 1; break;
        }
        if (!step.value) break;
        assert(xlir_generator_item_get(generator, &item, sizeof(item), true));
        assert(item.Data && item.Size==(count?4u:5u) && !memcmp(item.Data,count?"done":"pause",item.Size));
        xlir_string_drop(item);
        if (++count == stop) break;
    }
    assert(failed == (fail && stop != 1));
    assert(count == (stop == 1 || failed ? 1 : 2));
    xlir_generator_destroy(generator); xrtClearError();
}
