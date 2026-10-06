#define XRT_MODULE_MEMORY_DEBUG
#include "src/runtime/xl6_value_iterator.h"
#include <assert.h>
#include <stdio.h>
static unsigned active,advances,failAt,withoutDiagnostic;
static const XL6_ValueIteratorRuntimeV2* iteratorTestRuntime(void);
#define xl6ValueIteratorRuntimeV2Get iteratorTestRuntime
#include XL6_ITERATOR_GENERATED
#undef xl6ValueIteratorRuntimeV2Get
#include XL6_ITERATOR_BINDINGS
static XL6_ValueIterator* create(const xvalue* source)
{ XL6_ValueIterator* state=xl6ValueIteratorRuntimeV2Get()->Create(source); if(state) ++active; return state; }
static bool advance(XL6_ValueIterator* state,bool* found)
{
    if(failAt && ++advances==failAt) {
        *found=false; if(!withoutDiagnostic) xrtSetErrorKind(XERR_MEMORY); return false;
    }
    return xl6ValueIteratorRuntimeV2Get()->Advance(state,found);
}
static void destroy(XL6_ValueIterator* state)
{ assert(active); --active; xl6ValueIteratorRuntimeV2Get()->Destroy(state); }
static const XL6_ValueIteratorRuntimeV2* iteratorTestRuntime(void)
{
    static XL6_ValueIteratorRuntimeV2 api;
    if(!api.Create) {api=*xl6ValueIteratorRuntimeV2Get(); api.Create=create; api.Advance=advance; api.Destroy=destroy;}
    return &api;
}
int main(void)
{
    assert(xrtMemDebugEnable(true)); assert(xlir_module_init() && !active);
    xmemdebugsnapshot before,after; xrtMemDebugSnapshot(&before);
    for(unsigned cycle=0;cycle<100;++cycle) {
        xvalue* source=xrtValueIntMap(); assert(source);
        assert(xrtValueIntMapSetNew(source,2,xrtValueInt(1)) && xrtValueIntMapSetNew(source,7,xrtValueInt(2)));
        int64_t cleanup=XL6_ITER_CLEANUPS; XL6_ITER_RESULT result=XL6_ITER_COUNTED(source);
        assert(result.ok && result.value==3 && !active && XL6_ITER_CLEANUPS==cleanup+1);
        for(unsigned fault=1;fault<=3;++fault) {
            cleanup=XL6_ITER_CLEANUPS; failAt=fault; advances=0;
            withoutDiagnostic=fault==3;
            result=XL6_ITER_COUNTED(source);
            assert(!result.ok && !active && XL6_ITER_CLEANUPS==cleanup+1 && advances==fault);
            if(!withoutDiagnostic) assert(xrtErrorKind(xrtGetError())==XERR_MEMORY);
            xrtClearError(); failAt=withoutDiagnostic=0;
        }
        result=XL6_ITER_COUNTED(source); assert(result.ok && result.value==3 && !active);
        xrtValueRelease(source);
    }
    xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes &&
        after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount &&
        before.InvalidFreeCount==after.InvalidFreeCount && before.DoubleFreeCount==after.DoubleFreeCount &&
        before.UseAfterFreeCount==after.UseAfterFreeCount && !xrtGetError());
    xlir_module_fini(); xlir_module_fini(); assert(!xrtGetError());
    puts("generated checked cursor: 100 physical cycles, all failure/finally routes balanced"); return 0;
}
