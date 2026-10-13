#ifndef XL6_NULLABLE_SLOT_MAP_KEY_H
#define XL6_NULLABLE_SLOT_MAP_KEY_H
#include "xl6_nullable_slot_map_key_abi.h"
#include "xl6_slot_map_key_value.h"

/* Private optional-record foundation. No language registration is implied. */
XL6_TEXT_API bool xl6NullableSlotMapKeyCopyValueV1(XL6_NullableSlotMapKeyV1,XL6_NullableSlotMapKeyV1*);
XL6_TEXT_API void xl6NullableSlotMapKeyDropValueV1(XL6_NullableSlotMapKeyV1);
XL6_TEXT_API bool xl6NullableSlotMapKeyEqualV1(XL6_NullableSlotMapKeyV1,XL6_NullableSlotMapKeyV1);
XL6_TEXT_API bool xl6NullableSlotMapKeyWrapV1(XL6_SlotMapKeyV1,XL6_NullableSlotMapKeyV1*);
XL6_TEXT_API bool xl6NullableSlotMapKeyProjectV1(XL6_NullableSlotMapKeyV1,XL6_SlotMapKeyV1*);
XL6_TEXT_API const xrttype* xl6NullableSlotMapKeyTypeV1(void);
XL6_TEXT_API xvalue* xl6NullableSlotMapKeyBoxV1(XL6_NullableSlotMapKeyV1);
XL6_TEXT_API bool xl6NullableSlotMapKeyUnboxV1(const xvalue*,XL6_NullableSlotMapKeyV1*);
XL6_TEXT_API bool xl6NullableSlotMapKeyTryUnboxV1(const xvalue*,XL6_NullableSlotMapKeyV1*);
#endif
