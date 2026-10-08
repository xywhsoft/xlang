#ifndef XL6_TEST_ARRAY_PRODUCER_H
#define XL6_TEST_ARRAY_PRODUCER_H
#include "src/runtime/xl6_array_input.h"
#include <assert.h>
#include <string.h>
/* Native host probes supply deliberate, independent logical identities.
 * Language fixtures additionally prove the compiler's canonical identities. */
#define XL6_TEST_TEXT_ARRAY_IDS UINT64_C(0x1001), UINT64_C(0x2001)
#define XL6_TEST_CHAR_ARRAY_IDS UINT64_C(0x1002), UINT64_C(0x2002)
#define XL6_TEST_INT_ARRAY_IDS UINT64_C(0x1003), UINT64_C(0x2003)
static inline size_t xl6TestArrayCount(const xvalue* value)
{
    XL6_Sequence* pin=NULL; XL6_SequenceLayoutV1 layout; const xvalueconverter* converter=NULL;
    int status=xl6SequenceBeginErasedArrayV1(value,&pin,&layout,&converter);
    assert(status>=0);
    if (!status) return xrtValueCount(value);
    size_t count=xrtTypedArrayCount(xl6SequenceStorageV1(pin)); xl6SequenceEndV1(pin); return count;
}
static inline uint32_t xl6TestArrayCharAt(const xvalue* value, size_t index)
{
    XL6_Sequence* pin=NULL; XL6_SequenceLayoutV1 layout; const xvalueconverter* converter=NULL;
    assert(xl6SequenceBeginErasedArrayV1(value,&pin,&layout,&converter)==1);
    assert(layout.type==xl6CharTypeV1() && index<xrtTypedArrayCount(xl6SequenceStorageV1(pin)));
    uint32_t result=*(const uint32_t*)xrtTypedArrayGet(xl6SequenceStorageV1(pin),index);
    xl6SequenceEndV1(pin); return result;
}
static inline void xl6TestArrayTextEquals(const xvalue* value, size_t index, xstrview expected)
{
    XL6_Sequence* pin=NULL; XL6_SequenceLayoutV1 layout; const xvalueconverter* converter=NULL;
    assert(xl6SequenceBeginErasedArrayV1(value,&pin,&layout,&converter)==1);
    assert(layout.type==xl6StringTypeV1() && index<xrtTypedArrayCount(xl6SequenceStorageV1(pin)));
    const xstrview* result=xrtTypedArrayGet(xl6SequenceStorageV1(pin),index);
    assert(result->Size==expected.Size && (!expected.Size || !memcmp(result->Data,expected.Data,expected.Size)));
    xl6SequenceEndV1(pin);
}
#endif
