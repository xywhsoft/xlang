/* Included after generated C. Check language status independently of presence,
 * including native failure without an allocated diagnostic. */
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef XL6_TREE_QUERY_TEST_FAULTS
static size_t point,seen;
static bool armed;
extern bool __real_xl6TreeQueryV2(const xvalue*,const XL6_TreeLayoutV1*,
    XL6_TreeQueryV2,const void*,bool,XL6_TreeQuerySlotV2,XL6_TreeQuerySlotV2,bool*);
bool __wrap_xl6TreeQueryV2(const xvalue* owner,const XL6_TreeLayoutV1* layout,
    XL6_TreeQueryV2 operation,const void* search,bool required,
    XL6_TreeQuerySlotV2 key,XL6_TreeQuerySlotV2 value,bool* found)
{
    if(armed && seen++==point) return false;
    return __real_xl6TreeQueryV2(owner,layout,operation,search,required,key,value,found);
}
#endif
int main(int argc,char** argv)
{
    xl6ProductMemoryBegin(SIZE_MAX); assert(xlir_module_init());
    if(argc==1) {
        for(unsigned round=0;round<1000;++round) assert(xlang_main(NULL)==0 && !xrtGetError());
    } else {
#ifdef XL6_TREE_QUERY_TEST_FAULTS
        assert(argc==3); point=(size_t)strtoull(argv[2],NULL,10);
        bool oom=!strcmp(argv[1],"oom"); assert(oom || !strcmp(argv[1],"query"));
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
    puts("[PASS] owned OrderedMap language queries"); return 0;
}
