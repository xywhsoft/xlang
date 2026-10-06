/* Calls the actual generated checked function with a resident native Array;
 * owner/element TypeIds and callable binding are extracted from verified IR. */
#include "src/runtime/xl6_array.h"
#include "src/runtime/xl6_array_erased.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define LAYOUT XL6_READ_STRING_OWNER,XL6_READ_STRING_ELEMENT,2,sizeof(xstrview),xl6StringTypeV1()
static void error(xerrkind kind)
{ assert(xrtGetError() && xrtErrorKind(xrtGetError())==kind); xrtClearError(); }
int main(void)
{
    assert(xrtMemDebugEnable(true)); assert(xlir_module_init());
    xmemdebugsnapshot before,after; xrtMemDebugSnapshot(&before);
    for (unsigned cycle=0;cycle<100;++cycle) {
        xvalue* value=xl6ArrayCreateV1(LAYOUT); assert(value);
        xstrview item={"a\0b",3}; assert(xl6ArrayPushV1(value,LAYOUT,&item));
        xvalue* alias=xl6ArrayEraseV1(value,LAYOUT,xl6StringConverterV1()); assert(alias==value);
        XL6_READ_STRING_RESULT result=XL6_READ_STRING(value,-1);
        assert(result.ok && result.value.Data && result.value.Size==3 && !memcmp(result.value.Data,item.Data,3));
        xrtFree((void*)result.value.Data);
        result=XL6_READ_STRING(value,1); assert(!result.ok); error(XERR_RANGE);
        XL6_Sequence* pin=xl6SequenceBeginV1(value,LAYOUT); assert(pin);
        result=XL6_READ_STRING(value,0); assert(!result.ok); error(XERR_STATE); xl6SequenceEndV1(pin);
        assert(xrtMemDebugFailAfter(0)); result=XL6_READ_STRING(value,0);
        assert(!result.ok && xrtMemDebugFailTriggered()); xrtMemDebugFailClear(); error(XERR_MEMORY);
        result=XL6_READ_STRING(value,0); assert(result.ok); xrtFree((void*)result.value.Data);
        xrtValueRelease(alias); xrtValueRelease(value);
    }
    xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes &&
        after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount &&
        before.InvalidFreeCount==after.InvalidFreeCount && before.DoubleFreeCount==after.DoubleFreeCount &&
        before.UseAfterFreeCount==after.UseAfterFreeCount && !xrtGetError());
    xlir_module_fini(); xlir_module_fini(); assert(!xrtGetError());
    puts("generated/native string reads: 100 physical cycles balanced"); return 0;
}
