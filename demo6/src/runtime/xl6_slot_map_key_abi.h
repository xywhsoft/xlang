#ifndef XL6_SLOT_MAP_KEY_ABI_H
#define XL6_SLOT_MAP_KEY_ABI_H
#include <stdbool.h>
#include <stdint.h>

/* Immutable owning value. The instance is opaque and independent of the map.
 * Only genuine runtime producers may create a nonempty instance/slot pair.
 * A C assignment observes/transfers the carrier; it does not acquire an owner. */
typedef struct XL6_SlotMapInstanceV1 XL6_SlotMapInstanceV1;
typedef struct XL6_SlotMapKeyV1 {
    XL6_SlotMapInstanceV1* instance;
    uint64_t slot;
} XL6_SlotMapKeyV1;
typedef struct xlir_result_slot_map_key_v1 {
    bool ok;
    XL6_SlotMapKeyV1 value;
} xlir_result_slot_map_key_v1;

/* Copy observes source and publishes one independent owner into empty output.
 * False is an outcome even if no error object could be materialized. Drop
 * consumes its carrier by value; the caller detaches its owning slot first. */
bool xl6SlotMapKeyCopyValueV1(XL6_SlotMapKeyV1, XL6_SlotMapKeyV1*);
void xl6SlotMapKeyDropValueV1(XL6_SlotMapKeyV1);
bool xl6SlotMapKeyEqualV1(XL6_SlotMapKeyV1, XL6_SlotMapKeyV1);
/* Owned return over the standard C record ABI; input is only observed. */
XL6_SlotMapKeyV1 xl6SlotMapKeyCloneV1(XL6_SlotMapKeyV1);
struct xvalue;
struct xvalue* xl6SlotMapKeyBoxV1(XL6_SlotMapKeyV1);
bool xl6SlotMapKeyUnboxV1(const struct xvalue*, XL6_SlotMapKeyV1*);
struct xrttype;
struct xvalueconverter;
const struct xrttype* xl6SlotMapKeyTypeV1(void);
const struct xvalueconverter* xl6SlotMapKeyConverterV1(void);
struct xvalue* xl6SequenceSlotMapKeyCreateV1(unsigned int, uint64_t);
struct XL6_NativeLeafValueProfileV1;
const struct XL6_NativeLeafValueProfileV1* xl6SlotMapKeyValueProfileV1Get(void);
#endif
