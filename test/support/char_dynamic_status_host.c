#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdio.h>
static unsigned mode;
extern bool __real_xl6CharValueCheckedV1(const xvalue*,uint64_t,bool,uint32_t*);
bool __wrap_xl6CharValueCheckedV1(const xvalue* source,uint64_t identity,bool strict,uint32_t* output)
{
    if (!mode) return __real_xl6CharValueCheckedV1(source,identity,strict,output);
    *output=0; /* A valid payload cannot turn explicit failure into success. */
    if (mode==2) xrtSetErrorKind(XERR_TYPE);
    return false;
}
int main(void)
{
    for (mode=0;mode<3;++mode) {
        xl6ProductMemoryBegin(SIZE_MAX); int initialized=xlir_module_init();
        assert(!xl6ProductMemoryEnd()); xerror* failure=xrtTakeError();
        assert(mode ? !initialized : initialized);
        assert(mode==2 ? failure && xrtErrorKind(failure)==XERR_TYPE : !failure);
        xlir_module_fini(); assert(!xrtGetError() && !xlir_module_initialized);
        xlir_module_fini(); xrtErrorFree(failure); xrtClearError(); xl6ProductMemoryVerify();
    }
    puts("char status valid-zero/failed-zero/null-diagnostic/cleanup balanced"); return 0;
}
