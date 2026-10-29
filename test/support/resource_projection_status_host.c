#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdio.h>
static unsigned mode;
extern bool __real_xl6ValueDomainProjectionWitnessTakeV1(xvalue*,const xvalue*,bool,xvalue**);
bool __wrap_xl6ValueDomainProjectionWitnessTakeV1(xvalue* witness,const xvalue* source,bool nullable,xvalue** output)
{
    assert(output && !*output && witness && source);
    if(!mode)return __real_xl6ValueDomainProjectionWitnessTakeV1(witness,source,nullable,output);
    xrtValueRelease(witness); if(mode==2)xrtSetErrorKind(XERR_TYPE); return false;
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
    puts("resource projection status/null-diagnostic/cleanup balanced"); return 0;
}
