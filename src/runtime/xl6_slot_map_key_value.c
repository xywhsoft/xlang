#include "xl6_slot_map_key_value.h"
#include "xl6_native_leaf_value.h"

/* Canonical language `type:SlotMapKey`, distinct from the resident C ABI ID.
 * Immutable leaf: owns only an instance record, never the map or source code. */
#define KEY_VALUE_ID XL6_SLOT_MAP_KEY_LANGUAGE_VALUE_ID
uint64 xl6SlotMapKeyHashV1(XL6_SlotMapKeyV1 key)
{
    /* No struct padding, slot address or text sentinel. Real XRT hash;
     * process-local identity, not a persistent identifier. */
    const uint64 fields[2]={(uint64)(uintptr_t)key.instance,(uint64)key.slot};
    return xrtHash64(fields,sizeof(fields));
}
static uint64 contentHash(const void* key,ptr context)
{ (void)context; return xl6SlotMapKeyHashV1(*(const XL6_SlotMapKeyV1*)key); }
static bool contentEqual(const void* left,const void* right,ptr context)
{ (void)context; return xl6SlotMapKeyEqualV1(*(const XL6_SlotMapKeyV1*)left,*(const XL6_SlotMapKeyV1*)right); }
static const xtypedsetkeypolicy content={contentHash,contentEqual,NULL};
static bool empty(const void* raw)
{ const XL6_SlotMapKeyV1* key=raw; return key && !key->instance && !key->slot; }
static const XL6_NativeLeafValueProfileV1 profile={xl6SlotMapKeyTypeV1,KEY_VALUE_ID,&content,empty};
const XL6_NativeLeafValueProfileV1* xl6SlotMapKeyValueProfileV1Get(void) { return &profile; }
bool xl6SlotMapKeyValueAdmitsV1(const xvalue* value)
{ return xl6NativeLeafValueAdmitsV1(value,&profile); }
const xrtownershipadapterv1* xl6SlotMapKeyValueOwnershipAdapterV1(xrtownershipref ref)
{ return xl6NativeLeafValueOwnershipAdapterV1(ref,&profile); }
xvalue* xl6SlotMapKeyBoxV1(XL6_SlotMapKeyV1 key)
{
    return xl6NativeLeafValueBoxV1(&profile,&key);
}
bool xl6SlotMapKeyUnboxV1(const xvalue* value,XL6_SlotMapKeyV1* output)
{
    return xl6NativeLeafValueUnboxV1(value,&profile,output);
}
static bool toTyped(const xvalue* value,const xrttype* type,ptr output,ptr context)
{
    if (context || type!=xl6SlotMapKeyTypeV1()) { xrtSetErrorKind(XERR_TYPE); return false; }
    return xl6SlotMapKeyUnboxV1(value,output);
}
static xvalue* fromTyped(const xrttype* type,const void* input,ptr context)
{
    if (context || type!=xl6SlotMapKeyTypeV1()) { xrtSetErrorKind(XERR_TYPE); return NULL; }
    if (!input) { xrtSetErrorKind(XERR_ARGUMENT); return NULL; }
    return xl6SlotMapKeyBoxV1(*(const XL6_SlotMapKeyV1*)input);
}
const xvalueconverter* xl6SlotMapKeyConverterV1(void)
{
    static const xvalueconverter converter={.ToTyped=toTyped,.FromTyped=fromTyped};
    return &converter;
}
const xtypedsetkeypolicy* xl6SlotMapKeyContentPolicyV1(void)
{
    return &content;
}
