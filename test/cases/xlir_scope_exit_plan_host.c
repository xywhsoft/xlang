#include <assert.h>
#include "../../build/test/xlir_scope_exit_plan.c"

void xlir_scope_prepare(void) { assert(xlir_module_init()); }
void xlir_scope_run(void)
{
    assert(xlir_scope_probe() == 41);
    assert(xlir_scope_observed == 1);
    assert(xrtGetError() == NULL);
}
void xlir_scope_finish(void) { xlir_module_fini(); }
