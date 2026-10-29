/* Preinclude real generated C and bindings derived from its verified IR.
 * Exercise the same language functions with genuine native Array storage,
 * not another handwritten stand-in for generated count expressions. */
#include "src/runtime/xl6_array.h"
#include "src/runtime/xl6_array_erased.h"
#include <assert.h>
#include <stdio.h>
#define LAYOUT XL6_COUNT_OWNER_ID,XL6_COUNT_ELEMENT_ID,1,sizeof(int64_t),xrtTypeInt64()
int main(void)
{
    assert(xrtMemDebugEnable(true));
    assert(xlir_module_init());
    xmemdebugsnapshot before,after; xrtMemDebugSnapshot(&before);
    for(unsigned cycle=0;cycle<100;++cycle) {
        xvalue* source=xl6ArrayCreateV1(LAYOUT); assert(source);
        int64_t item=1; assert(xl6ArrayPushV1(source,LAYOUT,&item));
        xvalue* erased=xl6ArrayEraseV1(source,LAYOUT,NULL); assert(erased==source);
        xlir_result_i64 scalar=XL6_COUNT_ERASED(erased); assert(scalar.ok && scalar.value==1);
        xlir_result_xvalue boxed=XL6_COUNT_SAFE(erased); int64_t result=0;
        assert(boxed.ok && xrtValueGetInt(boxed.value,&result) && result==1); xrtValueRelease(boxed.value);
        XL6_COUNT_OPTIONAL_RESULT optional=XL6_COUNT_OPTIONAL(source);
        assert(optional.ok && optional.value.hasValue && optional.value.value==1);
        optional=XL6_COUNT_OPTIONAL(xrtValueNull()); assert(optional.ok && !optional.value.hasValue);
        XL6_Sequence* pin=xl6SequenceBeginV1(source,LAYOUT); assert(pin);
        scalar=XL6_COUNT_ERASED(source); assert(!scalar.ok && xrtErrorKind(xrtGetError())==XERR_STATE);
        xrtClearError();
        boxed=XL6_COUNT_SAFE(source); assert(!boxed.ok && xrtErrorKind(xrtGetError())==XERR_STATE);
        xrtClearError();
        optional=XL6_COUNT_OPTIONAL(source); assert(!optional.ok && xrtErrorKind(xrtGetError())==XERR_STATE);
        xrtClearError(); xl6SequenceEndV1(pin);
        boxed=XL6_COUNT_SAFE(xrtValueNull());
        assert(boxed.ok && boxed.value && xrtValueType(boxed.value)==XVALUE_NULL);
        xrtValueRelease(boxed.value);
        xrtValueRelease(erased); xrtValueRelease(source);
    }
    xrtMemDebugSnapshot(&after);
    assert(!xrtGetError() && before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes &&
        after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount);
    xlir_module_fini(); xlir_module_fini(); assert(!xrtGetError());
    puts("generated/native count: 100 physical cycles balanced"); return 0;
}
