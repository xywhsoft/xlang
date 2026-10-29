/* Included after actual generated C; no unreported replacement providers. */
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    xl6ProductMemoryBegin(SIZE_MAX);
    assert(xlir_module_init());
    for (unsigned round=0;round<1000;++round)
        assert(xlang_main(NULL)==0 && !xrtGetError());
    xlir_module_fini();
    assert(!xrtGetError() && !xlir_module_initialized);
    assert(!xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
    puts("public container names: 1000 balanced lifetimes");
    return 0;
}
