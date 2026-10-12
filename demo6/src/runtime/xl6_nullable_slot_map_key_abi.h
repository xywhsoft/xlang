#ifndef XL6_NULLABLE_SLOT_MAP_KEY_ABI_H
#define XL6_NULLABLE_SLOT_MAP_KEY_ABI_H
#include "xl6_slot_map_key_abi.h"

/* Presence is independent of payload bits. C assignment only observes or
 * transfers an owner; Copy acquires. Absent records have an empty payload. */
typedef struct XL6_NullableSlotMapKeyV1 {
    bool hasValue;
    XL6_SlotMapKeyV1 value;
} XL6_NullableSlotMapKeyV1;

typedef struct xlir_result_nullable_slot_map_key_v1 {
    bool ok;
    XL6_NullableSlotMapKeyV1 value;
} xlir_result_nullable_slot_map_key_v1;

bool xl6NullableSlotMapKeyCopyValueV1(XL6_NullableSlotMapKeyV1,XL6_NullableSlotMapKeyV1*);
void xl6NullableSlotMapKeyDropValueV1(XL6_NullableSlotMapKeyV1);
bool xl6NullableSlotMapKeyEqualV1(XL6_NullableSlotMapKeyV1,XL6_NullableSlotMapKeyV1);
bool xl6NullableSlotMapKeyWrapV1(XL6_SlotMapKeyV1,XL6_NullableSlotMapKeyV1*);
bool xl6NullableSlotMapKeyProjectV1(XL6_NullableSlotMapKeyV1,XL6_SlotMapKeyV1*);
const struct xrttype* xl6NullableSlotMapKeyTypeV1(void);
struct xvalue* xl6NullableSlotMapKeyBoxV1(XL6_NullableSlotMapKeyV1);
bool xl6NullableSlotMapKeyUnboxV1(const struct xvalue*,XL6_NullableSlotMapKeyV1*);
#endif
