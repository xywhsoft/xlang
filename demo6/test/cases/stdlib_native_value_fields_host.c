#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef XL6_NATIVE_FIELD_FAULTS
static _Thread_local bool armed;
static _Thread_local unsigned lane,flavor;
static _Thread_local size_t point,seen;
static bool fail(unsigned candidate){return armed && lane==candidate && seen++==point;}
extern bool __real_xl6SlotMapKeyUnboxV1(const xvalue*,XL6_SlotMapKeyV1*);
bool __wrap_xl6SlotMapKeyUnboxV1(const xvalue* source,XL6_SlotMapKeyV1* out)
{return fail(0)?false:__real_xl6SlotMapKeyUnboxV1(source,out);}
extern bool __real_xrtTypeCopyValue(const xrttype*,void*,const void*);
bool __wrap_xrtTypeCopyValue(const xrttype* type,void* out,const void* source)
{
    if(!fail(1))return __real_xrtTypeCopyValue(type,out,source);
    if(!flavor)return false;
    bool copied=__real_xrtTypeCopyValue(type,out,source);
    if(!copied)return false;
    if(flavor==2){xrtSetErrorKind(XERR_MEMORY);return true;}
    return false;
}
#endif
static bool execute(void)
{
    if(!xlir_module_init())return false;
#ifdef XL6_NATIVE_FIELD_HAS_MAIN
    int code=-1;return xl_script_main_checked(NULL,&code) && code==0;
#else
    return true;
#endif
}
static void finish_module(void)
{
    unsigned retries=0;
    for(;;){xlir_module_fini();if(!xlir_module_initialized)return;
        assert((xlir_module_initialized==1 || xlir_module_initialized==2) && retries++<5000);
        xrtClearError();xrtSleep(1);
    }
}
int main(int argc,char** argv)
{
    xl6ProductMemoryBegin(SIZE_MAX);
    if(argc==1){for(unsigned i=0;i<200;++i){assert(execute());finish_module();assert(!xrtGetError());}}
    else{
#ifdef XL6_NATIVE_FIELD_FAULTS
        assert(argc==3);point=(size_t)strtoull(argv[2],NULL,10);bool oom=!strcmp(argv[1],"oom");
        lane=!strcmp(argv[1],"project")?0:1;
        flavor=!strcmp(argv[1],"typed-partial")?1:!strcmp(argv[1],"typed-diagnostic")?2:0;
        assert(oom || !strcmp(argv[1],"project") || !strcmp(argv[1],"typed-copy") || flavor);
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
    puts("[PASS] exact native field ownership, temporary receivers and cleanup");return 0;
}
