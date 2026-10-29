/* Included after generated standalone C. Native checked outputs stay empty on
 * failure; faults below occur before a publish, never forge a broken callee. */
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef XL6_SLOT_MAP_TEST_FAULTS
static size_t point,seen;
static bool armed;
static unsigned lane;
static bool fail(unsigned kind)
{ return armed && lane==kind && seen++==point; }
extern bool __real_xl6SlotMapInsertV1(const xvalue*,const XL6_SlotMapLayoutV1*,const void*,XL6_SlotMapKeyV1*);
bool __wrap_xl6SlotMapInsertV1(const xvalue* v,const XL6_SlotMapLayoutV1* l,const void* input,XL6_SlotMapKeyV1* out)
{ return fail(0) ? false : __real_xl6SlotMapInsertV1(v,l,input,out); }
extern bool __real_xl6SlotMapReadRequiredV1(const xvalue*,const XL6_SlotMapLayoutV1*,XL6_SlotMapKeyV1,void*);
bool __wrap_xl6SlotMapReadRequiredV1(const xvalue* v,const XL6_SlotMapLayoutV1* l,XL6_SlotMapKeyV1 key,void* out)
{ return fail(1) ? false : __real_xl6SlotMapReadRequiredV1(v,l,key,out); }
extern bool __real_xl6SlotMapSizeV1(const xvalue*,const XL6_SlotMapLayoutV1*,bool,int64_t*);
bool __wrap_xl6SlotMapSizeV1(const xvalue* v,const XL6_SlotMapLayoutV1* l,bool cap,int64_t* out)
{ return fail(2) ? false : __real_xl6SlotMapSizeV1(v,l,cap,out); }
#endif
int main(int argc,char** argv)
{
    xl6ProductMemoryBegin(SIZE_MAX); assert(xlir_module_init());
    if(argc==1) {
        for(unsigned round=0;round<1000;++round) assert(xlang_main(NULL)==0 && !xrtGetError());
    } else {
#ifdef XL6_SLOT_MAP_TEST_FAULTS
        assert(argc==3); point=(size_t)strtoull(argv[2],NULL,10);
        bool oom=!strcmp(argv[1],"oom");
        lane=!strcmp(argv[1],"insert") ? 0 : !strcmp(argv[1],"get") ? 1 : 2;
        assert(oom || !strcmp(argv[1],"insert") || !strcmp(argv[1],"get") || !strcmp(argv[1],"size"));
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
    puts("[PASS] public SlotMap typed slots, 1000 source lifetimes"); return 0;
}
