#include "test/support/module_product_memory.h"
#include "src/runtime/xl6_float_convert.h"
#include <assert.h>
#include <stdio.h>
static unsigned mode;
extern bool __real_xl6Float32ValueConvertV1(const xvalue*,float*);
bool __wrap_xl6Float32ValueConvertV1(const xvalue* source,float* output)
{
    if(!mode)return __real_xl6Float32ValueConvertV1(source,output);
    assert(source && output);
    if(mode==2)xrtSetErrorKind(XERR_TYPE);
    return false;
}
int main(void)
{
    for(mode=0;mode<3;++mode) {
        xl6ProductMemoryBegin(SIZE_MAX); int initialized=xlir_module_init();
        assert(!xl6ProductMemoryEnd()); xerror* failure=xrtTakeError();
        assert(mode ? !initialized : initialized);
        assert(mode==2 ? failure && xrtErrorKind(failure)==XERR_TYPE : !failure);
        xlir_module_fini(); assert(!xrtGetError() && !xlir_module_initialized);
        xlir_module_fini(); xrtErrorFree(failure); xrtClearError(); xl6ProductMemoryVerify();
    }
    puts("float status valid-zero/failed-zero/null-diagnostic/cleanup balanced"); return 0;
}
