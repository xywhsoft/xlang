/* Included after generated standalone C. Fail before publishing snapshots;
 * a checked NULL must fail even when a diagnostic could not be allocated. */
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef XL6_SLOT_MAP_TEST_FAULTS
static size_t point,seen;
static bool armed;
static unsigned lane;
static bool fail(unsigned kind) { return armed && lane==kind && seen++==point; }
extern xvalue* __real_xl6SlotMapKeysV1(const xvalue*,const XL6_SlotMapLayoutV1*,uint64_t);
xvalue* __wrap_xl6SlotMapKeysV1(const xvalue* v,const XL6_SlotMapLayoutV1* l,uint64_t identity)
{ return fail(0) ? NULL : __real_xl6SlotMapKeysV1(v,l,identity); }
extern xvalue* __real_xl6SlotMapValuesV1(const xvalue*,const XL6_SlotMapLayoutV1*,const xvalueconverter*);
xvalue* __wrap_xl6SlotMapValuesV1(const xvalue* v,const XL6_SlotMapLayoutV1* l,const xvalueconverter* converter)
{ return fail(1) ? NULL : __real_xl6SlotMapValuesV1(v,l,converter); }
extern bool __real_xl6SlotMapKeyUnboxV1(const xvalue*,XL6_SlotMapKeyV1*);
bool __wrap_xl6SlotMapKeyUnboxV1(const xvalue* v,XL6_SlotMapKeyV1* out)
{ return fail(2) ? false : __real_xl6SlotMapKeyUnboxV1(v,out); }
#endif
int main(int argc,char** argv)
{
    xl6ProductMemoryBegin(SIZE_MAX); assert(xlir_module_init());
    if(argc==1) {
        for(unsigned round=0;round<1000;++round) assert(xlang_main(NULL)==0 && !xrtGetError());
    } else {
#ifdef XL6_SLOT_MAP_TEST_FAULTS
        assert(argc==3); point=(size_t)strtoull(argv[2],NULL,10);
        bool oom=!strcmp(argv[1],"oom"); lane=!strcmp(argv[1],"keys") ? 0 : !strcmp(argv[1],"values") ? 1 : 2;
        assert(oom || !strcmp(argv[1],"keys") || !strcmp(argv[1],"values") || !strcmp(argv[1],"project"));
        if(oom) assert(xrtMemDebugFailAfter(point)); else armed=true;
        int code=-1; bool ok=xl_script_main_checked(NULL,&code)!=0;
        armed=false; bool hit=oom ? xrtMemDebugFailTriggered() : seen>point;
        xrtMemDebugFailClear(); assert(ok==!hit && code==(hit ? 1 : 0));
        if(!oom) assert(!xrtGetError());
        xrtClearError(); assert(xlang_main(NULL)==0 && !xrtGetError());
        printf("prefix=%s at=%zu hit=%u\n",argv[1],point,(unsigned)hit);
#else
        (void)argv; assert(false);
#endif
    }
    xlir_module_fini(); assert(!xrtGetError() && !xlir_module_initialized);
    assert(!xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
    puts("[PASS] public SlotMap independent typed snapshots"); return 0;
}
