#include "src/runtime/xl6_array_extract_native_value_abi.h"
#include "src/runtime/xl6_array.h"
#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_slot_map_key_value.h"
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static XL6_SlotMapKeyV1 genuine(void)
{
    const xrttype* type=xrtTypeInt64();
    XL6_SlotMapLayoutV1 plan={0xEE01,{0xEE02,0xEE03,1,sizeof(int64_t),type}};
    xvalue* schema=xl6SequencePrimitiveCreateV1(XL6_SEQUENCE_ARRAY_V1,plan.value.identity,
        plan.value.elementIdentity,1,sizeof(int64_t),type,XL6_VALUE_ADMIT_I64,false,plan.value.elementIdentity);
    assert(schema); xvalue* map=xl6SlotMapCreateTakeV1(&plan,schema); assert(map);
    XL6_SlotMapKeyV1 key={0}; assert(xl6SlotMapInsertV1(map,&plan,&(int64_t){7},&key));
    xrtValueRelease(map); return key;
}
int main(void)
{
    xl6ProductMemoryBegin(SIZE_MAX);
    const XL6_NativeLeafValueProfileV1* profile=xl6SlotMapKeyValueProfileV1Get();
    const uint64_t identity=0xEE04,element=profile->identity;
    const xrttype* type=profile->Type();
    XL6_SlotMapKeyV1 key=genuine(),result={0};
    /* The descriptor's initialized-slot assignment differs deliberately from
     * checked language acquire, which must refuse an occupied output. */
    XL6_SlotMapKeyV1 replacement=genuine();
    assert(xrtTypeCopyValue(type,&result,&replacement));
    assert(xrtTypeCopyValue(type,&result,&key) && xl6SlotMapKeyEqualV1(result,key));
    assert(xrtTypeCopyValue(type,&result,&result));
    assert(xrtTypeMoveValue(type,&result,&replacement) && !replacement.instance && !replacement.slot);
    xl6SlotMapKeyDropValueV1(result); result=(XL6_SlotMapKeyV1){0};
    xvalue* native=xl6SequenceSlotMapKeyCreateV1(XL6_SEQUENCE_ARRAY_V1,identity); assert(native);
    XL6_Sequence* owner=xl6SequenceBeginRoleV1(native,XL6_SEQUENCE_ARRAY_V1,
        identity,element,5,sizeof(key),type); assert(owner);
    assert(xl6SequenceBindProjectionV1(owner,xl6SlotMapKeyConverterV1())); xl6SequenceEndV1(owner); owner=NULL;
    assert(xl6ArrayPushV1(native,identity,element,5,sizeof(key),type,&key));
    assert(xl6ArrayResizeV1(native,identity,element,5,sizeof(key),type,2));
    XL6_SequenceLayoutV1 actual; const xvalueconverter* converter=NULL;
    assert(xl6SequenceBeginErasedArrayV1(native,&owner,&actual,&converter)==1);
    void* alias=(void*)xrtTypedArrayConstGet(xl6SequenceStorageV1(owner),1);
    xl6SequenceEndV1(owner); owner=NULL;
    assert(alias && !xl6ArrayExtractNativeValueV1(native,identity,element,0,false,profile,alias));
    assert(xrtGetError() && profile->Empty(alias)); xrtClearError();
    assert(xl6ArraySizeV1(native,identity,element,5,sizeof(key),type,false)==2);
    assert(xl6ArrayResizeV1(native,identity,element,5,sizeof(key),type,1));
    assert(!xl6ArrayExtractNativeValueV1(native,identity+1,element,0,true,profile,&result));
    assert(!result.instance && xrtGetError()); xrtClearError();
    assert(!xl6ArrayExtractNativeValueV1(native,identity,element+1,0,true,profile,&result));
    assert(!result.instance && xrtGetError()); xrtClearError();
    assert(!xl6ArrayExtractNativeValueV1(native,identity,element,0,true,profile,&key));
    assert(key.instance && xrtGetError()); xrtClearError();
    assert(!xl6ArrayExtractNativeValueV1(native,identity,element,1,true,profile,&result));
    assert(!result.instance && xrtGetError()); xrtClearError();
    /* Alignment refusal must occur before a typed Empty probe. */
    _Alignas(max_align_t) unsigned char raw[sizeof(key)+1]={0};
    assert(!xl6ArrayExtractNativeValueV1(native,identity,element,0,true,profile,raw+1));
    for (size_t i=0;i<sizeof(raw);++i) assert(!raw[i]);
    assert(xrtGetError()); xrtClearError();
    assert(xrtMemDebugFailAfter(0));
    assert(xl6ArrayExtractNativeValueV1(native,identity,element,0,true,profile,&result));
    assert(!xrtMemDebugFailTriggered()); xrtMemDebugFailClear();
    assert(xl6SlotMapKeyEqualV1(key,result) && xl6ArraySizeV1(native,identity,element,5,sizeof(key),type,false)==0);
    xl6SlotMapKeyDropValueV1(result); result=(XL6_SlotMapKeyV1){0};
    assert(!xl6ArrayExtractNativeValueV1(native,identity,element,0,true,profile,&result));
    assert(!result.instance && xrtGetError()); xrtClearError();
    /* Empty records are real, valid values; the output sentinel is not status. */
    assert(xl6ArrayResizeV1(native,identity,element,5,sizeof(key),type,1));
    assert(xl6ArrayExtractNativeValueV1(native,identity,element,0,false,profile,&result));
    assert(!result.instance && !result.slot && !xrtGetError());
    xvalue* wire=xrtValueArray(); assert(wire);
    assert(xrtValueArrayAppendNew(wire,xl6SlotMapKeyBoxV1(key)));
    assert(xl6ArrayExtractNativeValueV1(wire,identity,element,0,false,profile,&result));
    assert(xl6SlotMapKeyEqualV1(key,result) && !xrtValueCount(wire));
    xl6SlotMapKeyDropValueV1(result); result=(XL6_SlotMapKeyV1){0};
    assert(xrtValueArrayAppendNew(wire,xrtValueInt(9)));
    assert(!xl6ArrayExtractNativeValueV1(wire,identity,element,0,true,profile,&result));
    assert(!result.instance && xrtValueCount(wire)==1 && xrtGetError()); xrtClearError();
    xrtValueRelease(wire); xrtValueRelease(native); xl6SlotMapKeyDropValueV1(key);
    assert(!xrtGetError()); assert(!xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
    puts("[PASS] native/wire owning extraction, no-allocation native move, empty value, refusal and ownership");
    return 0;
}
