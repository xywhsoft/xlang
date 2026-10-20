/* Preinclude an actual generated module. Its finalizer releases globals;
 * source retirement is a separate checked host duty, including failed init.
 * Observe physical worker/TLS retirement before comparing allocator ledgers. */
#include <assert.h>
static void productExecutionFini(void)
{
    xlir_module_fini();
    assert(!xlir_module_initialized);
    assert(xl6ExecutionModuleRetireV1(&xlir_execution_source,true)==XL6_MODULE_RETIRE_READY);
}
#define xlir_module_fini productExecutionFini
#include "xlir_module_product_oom_host.c"
