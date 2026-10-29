/* Real generated functions; no copied read expressions or simulated native
 * carriers. Bindings (including logical IDs) come from verified source IR. */
#include "src/runtime/xl6_array.h"
#include "src/runtime/xl6_array_erased.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#define LAYOUT(Suffix, Type, CType) XL6_READ_##Suffix##_OWNER, XL6_READ_##Suffix##_ELEMENT, 1, sizeof(CType), Type()
static void error(xerrkind kind)
{
    assert(xrtGetError() && xrtErrorKind(xrtGetError()) == kind); xrtClearError();
}
int main(void)
{
    assert(xrtMemDebugEnable(true)); assert(xlir_module_init());
    xmemdebugsnapshot before, after; xrtMemDebugSnapshot(&before);
    for (unsigned cycle = 0; cycle < 100; ++cycle) {
#define VECTOR(Suffix, Type, CType, Value) do { \
    xvalue* source = xl6ArrayCreateV1(LAYOUT(Suffix, Type, CType)); assert(source); \
    CType value = (Value); assert(xl6ArrayPushV1(source, LAYOUT(Suffix, Type, CType), &value)); \
    xvalue* alias = xl6ArrayEraseV1(source, LAYOUT(Suffix, Type, CType), NULL); assert(alias == source); \
    assert(xrtMemDebugFailAfter(0)); \
    XL6_READ_##Suffix##_RESULT result = XL6_READ_##Suffix(source, -1); \
    assert(result.ok && result.value == value && !xrtMemDebugFailTriggered() && !xrtGetError()); \
    xrtMemDebugFailClear(); \
    result = XL6_READ_##Suffix(source, 1); assert(!result.ok); error(XERR_RANGE); \
    result = XL6_READ_##Suffix(source, INT64_MIN); assert(!result.ok); error(XERR_RANGE); \
    XL6_Sequence* pin = xl6SequenceBeginV1(source, LAYOUT(Suffix, Type, CType)); assert(pin); \
    result = XL6_READ_##Suffix(source, 0); assert(!result.ok); error(XERR_STATE); xl6SequenceEndV1(pin); \
    xrtValueRelease(alias); xrtValueRelease(source); \
} while (0)
        VECTOR(I64, xrtTypeInt64, int64_t, INT64_MIN);
        VECTOR(U64, xrtTypeUInt64, uint64_t, UINT64_MAX);
        VECTOR(BOOL, xrtTypeBool, bool, true);
        VECTOR(F64, xrtTypeFloat64, double, -3.5);
#undef VECTOR
        xvalue* source = xl6ArrayCreateV1(LAYOUT(I64, xrtTypeInt64, int64_t)); assert(source);
        int64_t item = 7; assert(xl6ArrayPushV1(source, LAYOUT(I64, xrtTypeInt64, int64_t), &item));
        xvalue* alias = xl6ArrayEraseV1(source, LAYOUT(I64, xrtTypeInt64, int64_t), NULL); assert(alias == source);
        XL6_READ_SUM_RESULT sum = XL6_READ_SUM(source); assert(sum.ok && sum.value == 7);
        xrtValueRelease(alias); xrtValueRelease(source);
        /* Reproduce a failed projection AFTER activation of the loop owner.
         * The owned snapshot must close before the function propagates Type. */
        xvalue* invalid = xrtValueArray(); assert(invalid);
        assert(xrtValueArrayAppendNew(invalid, xrtValueFloat(2.5)));
        sum = XL6_READ_SUM(invalid); assert(!sum.ok); error(XERR_TYPE); xrtValueRelease(invalid);
    }
    xrtMemDebugSnapshot(&after);
    assert(!xrtGetError() && before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes &&
        after.AllocCount - before.AllocCount == after.FreeCount - before.FreeCount &&
        before.InvalidFreeCount == after.InvalidFreeCount && before.DoubleFreeCount == after.DoubleFreeCount &&
        before.UseAfterFreeCount == after.UseAfterFreeCount);
    xlir_module_fini(); xlir_module_fini(); assert(!xrtGetError());
    puts("generated/native scalar reads: 100 physical cycles balanced; reads zero allocations"); return 0;
}
