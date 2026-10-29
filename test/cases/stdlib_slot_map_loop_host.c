/* Include after generated C; checked failure must work without an allocated
 * diagnostic. Both a failed execution and its retry must retire all owners. */
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef XL6_SLOT_MAP_LOOP_FAULTS
static size_t point,seen;
static bool armed;
static unsigned lane;
static bool fail(unsigned kind) { return armed && lane==kind && seen++==point; }
extern XL6_ValueIterator* __real_xl6SlotMapPairIteratorCreateV1(const xvalue*,const XL6_SlotMapLayoutV1*,uint64_t,const xvalueconverter*);
XL6_ValueIterator* __wrap_xl6SlotMapPairIteratorCreateV1(const xvalue* v,const XL6_SlotMapLayoutV1* l,uint64_t k,const xvalueconverter* c)
{ return fail(0) ? NULL : __real_xl6SlotMapPairIteratorCreateV1(v,l,k,c); }
extern XL6_ValueIterator* __real_xl6SlotMapKeyIteratorCreateV1(const xvalue*,const XL6_SlotMapLayoutV1*,uint64_t);
XL6_ValueIterator* __wrap_xl6SlotMapKeyIteratorCreateV1(const xvalue* v,const XL6_SlotMapLayoutV1* l,uint64_t k)
{ return fail(1) ? NULL : __real_xl6SlotMapKeyIteratorCreateV1(v,l,k); }
extern XL6_ValueIterator* __real_xl6SlotMapItemIteratorCreateV1(const xvalue*,const XL6_SlotMapLayoutV1*,const xvalueconverter*);
XL6_ValueIterator* __wrap_xl6SlotMapItemIteratorCreateV1(const xvalue* v,const XL6_SlotMapLayoutV1* l,const xvalueconverter* c)
{ return fail(2) ? NULL : __real_xl6SlotMapItemIteratorCreateV1(v,l,c); }
extern XL6_ValueIterator* __real_xl6SlotMapCountIteratorCreateV1(const xvalue*,const XL6_SlotMapLayoutV1*);
XL6_ValueIterator* __wrap_xl6SlotMapCountIteratorCreateV1(const xvalue* v,const XL6_SlotMapLayoutV1* l)
{ return fail(3) ? NULL : __real_xl6SlotMapCountIteratorCreateV1(v,l); }
extern bool __real_xl6ValueIteratorKeyCopyV3(XL6_ValueIterator*,XL6_SlotMapKeyV1*);
bool __wrap_xl6ValueIteratorKeyCopyV3(XL6_ValueIterator* v,XL6_SlotMapKeyV1* out)
{ return fail(4) ? false : __real_xl6ValueIteratorKeyCopyV3(v,out); }
#endif
int main(int argc,char** argv)
{
    xl6ProductMemoryBegin(SIZE_MAX); assert(xlir_module_init());
    if(argc==1) {
        for(unsigned round=0;round<1000;++round) assert(xlang_main(NULL)==0 && !xrtGetError());
    } else {
#ifdef XL6_SLOT_MAP_LOOP_FAULTS
        assert(argc==3); point=(size_t)strtoull(argv[2],NULL,10);
        bool oom=!strcmp(argv[1],"oom");
        const char* names[]={"pairs","keys","values","count","key-copy"};
        lane=5; for(unsigned i=0;i<5;++i) if(!strcmp(argv[1],names[i])) lane=i;
        assert(oom || lane<5);
        if(oom) assert(xrtMemDebugFailAfter(point)); else armed=true;
        int code=-1; bool ok=xl_script_main_checked(NULL,&code)!=0;
        armed=false; bool hit=oom ? xrtMemDebugFailTriggered() : seen>point;
        xrtMemDebugFailClear(); assert(ok==!hit && code==(hit ? 1 : 0));
        xrtClearError(); assert(xlang_main(NULL)==0 && !xrtGetError());
        printf("prefix=%s at=%zu hit=%u\n",argv[1],point,(unsigned)hit);
#else
        (void)argv; assert(false);
#endif
    }
    xlir_module_fini(); assert(!xrtGetError() && !xlir_module_initialized);
    assert(!xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
    puts("[PASS] public SlotMap loops with checked retirement"); return 0;
}
