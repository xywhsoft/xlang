/* Every direct native copy runs through the real checked leaf implementation,
 * except one explicitly selected false return. No production provider is
 * substituted; GCC wrapping exposes the generated frame's failure branch. */
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef XLIR_NATIVE_SSA_SOURCE
#error "Pass verified generated native SSA C"
#endif
#include XLIR_NATIVE_SSA_SOURCE
static bool armed;
static unsigned lane;
static size_t seen,typedSeen,point,cleanups,calls;
extern bool __real_xl6SlotMapKeyCopyValueV1(XL6_SlotMapKeyV1,XL6_SlotMapKeyV1*);
bool __wrap_xl6SlotMapKeyCopyValueV1(XL6_SlotMapKeyV1 input,XL6_SlotMapKeyV1* output)
{
    size_t at=seen++;
    if(armed && lane<2 && at==point){
        assert(!output->instance && !output->slot);
        if(lane==1)xrtSetErrorKind(XERR_RANGE);
        return false;
    }
    return __real_xl6SlotMapKeyCopyValueV1(input,output);
}
extern bool __real_xrtTypeCopyValue(const xrttype*,void*,const void*);
bool __wrap_xrtTypeCopyValue(const xrttype* type,void* output,const void* input)
{
    if(type==xl6SlotMapKeyTypeV1()){
        size_t at=typedSeen++;
        if(armed && lane>=2 && at==point){
            assert(__real_xrtTypeCopyValue(type,output,input));
            /* The real checked leaf must undo a partial acquisition, whether
             * Copy reports false without a diagnostic or true with one. */
            if(lane==3)xrtSetErrorKind(XERR_RANGE);
            return lane==3;
        }
    }
    return __real_xrtTypeCopyValue(type,output,input);
}
void xlir_spill_cleanup_seen(void){++cleanups;}
xfuture* xlir_make_i64_future(void)
{
    static int64_t value=41;xfuture* future=NULL;
    xpromise* promise=xrtPromiseCreate(&future,NULL);++calls;
    if(!promise)return future;
    assert(xrtPromiseResolve(promise,&value));xrtPromiseDestroy(promise);return future;
}
static size_t success(void)
{
    seen=typedSeen=cleanups=calls=0;assert(xlir_module_init());
    assert(cleanups==1 && calls==1 && !xrtGetError());
    size_t copies=seen;xlir_module_fini();assert(!xrtGetError() && !xlir_module_initialized);
    return copies;
}
int main(int argc,char** argv)
{
    xl6ProductMemoryBegin(SIZE_MAX);
    if(argc==1){
        size_t copies=0;
        for(unsigned i=0;i<20;++i){size_t now=success();if(i)assert(copies==now);copies=now;}
        printf("lifetimes=20 native-copies=%zu\n",copies);
    }else{
        assert(argc==3);point=(size_t)strtoull(argv[2],NULL,10);
        bool oom=!strcmp(argv[1],"oom");
        lane=!strcmp(argv[1],"copy-error")?1:!strcmp(argv[1],"partial-copy")?2:!strcmp(argv[1],"true-error")?3:0;
        assert(oom || !strcmp(argv[1],"copy") || !strcmp(argv[1],"copy-error") ||
            !strcmp(argv[1],"partial-copy") || !strcmp(argv[1],"true-error"));
        if(oom)assert(xrtMemDebugFailAfter(point));else armed=true;
        bool ok=xlir_module_init()!=0,hit=oom?xrtMemDebugFailTriggered():(lane>=2?typedSeen:seen)>point;
        armed=false;xrtMemDebugFailClear();assert(ok==!hit);
        if(!oom || calls)assert(cleanups==1);
        /* Checked false is authoritative even with a NULL diagnostic. The
         * deliberate diagnostic lanes must retain their precise first error. */
        if(hit && !oom && (lane==1 || lane==3))
            assert(xrtGetError() && xrtErrorKind(xrtGetError())==XERR_RANGE);
        xrtClearError();xlir_module_fini();xrtClearError();success();
        printf("prefix=%s at=%zu hit=%u\n",argv[1],point,(unsigned)hit);
    }
    assert(!xl6ProductMemoryEnd());xl6ProductMemoryVerify();return 0;
}
