#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef XL6_FUTURE_ITERATION_FAULTS
static _Thread_local bool armed;
static _Thread_local unsigned lane;
static _Thread_local size_t seen,point;
static bool fail(unsigned candidate){return armed && lane==candidate && seen++==point;}
extern XL6_ValueIterator* __real_xl6SlotMapPairIteratorCreateV1(const xvalue*,const XL6_SlotMapLayoutV1*,uint64_t,const xvalueconverter*);
XL6_ValueIterator* __wrap_xl6SlotMapPairIteratorCreateV1(const xvalue* v,const XL6_SlotMapLayoutV1* l,uint64_t k,const xvalueconverter* c)
{return fail(0)?NULL:__real_xl6SlotMapPairIteratorCreateV1(v,l,k,c);}
extern XL6_ValueIterator* __real_xl6SlotMapKeyIteratorCreateV1(const xvalue*,const XL6_SlotMapLayoutV1*,uint64_t);
XL6_ValueIterator* __wrap_xl6SlotMapKeyIteratorCreateV1(const xvalue* v,const XL6_SlotMapLayoutV1* l,uint64_t k)
{return fail(1)?NULL:__real_xl6SlotMapKeyIteratorCreateV1(v,l,k);}
extern XL6_ValueIterator* __real_xl6SlotMapItemIteratorCreateV1(const xvalue*,const XL6_SlotMapLayoutV1*,const xvalueconverter*);
XL6_ValueIterator* __wrap_xl6SlotMapItemIteratorCreateV1(const xvalue* v,const XL6_SlotMapLayoutV1* l,const xvalueconverter* c)
{return fail(2)?NULL:__real_xl6SlotMapItemIteratorCreateV1(v,l,c);}
extern XL6_ValueIterator* __real_xl6SlotMapCountIteratorCreateV1(const xvalue*,const XL6_SlotMapLayoutV1*);
XL6_ValueIterator* __wrap_xl6SlotMapCountIteratorCreateV1(const xvalue* v,const XL6_SlotMapLayoutV1* l)
{return fail(3)?NULL:__real_xl6SlotMapCountIteratorCreateV1(v,l);}
extern const XL6_ValueIteratorRuntimeV2* __real_xl6ValueIteratorRuntimeV2Get(void);
static XL6_ValueIterator* create(const xvalue* source){return __real_xl6ValueIteratorRuntimeV2Get()->Create(source);}
static bool advance(XL6_ValueIterator* v,bool* found)
{if(fail(4)){if(found)*found=false;return false;}return __real_xl6ValueIteratorRuntimeV2Get()->Advance(v,found);}
static const XL6_ValueIteratorView* view(const XL6_ValueIterator* v){return __real_xl6ValueIteratorRuntimeV2Get()->View(v);}
static void destroy(XL6_ValueIterator* v){__real_xl6ValueIteratorRuntimeV2Get()->Destroy(v);}
static xrtownershipref ownership(const XL6_ValueIterator* v){return __real_xl6ValueIteratorRuntimeV2Get()->Ownership(v);}
const XL6_ValueIteratorRuntimeV2* __wrap_xl6ValueIteratorRuntimeV2Get(void)
{static const XL6_ValueIteratorRuntimeV2 api={create,advance,view,destroy,ownership};return &api;}
extern bool __real_xl6ValueIteratorKeyCopyV3(XL6_ValueIterator*,XL6_SlotMapKeyV1*);
bool __wrap_xl6ValueIteratorKeyCopyV3(XL6_ValueIterator* v,XL6_SlotMapKeyV1* key)
{return fail(5)?false:__real_xl6ValueIteratorKeyCopyV3(v,key);}
extern bool __real_xrtTypeCopyValue(const xrttype*,void*,const void*);
bool __wrap_xrtTypeCopyValue(const xrttype* t,void* out,const void* source)
{return fail(6)?false:__real_xrtTypeCopyValue(t,out,source);}
#endif
static bool execute(void)
{
    if(!xlir_module_init())return false;
#ifdef XL6_FUTURE_ITERATION_HAS_MAIN
    int code=-1;return xl_script_main_checked(NULL,&code) && code==0;
#else
    return true;
#endif
}
static void finish_module(void)
{
    unsigned retries=0;
    for(;;){
        xlir_module_fini();if(!xlir_module_initialized)return;
        assert((xlir_module_initialized==1 || xlir_module_initialized==2) && retries++<5000);
        xrtClearError();xrtSleep(1);
    }
}
int main(int argc,char** argv)
{
    xl6ProductMemoryBegin(SIZE_MAX);
    if(argc==1){
        for(unsigned i=0;i<200;++i){assert(execute());finish_module();assert(!xrtGetError());}
    }else{
#ifdef XL6_FUTURE_ITERATION_FAULTS
        const char* names[]={"pairs","keys","values","count","advance","key-copy","typed-copy"};
        assert(argc==3);point=(size_t)strtoull(argv[2],NULL,10);lane=7;
        for(unsigned i=0;i<7;++i)if(!strcmp(argv[1],names[i]))lane=i;
        bool oom=!strcmp(argv[1],"oom");assert(oom || lane<7);
        if(oom)assert(xrtMemDebugFailAfter(point));else armed=true;
        bool ok=execute();bool hit=oom?xrtMemDebugFailTriggered():seen>point;
        armed=false;xrtMemDebugFailClear();assert(ok==!hit);
        xrtClearError();finish_module();xrtClearError();assert(execute());finish_module();assert(!xrtGetError());
        printf("prefix=%s at=%zu hit=%u\n",argv[1],point,(unsigned)hit);
#else
        (void)argv;assert(false);
#endif
    }
    assert(!xlir_module_initialized && !xl6ProductMemoryEnd());xl6ProductMemoryVerify();
    puts("[PASS] Future element iteration, exact projections and owned cleanup");return 0;
}
