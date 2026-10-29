#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef XLIR_NATIVE_SSA_SOURCE
#error "Pass actual verified generator C"
#endif
#include XLIR_NATIVE_SSA_SOURCE
static bool armed,early;
static unsigned lane;
static size_t seen,typedSeen,putSeen,point,entries,cleanups,resumes;
extern bool __real_xl6SlotMapKeyCopyValueV1(XL6_SlotMapKeyV1,XL6_SlotMapKeyV1*);
bool __wrap_xl6SlotMapKeyCopyValueV1(XL6_SlotMapKeyV1 input,XL6_SlotMapKeyV1* output)
{
    size_t at=seen++;
    if(armed && lane<2 && at==point){assert(!output->instance && !output->slot);if(lane==1)xrtSetErrorKind(XERR_RANGE);return false;}
    return __real_xl6SlotMapKeyCopyValueV1(input,output);
}
extern bool __real_xrtTypeCopyValue(const xrttype*,void*,const void*);
bool __wrap_xrtTypeCopyValue(const xrttype* type,void* output,const void* input)
{
    if(type==xl6SlotMapKeyTypeV1()){
        size_t at=typedSeen++;
        if(armed && (lane==2 || lane==3) && at==point){
            assert(__real_xrtTypeCopyValue(type,output,input));
            if(lane==3)xrtSetErrorKind(XERR_RANGE);
            return lane==3;
        }
    }
    return __real_xrtTypeCopyValue(type,output,input);
}
extern const XL6_GeneratorRuntimeV1* __real_xl6GeneratorRuntimeV1Get(void);
static bool put(XL6_Generator* generator,const void* item,size_t bytes)
{
    size_t at=putSeen++;
    if(armed && (lane==4 || lane==5) && at==point){if(lane==5)xrtSetErrorKind(XERR_RANGE);return false;}
    return __real_xl6GeneratorRuntimeV1Get()->Put(generator,item,bytes);
}
/* Explicit test-only call-site interposition. Every operation other than the
 * selected pre-publication Put refusal is the actual resident function. */
const XL6_GeneratorRuntimeV1* __wrap_xl6GeneratorRuntimeV1Get(void)
{
    static XL6_GeneratorRuntimeV1 api;api=*__real_xl6GeneratorRuntimeV1Get();api.Put=put;return &api;
}
void xlir_ssa_generator_entered(void){++entries;}
void xlir_ssa_generator_resumed(void){++resumes;}
bool xlir_ssa_generator_stop(void){return early;}
void xlir_ssa_generator_cleanup(void){++cleanups;}
static size_t success(void)
{
    seen=typedSeen=putSeen=entries=cleanups=resumes=0;
    assert(xlir_module_init());assert(entries==2 && cleanups==2 && resumes==(early?0:2) && putSeen==2 && !xrtGetError());
    size_t copies=seen;xlir_module_fini();assert(!xrtGetError() && !xlir_module_initialized);return copies;
}
int main(int argc,char** argv)
{
    xl6ProductMemoryBegin(SIZE_MAX);
    if(argc==1 || (argc==2 && !strcmp(argv[1],"early"))){
        early=argc==2;size_t copies=0;
        for(unsigned i=0;i<20;++i){size_t now=success();if(i)assert(copies==now);copies=now;}
        printf("lifetimes=20 early=%u native-copies=%zu\n",(unsigned)early,copies);
    }else{
        assert(argc==3);point=(size_t)strtoull(argv[2],NULL,10);bool oom=!strcmp(argv[1],"oom");
        const char* names[]={"copy","copy-error","partial-copy","true-error","put","put-error"};
        lane=6;for(unsigned i=0;i<6;++i)if(!strcmp(argv[1],names[i]))lane=i;assert(oom || lane<6);
        if(oom)assert(xrtMemDebugFailAfter(point));else armed=true;
        bool ok=xlir_module_init()!=0,hit=oom?xrtMemDebugFailTriggered():(lane<2?seen:lane<4?typedSeen:putSeen)>point;
        armed=false;xrtMemDebugFailClear();assert(ok==!hit && cleanups==entries);
        if(hit && !oom && (lane==1 || lane==3 || lane==5))assert(xrtGetError() && xrtErrorKind(xrtGetError())==XERR_RANGE);
        xrtClearError();xlir_module_fini();xrtClearError();success();
        printf("prefix=%s at=%zu hit=%u\n",argv[1],point,(unsigned)hit);
    }
    assert(!xl6ProductMemoryEnd());xl6ProductMemoryVerify();return 0;
}
