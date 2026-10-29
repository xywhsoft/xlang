#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdio.h>
#if !defined(XLIR_GENERATED_MODULE)
#error "XLIR_GENERATED_MODULE must name the actual generated language C"
#endif
#include XLIR_GENERATED_MODULE
int main(void)
{
    xl6ProductMemoryBegin(SIZE_MAX);
    assert(xl_script_module_init(NULL));
    xvalue* arguments=xrtValueArray(); assert(arguments);
    int result=xlang_main(arguments);
    xrtValueRelease(arguments);
    if (result || xrtGetError()) {
        fprintf(stderr,"native Tree language: result=%d diagnostic=%s\n",result,
            xrtGetError() ? xrtErrorMessage(xrtGetError()) : "none");
        return 1;
    }
    assert(xl_script_module_unit_checked() && !xrtGetError());
    assert(!xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
    puts("actual generated Tree language: init/main/fini lifetimes balanced");
    return 0;
}
