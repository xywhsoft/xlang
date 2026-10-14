#include "test/support/module_product_memory.h"
#include "src/runtime/xl6_dynamic_product.h"
#include <assert.h>
#include <stdio.h>
static unsigned mode;
extern bool __real_xl6DynamicDispatchProductTakeV1(xvalue*,XL6_DynamicOwnedDispatchV1,size_t,xvalue**,size_t,const xstrview*,xvalue**,size_t,xvalue***);
bool __wrap_xl6DynamicDispatchProductTakeV1(xvalue* receiver,XL6_DynamicOwnedDispatchV1 dispatch,size_t argc,xvalue** argv,size_t kwc,
    const xstrview* names,xvalue** keywords,size_t count,xvalue*** output)
{
    if(!mode)return __real_xl6DynamicDispatchProductTakeV1(receiver,dispatch,argc,argv,kwc,names,keywords,count,output);
    assert(!argc && !kwc && count==2 && !*output[0] && !*output[1]);
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
    puts("method product status valid-zero/failed-zero/null-diagnostic/cleanup balanced"); return 0;
}
