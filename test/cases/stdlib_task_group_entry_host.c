/* Generic generated entry failure prefixes, actual unmodified product only. */
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#ifndef XLIR_GENERATED_MODULE
#error An actual emitted module is required
#endif
#define main xl6GeneratedMain
#include XLIR_GENERATED_MODULE
#undef main
int main(int argc,char** argv)
{
    assert(argc==1 || argc==2);
    size_t point=argc==2?(size_t)strtoull(argv[1],NULL,10):SIZE_MAX;
    xl6ProductMemoryBegin(point);int code=xl6GeneratedMain(0,NULL);
    bool hit=xl6ProductMemoryEnd();if(!hit)assert(!code&&!xrtGetError());
    xrtClearError();bool recovered=xlir_entry_module!=NULL;
    if(recovered){
        assert(hit&&code);XL6_StaticModuleResultV1 retired=xl6StaticModuleDrainForV1(xlir_entry_module,-1);
        assert(retired.ready&&xl6StaticModuleDestroyV1(&xlir_entry_module));
        xl6OwnershipCatalogDestroyV1(xlir_entry_catalog);xlir_entry_catalog=NULL;xrtClearError();
    }
    xl6ProductMemoryVerify();
    printf("TaskGroup entry recovered=%d hit=%d complete-prefix balanced\n",recovered,hit);return 0;
}
