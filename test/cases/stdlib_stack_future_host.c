#include "test/support/module_product_memory.h"
#include <assert.h>
static void finish(void)
{
    for(unsigned i=0;;++i){xlir_module_fini();if(!xlir_module_initialized)return;
        assert((xlir_module_initialized==1 || xlir_module_initialized==2) && i<5000);xrtClearError();xrtSleep(1);}
}
int main(void)
{
    xl6ProductMemoryBegin(SIZE_MAX);
    for(unsigned i=0;i<100;++i){assert(xlir_module_init());finish();assert(!xrtGetError());}
    assert(!xl6ProductMemoryEnd());xl6ProductMemoryVerify();return 0;
}
