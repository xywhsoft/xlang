/* Actual generated main/module or top-level script lifecycle, with hooks. */
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdio.h>
#if !defined(XLIR_GENERATED_MODULE)
#error "An actual generated language module is required"
#endif
#include XLIR_GENERATED_MODULE
int main(void)
{
    xl6ProductMemoryBegin(SIZE_MAX);
#ifdef XLIR_SCRIPT_FRAGMENT
    assert(xlir_module_init());
    assert(!xrtGetError());
    assert(xlir_module_fini_body().ok);
#else
    assert(xl_script_module_init(NULL));
    xvalue* arguments=xrtValueArray(); assert(arguments);
    int result=xlang_main(arguments); xrtValueRelease(arguments);
    if(result || xrtGetError()) {
        fprintf(stderr,"semaphore language: result=%d error=%s\n",result,
            xrtGetError() ? xrtErrorMessage(xrtGetError()) : "none");
        return 1;
    }
    assert(xl_script_module_unit_checked());
#endif
    assert(!xrtGetError());
    assert(!xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
    puts("actual semaphore language: init/main/fini lifetimes balanced");
    return 0;
}
