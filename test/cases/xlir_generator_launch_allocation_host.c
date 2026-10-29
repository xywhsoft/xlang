#include <assert.h>
#include <stdio.h>
#include XLIR_LAUNCH_ALLOCATION_SOURCE
#ifndef XLIR_LAUNCH_EXPECT_BODY
#define XLIR_LAUNCH_EXPECT_BODY 1
#endif
static int starts, finals, catches;
void payload_fault_caught(void) { ++catches; }
void payload_fault_started(void) { ++starts; }
void payload_fault_finally(void) { ++finals; }
void payload_fault_prepare(void) { assert(xlir_module_init() && !xrtGetError()); }
void payload_fault_finish(void) { xlir_module_fini(); assert(!xrtGetError()); }
void payload_fault_ambient(void)
{
    xerror* original = xrtErrorCreate(XERR_STATE, "launch caller", 79, "pre-existing diagnostic");
    assert(original); xrtSetErrorTake(xrtErrorRef(original));
#if XLIR_LAUNCH_EXPECT_BODY
    xlir_result_generator result = XLIR_PAYLOAD_GENERATOR_LAUNCH();
#else
    xlir_result_future result = XLIR_PAYLOAD_ASYNC_LAUNCH();
#endif
    assert(result.ok && result.value && xrtGetError() == original);
    xrtErrorFree(xrtTakeError()); xrtErrorFree(original);
#if XLIR_LAUNCH_EXPECT_BODY
    xlir_generator_destroy(result.value);
#else
    xrtFutureDestroy(result.value);
#endif
    assert(!xrtGetError());
}
int payload_fault_run(int caught, int fault)
{
    xl6_error_scope scope = {0}; xl6_error_result outcome;
    int64_t result; int ok;
    starts = finals = catches = 0;
    assert(xl6ErrorScopeBegin(&scope)); result = payload_fault_case(caught != 0);
    outcome = xl6ErrorScopeEnd(&scope);
    ok = starts == finals && starts <= 1 && catches == ((caught && fault >= 0) ? 1 : 0);
    if (fault < 0) ok = ok && starts == XLIR_LAUNCH_EXPECT_BODY && outcome.ok && !outcome.error && result == 16;
    else if (caught) ok = ok && outcome.ok && !outcome.error && result == 77;
    else ok = ok && !outcome.ok && outcome.error && xrtErrorKind(outcome.error) == XERR_MEMORY;
    if (!ok) fprintf(stderr, "activation allocation failure caught=%d fault=%d starts=%d finals=%d result=%lld ok=%d errorKind=%d\n",
        caught, fault, starts, finals, (long long)result, outcome.ok, outcome.error ? (int)xrtErrorKind(outcome.error) : -1);
    xrtErrorFree(outcome.error); assert(!xrtGetError()); return ok;
}
