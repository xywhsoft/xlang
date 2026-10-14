#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdio.h>
static unsigned mode;
extern bool __real_xl6SequenceProjectionWitnessTakeV1(xvalue*,unsigned int,const xvalue*,bool,xvalue**);
bool __wrap_xl6SequenceProjectionWitnessTakeV1(xvalue* witness,unsigned int role,
    const xvalue* source,bool nullable,xvalue** output)
{
    assert(output && !*output && witness && source);
    if(!mode)return __real_xl6SequenceProjectionWitnessTakeV1(witness,role,source,nullable,output);
    xrtValueRelease(witness);
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
    puts("sequence projection status/null-diagnostic/cleanup balanced"); return 0;
}
