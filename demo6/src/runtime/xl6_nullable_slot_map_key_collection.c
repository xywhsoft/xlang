#include "xl6_nullable_slot_map_key_collection.h"
static bool empty(const void* raw)
{
    const XL6_NullableSlotMapKeyV1* key=raw;
    return key && !key->hasValue && !key->value.instance && !key->value.slot;
}
static bool slot(const void* raw)
{
    const XL6_NullableSlotMapKeyV1* key=raw;
    return key && (!key->value.instance==!key->value.slot) &&
        (key->hasValue || (!key->value.instance && !key->value.slot));
}
static bool value(const xvalue* raw)
{return raw && (xrtValueType(raw)==XVALUE_NULL || xl6SlotMapKeyValueAdmitsV1(raw));}
static bool copy(const void* input,void* output)
{return xl6NullableSlotMapKeyCopyValueV1(*(const XL6_NullableSlotMapKeyV1*)input,output);}
static uint64 hash(const void* raw,ptr context)
{
    (void)context;const XL6_NullableSlotMapKeyV1* key=raw;
    /* Null never hashes as the present empty key. No record padding or reads
     * from an absent payload; hashes are process-local, not persistence IDs. */
    return key->hasValue ? xl6SlotMapKeyHashV1(key->value) : UINT64_C(0x0D4F73AB4D6A73E1);
}
static bool equal(const void* a,const void* b,ptr context)
{(void)context;return xl6NullableSlotMapKeyEqualV1(*(const XL6_NullableSlotMapKeyV1*)a,*(const XL6_NullableSlotMapKeyV1*)b);}
static const xtypedsetkeypolicy* content(void)
{static const xtypedsetkeypolicy p={hash,equal,NULL};return &p;}
static bool toTyped(const xvalue* input,const xrttype* type,ptr output,ptr context)
{
    if(context || type!=xl6NullableSlotMapKeyTypeV1()){xrtSetErrorKind(XERR_TYPE);return false;}
    return xl6NullableSlotMapKeyUnboxV1(input,output);
}
static xvalue* fromTyped(const xrttype* type,const void* input,ptr context)
{
    if(context || type!=xl6NullableSlotMapKeyTypeV1()){xrtSetErrorKind(XERR_TYPE);return NULL;}
    if(!input){xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    return xl6NullableSlotMapKeyBoxV1(*(const XL6_NullableSlotMapKeyV1*)input);
}
const xvalueconverter* xl6NullableSlotMapKeyConverterV1(void)
{static const xvalueconverter c={.ToTyped=toTyped,.FromTyped=fromTyped};return &c;}
const XL6_NativeCollectionValueProfileV1* xl6NullableSlotMapKeyCollectionProfileV1Get(void)
{
    static const XL6_NativeCollectionValueProfileV1 p={xl6NullableSlotMapKeyTypeV1,
        XL6_NULLABLE_SLOT_MAP_KEY_LANGUAGE_VALUE_ID,XL6_SLOT_MAP_KEY_LANGUAGE_VALUE_ID,
        content,empty,copy,slot,value,xl6NullableSlotMapKeyConverterV1};return &p;
}
xvalue* xl6SequenceNullableSlotMapKeyCreateV1(unsigned int role,uint64_t identity)
{return xl6SequenceNativeValueCreateV1(role,identity,xl6NullableSlotMapKeyCollectionProfileV1Get());}
