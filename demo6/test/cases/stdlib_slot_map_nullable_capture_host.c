#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static _Thread_local bool armed;
static _Thread_local size_t seen,point;
extern bool __real_xl6NullableSlotMapKeyCopyValueV1(XL6_NullableSlotMapKeyV1,XL6_NullableSlotMapKeyV1*);
bool __wrap_xl6NullableSlotMapKeyCopyValueV1(XL6_NullableSlotMapKeyV1 input,XL6_NullableSlotMapKeyV1* out)
{return armed && seen++==point ? false : __real_xl6NullableSlotMapKeyCopyValueV1(input,out);}
static bool execute(void){return xlir_module_init()!=0;}
static void finish_module(void)
{
    for(unsigned waits=0;;++waits){
        xlir_module_fini();if(!xlir_module_initialized)return;
        assert((xlir_module_initialized==1 || xlir_module_initialized==2) && waits<5000);
        xrtClearError();xrtSleep(1);
    }
}
int main(int argc,char** argv)
{
    xl6ProductMemoryBegin(SIZE_MAX);
    if(argc==1){for(unsigned i=0;i<100;++i){assert(execute());finish_module();assert(!xrtGetError());}}
    else{
        assert(argc==3);bool oom=!strcmp(argv[1],"oom");assert(oom || !strcmp(argv[1],"copy"));
        point=(size_t)strtoull(argv[2],NULL,10);seen=0;
        if(oom)assert(xrtMemDebugFailAfter(point));else armed=true;
        bool ok=execute(),hit=oom?xrtMemDebugFailTriggered():seen>point;
        armed=false;xrtMemDebugFailClear();assert(ok==!hit);
        xrtClearError();finish_module();xrtClearError();
        assert(execute());finish_module();assert(!xrtGetError());
        printf("lane=%s point=%zu hit=%u balanced\n",argv[1],point,(unsigned)hit);
    }
    assert(!xl6ProductMemoryEnd());xl6ProductMemoryVerify();return 0;
}
