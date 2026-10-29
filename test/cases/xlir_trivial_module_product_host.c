/* Preinclude a minimal generated program with no language error dependency.
 * The independent memory-debug translation unit does not enlarge its ABI. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "test/support/module_product_memory.h"

int main(void)
{
    xl6ProductMemoryBegin(SIZE_MAX);
    assert(xlir_module_init());
    assert(!xl6ProductMemoryEnd());
    xlir_module_fini();
    assert(!xlir_module_initialized);
    xlir_module_fini();
    xl6ProductMemoryVerify();
    puts("trivial module product balanced");
    return 0;
}
