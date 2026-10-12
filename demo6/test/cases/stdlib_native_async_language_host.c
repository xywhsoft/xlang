#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef XL6_NATIVE_ASYNC_FAULTS
static bool armed;static unsigned lane;static size_t seen,point;
extern bool __real_xl6SlotMapKeyCopyValueV1(XL6_SlotMapKeyV1,XL6_SlotMapKeyV1*);
bool __wrap_xl6SlotMapKeyCopyValueV1(XL6_SlotMapKeyV1 v,XL6_SlotMapKeyV1* out)
{return armed && lane==0 && seen++==point ? false : __real_xl6SlotMapKeyCopyValueV1(v,out);}
extern bool __real_xl6ValueIteratorKeyCopyV3(XL6_ValueIterator*,XL6_SlotMapKeyV1*);
bool __wrap_xl6ValueIteratorKeyCopyV3(XL6_ValueIterator* it,XL6_SlotMapKeyV1* out)
{return armed && lane==1 && seen++==point ? false : __real_xl6ValueIteratorKeyCopyV3(it,out);}
#endif
int main(int argc,char** argv)
{
    xl6ProductMemoryBegin(SIZE_MAX);
    if(argc==1){for(unsigned i=0;i<200;++i){assert(xlir_module_init());xlir_module_fini();assert(!xrtGetError() && !xlir_module_initialized);}}
    else{
#ifdef XL6_NATIVE_ASYNC_FAULTS
        assert(argc==3);point=(size_t)strtoull(argv[2],NULL,10);bool oom=!strcmp(argv[1],"oom");
        lane=!strcmp(argv[1],"copy")?0:1;assert(oom || !strcmp(argv[1],"copy") || !strcmp(argv[1],"key-copy"));
        if(oom)assert(xrtMemDebugFailAfter(point));else armed=true;
        bool ok=xlir_module_init()!=0;bool hit=oom?xrtMemDebugFailTriggered():seen>point;
        armed=false;xrtMemDebugFailClear();assert(ok==!hit);xrtClearError();xlir_module_fini();xrtClearError();
        assert(xlir_module_init());xlir_module_fini();assert(!xrtGetError() && !xlir_module_initialized);
        printf("prefix=%s at=%zu hit=%u\n",argv[1],point,(unsigned)hit);
#else
        (void)argv;assert(false);
#endif
    }
    assert(!xl6ProductMemoryEnd());xl6ProductMemoryVerify();puts("[PASS] native key await and checked retirement");return 0;
}
