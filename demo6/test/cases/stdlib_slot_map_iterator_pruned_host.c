#include "src/runtime/xl6_slot_map_iterator.h"
#include "src/runtime/xl6_array_admission.h"
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdio.h>
#ifndef MODE
#error MODE is required
#endif
int main(void)
{
    xl6ProductMemoryBegin(SIZE_MAX);
    const XL6_SlotMapLayoutV1 layout={0xCC11,{0xCC12,0xCC13,1,sizeof(int64_t),xrtTypeInt64()}};
    xvalue* witness=xl6SequencePrimitiveCreateV1(1,layout.value.identity,layout.value.elementIdentity,1,
        sizeof(int64_t),xrtTypeInt64(),XL6_VALUE_ADMIT_I64,false,layout.value.elementIdentity);
    xvalue* map=xl6SlotMapCreateTakeV1(&layout,witness); assert(map); XL6_SlotMapKeyV1 original={0};
    assert(xl6SlotMapInsertV1(map,&layout,&(int64_t){42},&original));
#if MODE==0
    XL6_ValueIterator* cursor=xl6SlotMapCountIteratorCreateV1(map,&layout);
#elif MODE==1
    XL6_ValueIterator* cursor=xl6SlotMapKeyIteratorCreateV1(map,&layout,0xCC14);
#elif MODE==2
    XL6_ValueIterator* cursor=xl6SlotMapItemIteratorCreateV1(map,&layout,NULL);
#elif MODE==3
    XL6_ValueIterator* cursor=xl6SlotMapPairIteratorCreateV1(map,&layout,0xCC14,NULL);
#endif
    assert(cursor); xrtValueRelease(map); const XL6_ValueIteratorRuntimeV2* api=xl6ValueIteratorRuntimeV2Get();
    bool found=false; assert(api->Advance(cursor,&found) && found && api->View(cursor)->ordinal==0);
#if MODE&1
    XL6_SlotMapKeyV1 key={0}; assert(xl6ValueIteratorKeyCopyV3(cursor,&key) && xl6SlotMapKeyEqualV1(key,original));
    xl6SlotMapKeyDropValueV1(key);
#endif
#if MODE&2
    int64 value=0; assert(xrtValueGetInt(api->View(cursor)->item,&value) && value==42);
#else
    assert(!api->View(cursor)->item);
#endif
    assert(api->Advance(cursor,&found) && !found); api->Destroy(cursor); xl6SlotMapKeyDropValueV1(original);
    assert(!xrtGetError() && !xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
    printf("pruned native cursor mode=%u balanced\n",(unsigned)MODE); return 0;
}
