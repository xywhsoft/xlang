#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef XL6_NATIVE_ASYNC_FAULTS
static _Thread_local bool armed;static _Thread_local unsigned lane;static _Thread_local size_t seen,point;
extern bool __real_xl6SlotMapKeyCopyValueV1(XL6_SlotMapKeyV1,XL6_SlotMapKeyV1*);
bool __wrap_xl6SlotMapKeyCopyValueV1(XL6_SlotMapKeyV1 v,XL6_SlotMapKeyV1* out)
{return armed && lane==0 && seen++==point ? false : __real_xl6SlotMapKeyCopyValueV1(v,out);}
extern bool __real_xrtTypeCopyValue(const xrttype*,void*,const void*);
bool __wrap_xrtTypeCopyValue(const xrttype* type,void* out,const void* source)
{return armed && lane==1 && seen++==point ? false : __real_xrtTypeCopyValue(type,out,source);}
#endif
static void finish_module(void)
{
    /* Accepted async work may still be leaving after initialization failed.
     * The existing checked teardown keeps state 1/2 on busy; do not silently
     * reinitialize it or discard execution references. This fixture has only
     * bounded internal producers, never an unresolved external Future. */
    unsigned waits=0;
    for(;;) {
        xlir_module_fini();if(!xlir_module_initialized)return;
        assert((xlir_module_initialized==1 || xlir_module_initialized==2) && waits++<5000);
        xrtClearError();xrtSleep(1);
    }
}
int main(int argc,char** argv)
{
    xl6ProductMemoryBegin(SIZE_MAX);
    if(argc==1){for(unsigned i=0;i<200;++i){bool healthy=xlir_module_init()!=0;
        if(!healthy)fprintf(stderr,"native Future module failure: kind=%u message=%s\n",(unsigned)xrtErrorKind(xrtGetError()),xrtErrorMessage(xrtGetError()));
        assert(healthy);finish_module();assert(!xrtGetError() && !xlir_module_initialized);}}
    else{
#ifdef XL6_NATIVE_ASYNC_FAULTS
        assert(argc==3);point=(size_t)strtoull(argv[2],NULL,10);bool oom=!strcmp(argv[1],"oom");
        lane=!strcmp(argv[1],"copy")?0:1;assert(oom || !strcmp(argv[1],"copy") || !strcmp(argv[1],"typed-copy"));
        if(oom)assert(xrtMemDebugFailAfter(point));else armed=true;
        bool ok=xlir_module_init()!=0;bool hit=oom?xrtMemDebugFailTriggered():seen>point;
        armed=false;xrtMemDebugFailClear();assert(ok==!hit);xrtClearError();finish_module();xrtClearError();
        bool retry=xlir_module_init()!=0;
        if(!retry)fprintf(stderr,"native Future retry failure: lane=%s point=%zu kind=%u message=%s\n",argv[1],point,(unsigned)xrtErrorKind(xrtGetError()),xrtErrorMessage(xrtGetError()));
        assert(retry);finish_module();assert(!xrtGetError() && !xlir_module_initialized);
        printf("prefix=%s at=%zu hit=%u\n",argv[1],point,(unsigned)hit);
#else
        (void)argv;assert(false);
#endif
    }
    assert(!xl6ProductMemoryEnd());xl6ProductMemoryVerify();puts("[PASS] native Future result and checked acquisition");return 0;
}
