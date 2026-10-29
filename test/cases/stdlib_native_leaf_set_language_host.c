/* Actual generated source consumes genuine keys through its public var
 * argument boundary. The source SlotMapKey ABI itself is not yet enabled. */
#include "src/runtime/xl6_slot_map_key_value.h"
#include "src/runtime/xl6_array_admission.h"
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdio.h>
static xvalue* create(XL6_SlotMapKeyV1* key)
{
    const xrttype* type=xrtTypeInt64();
    XL6_SlotMapLayoutV1 layout={0xDA01,{0xDA02,0xDA03,1,sizeof(int64_t),type}};
    xvalue* witness=xl6SequencePrimitiveCreateV1(XL6_SEQUENCE_ARRAY_V1,layout.value.identity,
        layout.value.elementIdentity,1,sizeof(int64_t),type,XL6_VALUE_ADMIT_I64,false,layout.value.elementIdentity);
    assert(witness); xvalue* map=xl6SlotMapCreateTakeV1(&layout,witness); assert(map);
    assert(xl6SlotMapInsertV1(map,&layout,&(int64_t){42},key)); return map;
}
static void keyDrop(XL6_SlotMapKeyV1* key) { xrtTypeDropValue(xl6SlotMapKeyTypeV1(),key); }
int main(void)
{
    xl6ProductMemoryBegin(SIZE_MAX); assert(xlir_module_init());
    XL6_SlotMapKeyV1 key={0},otherKey={0}; xvalue* map=create(&key),*other=create(&otherKey);
    xvalue* arguments=xrtValueArray(); assert(arguments);
    assert(xrtValueArrayAppendNew(arguments,xl6SlotMapKeyBoxV1(key)) &&
        xrtValueArrayAppendNew(arguments,xl6SlotMapKeyBoxV1(key)) &&
        xrtValueArrayAppendNew(arguments,xl6SlotMapKeyBoxV1(otherKey)));
    xrtValueRelease(map); xrtValueRelease(other); keyDrop(&key); keyDrop(&otherKey);
    assert(xlang_main(arguments)==0 && !xrtGetError()); xrtValueRelease(arguments);
    xlir_module_fini(); assert(!xrtGetError() && !xlir_module_initialized);
    assert(!xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
    puts("[PASS] actual generated Value Set content/identity and native key lifetimes"); return 0;
}
