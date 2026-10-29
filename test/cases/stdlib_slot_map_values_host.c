#include "src/runtime/xl6_slot_map.h"
#include "src/runtime/xl6_array_admission.h"
#include "test/support/module_product_memory.h"
#include <assert.h>
int main(void)
{
    xl6ProductMemoryBegin(SIZE_MAX);
    const XL6_SlotMapLayoutV1 layout={0xDB11,{0xDB12,0xDB13,1,sizeof(int64_t),NULL}};
    XL6_SlotMapLayoutV1 exact=layout; exact.value.type=xrtTypeInt64();
    for (unsigned at=0;at<1000;++at) {
        xvalue* schema=xl6SequencePrimitiveCreateV1(1,exact.value.identity,exact.value.elementIdentity,1,
            sizeof(int64_t),xrtTypeInt64(),XL6_VALUE_ADMIT_I64,false,exact.value.elementIdentity);
        xvalue* map=xl6SlotMapCreateTakeV1(&exact,schema); assert(map);
        XL6_SlotMapKeyV1 key={0}; int64_t input=at; assert(xl6SlotMapInsertV1(map,&exact,&input,&key));
        xvalue* result=xl6SlotMapValuesV1(map,&exact,NULL); assert(result);
        xl6SlotMapKeyDropValueV1(key); xrtValueRelease(map);
        XL6_Sequence* pin=NULL; XL6_SequenceLayoutV1 output={0}; const xvalueconverter* converter=NULL;
        assert(xl6SequenceBeginErasedArrayV1(result,&pin,&output,&converter)==1 && pin && !converter);
        assert(output.identity==exact.value.identity && output.elementIdentity==exact.value.elementIdentity);
        assert(xrtTypedArrayCount(xl6SequenceStorageV1(pin))==1);
        assert(*(const int64_t*)xrtTypedArrayConstGet(xl6SequenceStorageV1(pin),0)==input);
        xl6SequenceEndV1(pin); xrtValueRelease(result);
    }
    assert(!xrtGetError() && !xl6ProductMemoryEnd()); xl6ProductMemoryVerify(); return 0;
}
