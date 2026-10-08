#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdio.h>
/* Preinclude the real generated no-main module. Its private initialization
 * boundary is not a public process-entry API and need not declare XRT errors. */
int main(void)
{
    xl6ProductMemoryBegin(SIZE_MAX);
    assert(xlir_module_init()); xlir_module_fini();
    assert(!xlir_module_initialized && !xl6ProductMemoryEnd());
    xl6ProductMemoryVerify(); puts("no-main module balanced"); return 0;
}
