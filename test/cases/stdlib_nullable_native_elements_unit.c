#include "src/runtime/xl6_nullable_slot_map_key_collection.h"
#include "src/runtime/xl6_array_native_value_admission.h"
#include "src/runtime/xl6_array_extract_native_value_abi.h"
#include "src/runtime/xl6_array.h"
#include "src/runtime/xl6_array_admission.h"
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdio.h>
static XL6_SlotMapKeyV1 genuine(void)
{
    XL6_SlotMapLayoutV1 l={0xEE01,{0xEE02,0xEE03,1,sizeof(int64_t),xrtTypeInt64()}};
    xvalue* schema=xl6SequencePrimitiveCreateV1(XL6_SEQUENCE_ARRAY_V1,l.value.identity,
        l.value.elementIdentity,1,sizeof(int64_t),l.value.type,XL6_VALUE_ADMIT_I64,false,l.value.elementIdentity);
    assert(schema);xvalue* map=xl6SlotMapCreateTakeV1(&l,schema);assert(map);
    XL6_SlotMapKeyV1 key={0};assert(xl6SlotMapInsertV1(map,&l,&(int64_t){7},&key));
    xrtValueRelease(map);return key;
}
int main(void)
{
    xl6ProductMemoryBegin(SIZE_MAX);
    const XL6_NativeCollectionValueProfileV1* p=xl6NullableSlotMapKeyCollectionProfileV1Get();
    assert(xl6NativeCollectionValueProfileValidV1(p));
    const uint64_t id=0xEE04,element=p->identity;const xrttype* type=p->Type();
    XL6_NullableSlotMapKeyV1 absent={0},presentEmpty={true,{0}},key={true,genuine()},out={0};
    assert(p->Slot(&absent) && p->Slot(&presentEmpty) && p->Slot(&key));
    assert(!p->Content()->Equal(&absent,&presentEmpty,NULL));
    assert(p->Content()->Hash(&absent,NULL)!=p->Content()->Hash(&presentEmpty,NULL));
    XL6_NullableSlotMapKeyV1 poisoned={false,{(void*)1,0}};assert(!p->Slot(&poisoned));
    xvalue* array=xl6SequenceNullableSlotMapKeyCreateV1(XL6_SEQUENCE_ARRAY_V1,id);assert(array);
    XL6_Sequence* owner=xl6SequenceBeginRoleV1(array,XL6_SEQUENCE_ARRAY_V1,id,element,5,sizeof(key),type);assert(owner);
    assert(xl6SequenceBindProjectionV1(owner,p->Converter()));xl6SequenceEndV1(owner);
    assert(xl6ArrayPushV1(array,id,element,5,sizeof(key),type,&key));
    assert(xl6ArrayPushV1(array,id,element,5,sizeof(key),type,&presentEmpty));
    assert(xl6ArrayResizeV1(array,id,element,5,sizeof(key),type,3));
    assert(xl6ArrayFindV1(array,id,element,5,sizeof(key),type,&absent)==2);
    assert(xl6ArrayFindV1(array,id,element,5,sizeof(key),type,&presentEmpty)==1);
    assert(!xl6ArrayPushV1(array,id,element,5,sizeof(key),type,&poisoned));assert(xrtGetError());xrtClearError();
    XL6_SequenceLayoutV1 layout;const xvalueconverter* converter=NULL;
    assert(xl6SequenceBeginErasedArrayV1(array,&owner,&layout,&converter)==1);
    void* alias=(void*)xrtTypedArrayConstGet(xl6SequenceStorageV1(owner),2);xl6SequenceEndV1(owner);
    assert(!xl6ArrayExtractNativeValueV2(array,id,element,0,false,p,alias));
    assert(p->Empty(alias) && xrtGetError());xrtClearError();
    assert(!xl6ArrayExtractNativeValueV2(array,id,element,0,false,p,&key));assert(key.hasValue && xrtGetError());xrtClearError();
    assert(!xl6ArrayExtractNativeValueV2(array,id,element+1,0,false,p,&out));assert(p->Empty(&out) && xrtGetError());xrtClearError();
    _Alignas(max_align_t) unsigned char misaligned[sizeof(out)+1]={0};
    assert(!xl6ArrayExtractNativeValueV2(array,id,element,0,false,p,misaligned+1));assert(xrtGetError());xrtClearError();
    assert(xrtMemDebugFailAfter(0));
    assert(xl6ArrayExtractNativeValueV2(array,id,element,1,false,p,&out));
    assert(out.hasValue && !out.value.instance && !xrtMemDebugFailTriggered());xrtMemDebugFailClear();
    xl6NullableSlotMapKeyDropValueV1(out);out=(XL6_NullableSlotMapKeyV1){0};
    assert(xl6ArrayExtractNativeValueV2(array,id,element,1,false,p,&out));assert(p->Empty(&out));
    assert(xl6ArrayExtractNativeValueV2(array,id,element,0,true,p,&out));assert(xl6NullableSlotMapKeyEqualV1(key,out));
    xl6NullableSlotMapKeyDropValueV1(out);out=(XL6_NullableSlotMapKeyV1){0};
    xvalue* wire=xrtValueArray();assert(wire);
    assert(xrtValueArrayAppendNew(wire,xrtValueNull()));
    assert(xrtValueArrayAppendNew(wire,xl6NullableSlotMapKeyBoxV1(presentEmpty)));
    assert(xrtValueArrayAppendNew(wire,xl6NullableSlotMapKeyBoxV1(key)));
    assert(xl6ArrayExtractNativeValueV2(wire,id,element,0,false,p,&out) && p->Empty(&out));
    assert(xl6ArrayExtractNativeValueV2(wire,id,element,0,false,p,&out) && out.hasValue && !out.value.instance);
    xl6NullableSlotMapKeyDropValueV1(out);out=(XL6_NullableSlotMapKeyV1){0};
    assert(xl6ArrayExtractNativeValueV2(wire,id,element,0,false,p,&out) && xl6NullableSlotMapKeyEqualV1(key,out));
    xl6NullableSlotMapKeyDropValueV1(out);out=(XL6_NullableSlotMapKeyV1){0};
    assert(xrtValueArrayAppendNew(wire,xrtValueInt(9)));
    assert(!xl6ArrayExtractNativeValueV2(wire,id,element,0,false,p,&out));
    assert(xrtValueCount(wire)==1 && p->Empty(&out) && xrtGetError());xrtClearError();
    XL6_NativeCollectionValueProfileV1 wrong=*p;wrong.boxedIdentity=0;
    assert(!xl6NativeCollectionValueProfileValidV1(&wrong));
    assert(!xl6SequenceNativeValueCreateV1(XL6_SEQUENCE_ARRAY_V1,id,&wrong));assert(xrtGetError());xrtClearError();
    xrtValueRelease(wire);xrtValueRelease(array);xl6NullableSlotMapKeyDropValueV1(key);
    assert(!xrtGetError());assert(!xl6ProductMemoryEnd());xl6ProductMemoryVerify();
    puts("optional elements: null/present-empty/content, native/wire move, alias/refusal; balanced");return 0;
}
