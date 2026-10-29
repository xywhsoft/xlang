#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_VALUE_CONTAINER
#include "lib/xrt/xrt.h"
#include "src/runtime/xl6_value_iterator.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Only the provider TU redirects CursorAdvance. Normal calls still use the
 * actual XRT cursor; the selected failure is a contract-injection test, not
 * an allocation-failure claim for the underlying cursor. */
static const XL6_ValueIteratorRuntimeV2* api;
static XL6_ValueIterator* observing;
static unsigned mode, probes;
void iteratorProbeSetErrorKind(xerrkind kind)
{
    /* An independently omitted diagnostic tests the physical status contract;
     * canonical XRT kind errors are preallocated, not an OOM allocation site. */
    if (mode!=2) xrtSetErrorKind(kind);
}
xvalueiterresult iteratorProbeAdvance(xvaluecursor* cursor,xvaluekey* key,xvalue** item)
{
    ++probes;
    if (mode==1) { xrtSetErrorKind(XERR_MEMORY); return XVALUE_ITER_ERROR; }
    if (mode==2) return XVALUE_ITER_ERROR;
    if (mode==3) {
        bool found=true;
        assert(!api->Advance(observing,&found) && !found);
        assert(xrtErrorKind(xrtGetError())==XERR_STATE); xrtClearError();
        mode=0;
    }
    return xrtValueCursorAdvance(cursor,key,item);
}
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after; xrtMemDebugSnapshot(&after);
    assert(before->LiveCount==after.LiveCount && before->LiveBytes==after.LiveBytes &&
        after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount &&
        before->InvalidFreeCount==after.InvalidFreeCount && before->DoubleFreeCount==after.DoubleFreeCount &&
        before->UseAfterFreeCount==after.UseAfterFreeCount && !xrtGetError());
}
static xvalue* array(void)
{
    xvalue* result=xrtValueArray(); assert(result);
    assert(xrtValueArrayAppendNew(result,xrtValueString((xstrview){"a\0b",3})));
    assert(xrtValueArrayAppendNew(result,xrtValueString((xstrview){"",0})));
    return result;
}
static void vectors(void)
{
    bool found=true;
    assert(!api->Advance(NULL,&found) && !found); xrtClearError();
    xvalue* source=array(); XL6_ValueIterator* state=api->Create(source); assert(state);
    assert(!api->Advance(state,NULL)); assert(xrtErrorKind(xrtGetError())==XERR_ARGUMENT); xrtClearError();
    assert(api->Advance(state,&found) && found);
    const XL6_ValueIteratorView* view=api->View(state); xstrview text={0};
    assert(view && view->ordinal==0 && view->key.Type==XVALUE_KEY_INDEX && view->key.Index==0 &&
        xrtValueGetString(view->item,&text) && text.Size==3 && !memcmp(text.Data,"a\0b",3));
    /* A mutation of the original shell must not change the actual snapshot. */
    assert(xrtValueArraySetNew(source,1,xrtValueInt(42))); xrtValueRelease(source);
    observing=state; mode=3;
    assert(api->Advance(state,&found) && found && mode==0); observing=NULL;
    view=api->View(state); assert(view && view->ordinal==1 && xrtValueGetString(view->item,&text) && text.Size==0);
    assert(api->Advance(state,&found) && !found && !api->View(state)->item);
    assert(api->Advance(state,&found) && !found); api->Destroy(state);
    for (unsigned failure=1;failure<=2;++failure) {
        mode=failure;
        source=array(); state=api->Create(source); assert(state); xrtValueRelease(source);
        unsigned before=probes;
        if(mode==2) assert(xrtMemDebugFailAfter(0));
        assert(!api->Advance(state,&found) && !found);
        bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
        if(mode==1) assert(xrtErrorKind(xrtGetError())==XERR_MEMORY);
        else assert(!hit && !xrtGetError()); /* Explicitly diagnostic-free injection. */
        xrtClearError();
        mode=0;
        assert(!api->Advance(state,&found) && !found && probes==before+1);
        assert(xrtErrorKind(xrtGetError())==XERR_STATE); xrtClearError();
        assert(!api->View(state)); assert(xrtErrorKind(xrtGetError())==XERR_STATE); xrtClearError();
        api->Destroy(state);
    }
    mode=0;
}
int main(int argc,char** argv)
{
    api=xl6ValueIteratorRuntimeV2Get(); assert(xrtMemDebugEnable(true));
    xrtSetErrorKind(XERR_MEMORY); xrtClearError(); xmemdebugsnapshot before;
    xrtMemDebugSnapshot(&before);
    if(argc==2) {
        xvalue* source=array(); assert(xrtMemDebugFailAfter((size_t)strtoull(argv[1],NULL,10)));
        XL6_ValueIterator* state=api->Create(source); bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
        if(state) api->Destroy(state); else assert(hit);
        xrtClearError(); xrtValueRelease(source); printf("create hit=%u balanced\n",hit ? 1u : 0u);
    } else {
        assert(argc==1); for(unsigned i=0;i<100;++i) vectors();
        puts("checked iterator: 100 physical cycles balanced");
    }
    balanced(&before); return 0;
}
