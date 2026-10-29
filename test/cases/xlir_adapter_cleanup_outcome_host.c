#include <assert.h>
#include "../../build/test/xlir_adapter_cleanup_outcome.c"
#include "../../build/test/xlir_adapter_cleanup_outcome.h.bind.h"
extern void adapter_open_event(int id);
extern int adapter_drop_event(int id);
extern xerror* adapter_test_error(int id);
void adapter_open(int64_t id) { adapter_open_event((int)id); }
void adapter_drop(int64_t id)
{
    int mode = adapter_drop_event((int)id);
    if (mode) {
        xl6_error_result failed = {false, mode == 1 ? adapter_test_error((int)id) : NULL};
        xl6ErrorScopePropagateFailure(failed);
    }
}
void adapter_primary(void)
{
    xl6_error_result failed = {false, adapter_test_error(0)};
    xl6ErrorScopePropagateFailure(failed);
}
int adapter_cleanup_init(void) { return xlir_module_init(); }
void adapter_cleanup_finish(void) { xlir_module_fini(); }
int adapter_cleanup_invoke(int shape, int* retained)
{
    int ok = 0; xvalue* one = NULL; xvalue* two = NULL;
    switch (shape) {
    case 0: { xlir_result_i64 r = ADAPTER_CLEANUP_0(NULL); ok = r.ok; if (ok) assert(r.value == 17); break; }
    case 1: { xlir_result_xvalue r = ADAPTER_CLEANUP_1(NULL); ok = r.ok; one = r.value; break; }
    case 2: { ADAPTER_CLEANUP_PRODUCT r = ADAPTER_CLEANUP_2(NULL); ok = r.ok; one = r.value.e0; two = r.value.e1; break; }
    case 3: { xlir_result_void r = ADAPTER_CLEANUP_3(NULL); ok = r.ok; break; }
    case 4: { xlir_result_xvalue r = ADAPTER_CLEANUP_4(NULL); ok = r.ok; one = r.value; break; }
    default: assert(0);
    }
    /* Inspect the adapter's own outcome, before a surrounding language call
     * could notice a stale diagnostic and accidentally hide a false success. */
    *retained = (one != NULL) + (two != NULL);
    if (one) xrtValueRelease(one);
    if (two) xrtValueRelease(two);
    return ok;
}
