#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    xl6ProductMemoryBegin(SIZE_MAX);
    for(unsigned iteration=0;iteration<8;++iteration) {
        assert(xlir_module_init());
        int code=-1;
        assert(xl_script_main_checked(NULL,&code) && code==0);
        xlir_module_fini();
        assert(!xlir_module_initialized && !xrtGetError());
    }
    assert(!xl6ProductMemoryEnd());
    xl6ProductMemoryVerify();
    puts("[PASS] concurrent public queue, unique delivery and module cleanup");
    return 0;
}
