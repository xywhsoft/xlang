#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdio.h>
#ifndef XLIR_GENERATED_MODULE
#error An actual emitted module is required
#endif
#define main xl6GeneratedMain
#include XLIR_GENERATED_MODULE
#undef main
int main(void)
{
    xl6ProductMemoryBegin(SIZE_MAX);
    /* Exercise the actual emitted executable entry, including its static
     * ownership catalog and cyclic module retirement, not a shortcut init. */
    assert(xl6GeneratedMain(0,NULL)==0 && !xrtGetError());
    assert(!xl6ProductMemoryEnd());xl6ProductMemoryVerify();
    puts("actual codec/compress/template language: init/main/fini lifetimes balanced");
    return 0;
}
