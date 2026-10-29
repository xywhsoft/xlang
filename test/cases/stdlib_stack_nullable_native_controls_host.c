#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
static bool execute(void)
{int code=-1;return xlir_module_init() && xl_script_main_checked(NULL,&code) && code==0;}
int main(int argc,char** argv)
{
    assert(argc==1 || argc==2);xl6ProductMemoryBegin(SIZE_MAX);
    if(argc==1){for(unsigned i=0;i<100;++i){assert(execute());xlir_module_fini();assert(!xlir_module_initialized && !xrtGetError());}}
    else{
        size_t point=(size_t)strtoull(argv[1],NULL,10);assert(xrtMemDebugFailAfter(point));
        bool ok=execute(),hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();assert(ok==!hit);
        xrtClearError();xlir_module_fini();xrtClearError();assert(!xlir_module_initialized);
        assert(execute());xlir_module_fini();assert(!xlir_module_initialized && !xrtGetError());
        printf("caller prefix=%zu hit=%u balanced\n",point,(unsigned)hit);
    }
    assert(!xl6ProductMemoryEnd());xl6ProductMemoryVerify();return 0;
}
