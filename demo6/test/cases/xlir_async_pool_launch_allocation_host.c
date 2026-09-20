#include <assert.h>
#include <stdio.h>
#ifndef XLIR_GENERATOR_STORAGE_SOURCE
#define XLIR_GENERATOR_STORAGE_SOURCE "../../build/test/xlir_async_pool_launch_allocation.c"
#endif
#include XLIR_GENERATOR_STORAGE_SOURCE
static int starts, finals, catches;
void payload_fault_started(void) { ++starts; }
void payload_fault_finally(void) { ++finals; }
void payload_fault_caught(void) { ++catches; }
void payload_pool_prepare(void)
{
    starts = finals = catches = 0;
    assert(xlir_module_init() && !xrtGetError());
}
void payload_pool_invoke(int caught, int fault)
{
    xl6_error_scope scope = {0}; xl6_error_result outcome; int64_t result;
    assert(xl6ErrorScopeBegin(&scope)); result = payload_fault_case(caught != 0);
    outcome = xl6ErrorScopeEnd(&scope);
    assert(catches == ((caught && fault >= 0) ? 1 : 0));
    if (fault < 0) assert(outcome.ok && !outcome.error && result == 16);
    else if (caught) assert(outcome.ok && !outcome.error && result == 77);
    else assert(!outcome.ok && outcome.error && xrtErrorKind(outcome.error) == XERR_MEMORY);
    xrtErrorFree(outcome.error); assert(!xrtGetError());
}
void payload_pool_finish(int fault)
{
    /* Module quiescence joins workers before inspecting their counters.
     * Pool allocations belong to this lifetime, not to one exported call. */
    xlir_module_fini(); assert(!xrtGetError());
    assert(starts == finals && starts <= 1 && (fault >= 0 || starts == 1));
}
