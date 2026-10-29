#include "release/include/xruntime_decl.h"
#include "src/runtime/xl6_integer_convert.h"
#include "src/runtime/xl6_float_convert.h"
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define REFLECTION_ID UINT64_C(0x99110022)
static size_t successes, rejections;
static void convert(const xvalue* value, unsigned target, uint64_t expected)
{
    unsigned char output[10]; memset(output, 0xa5, sizeof(output));
    xl6ProductMemoryBegin(0);
    assert(xl6IntegerValueConvertV1(value, target, REFLECTION_ID, output + 1));
    assert(!xl6ProductMemoryEnd() && !xrtGetError()); xl6ProductMemoryVerify();
    uint64_t number = 0; memcpy(&number, output + 1, sizeof(number));
    assert(number == expected && output[0] == 0xa5 && output[9] == 0xa5); ++successes;
}
static void reject(const xvalue* value, unsigned target, xerrkind kind)
{
    unsigned char output[10], saved[10]; memset(output, 0xa5, sizeof(output)); memcpy(saved, output, sizeof(output));
    assert(!xl6IntegerValueConvertV1(value, target, REFLECTION_ID, output + 1));
    assert(xrtGetError() && xrtErrorKind(xrtGetError()) == kind && !memcmp(output, saved, sizeof(output)));
    xrtClearError(); ++rejections;
}
int main(void)
{
    xl6ProductMemoryBegin(SIZE_MAX); assert(!xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
    xmemdebugsnapshot before, after; xrtMemDebugSnapshot(&before);
    xvalue* sources[] = {xrtValueBool(true), xrtValueInt(1), xrtValueUInt(1), xrtValueFloat(1.75), xrtValueString((xstrview){" 1 ",3})};
    xvalue* token = xrtValueUInt(1); assert(token && xrtValueTypeIdBind(token, REFLECTION_ID));
    xvalue* badText = xrtValueString((xstrview){"1\0x",3}); assert(badText);
    for (unsigned profile = 1; profile <= 8; ++profile) {
        for (size_t i = 0; i < sizeof(sources) / sizeof(*sources); ++i) { assert(sources[i]); convert(sources[i], profile, 1); }
        convert(NULL, profile, 0);
        reject(token, profile, XERR_TYPE); reject(badText, profile, XERR_VALUE);
    }
    int64_t output = 123;
    assert(!xl6IntegerTextConvertV1((xstrview){"128",3}, XL6_INTEGER_SIGNED_8, &output));
    assert(output == 123 && xrtErrorKind(xrtGetError()) == XERR_RANGE); xrtClearError();
    double real = 123;
    assert(!xl6Float64TextConvertV1((xstrview){"1\0",2}, &real) && real == 123); xrtClearError();
    float single = 123;
    assert(!xl6Float32ValueConvertV2(token, REFLECTION_ID, &single));
    assert(single == 123 && xrtErrorKind(xrtGetError()) == XERR_TYPE); xrtClearError();
    /* Allocation failure must not publish a value or manufacture success when
     * both the initial diagnostic and its replacement cannot be allocated. */
    xl6ProductMemoryBegin(1);
    assert(!xl6IntegerValueConvertV1(badText, XL6_INTEGER_SIGNED_64, REFLECTION_ID, &output));
    assert(xl6ProductMemoryEnd() && output == 123); xrtClearError(); xl6ProductMemoryVerify();
    for (size_t i = 0; i < sizeof(sources) / sizeof(*sources); ++i) xrtValueRelease(sources[i]);
    xrtValueRelease(token); xrtValueRelease(badText);
    xrtMemDebugSnapshot(&after);
    assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
    assert(after.AllocCount - before.AllocCount == after.FreeCount - before.FreeCount);
    printf("language scalar conversions=%zu rejections=%zu zero-allocation unaligned transactional balanced\n", successes, rejections);
    return 0;
}
