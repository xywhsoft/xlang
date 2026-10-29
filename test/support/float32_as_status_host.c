#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdio.h>
static unsigned mode;
extern bool __real_xl6ValueReadF32V1(const xvalue*,uint64_t,float*);
bool __wrap_xl6ValueReadF32V1(const xvalue* source,uint64_t identity,float* output)
{
    assert(source && identity && output && *output==0);
    if(!mode)return __real_xl6ValueReadF32V1(source,identity,output);
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
    puts("F32 projection status/null-diagnostic/cleanup balanced"); return 0;
}
