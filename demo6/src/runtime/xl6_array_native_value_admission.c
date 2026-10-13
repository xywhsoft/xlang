#include "xl6_array_native_value_admission.h"
#include "xl6_slot_map_key_value.h"
#include "xl6_array_native_value_policy.h"
static bool empty(const void* raw)
{ const XL6_SlotMapKeyV1* key=raw; return key && !key->instance && !key->slot; }
static bool copy(const void* input,void* output)
{
    return xl6NativeLeafValueCopyV1(xl6SlotMapKeyValueProfileV1Get(),input,output);
}
static bool slot(const void* item)
{
    const XL6_SlotMapKeyV1* key=item;
    return key && (!key->instance==!key->slot);
}
const XL6_NativeCollectionValueProfileV1* xl6SlotMapKeyCollectionProfileV1Get(void)
{
    static const XL6_NativeCollectionValueProfileV1 profile={xl6SlotMapKeyTypeV1,
        XL6_SLOT_MAP_KEY_LANGUAGE_VALUE_ID,XL6_SLOT_MAP_KEY_LANGUAGE_VALUE_ID,
        xl6SlotMapKeyContentPolicyV1,empty,copy,slot,xl6SlotMapKeyValueAdmitsV1,xl6SlotMapKeyConverterV1};
    return &profile;
}
const XL6_SequenceAdmissionOpsV2* xl6ArraySlotMapKeyAdmissionOpsV2Get(void)
{return xl6ArrayNativeValueAdmissionOpsV2Get();}
xvalue* xl6SequenceSlotMapKeyCreateV1(unsigned int role,uint64_t identity)
{
    return xl6SequenceNativeValueCreateV1(role,identity,xl6SlotMapKeyCollectionProfileV1Get());
}
