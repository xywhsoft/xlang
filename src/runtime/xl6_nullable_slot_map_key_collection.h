#ifndef XL6_NULLABLE_SLOT_MAP_KEY_COLLECTION_H
#define XL6_NULLABLE_SLOT_MAP_KEY_COLLECTION_H
#include "xl6_array_native_value_policy.h"
#include "xl6_nullable_slot_map_key.h"
XL6_TEXT_API const XL6_NativeCollectionValueProfileV1* xl6NullableSlotMapKeyCollectionProfileV1Get(void);
XL6_TEXT_API const xvalueconverter* xl6NullableSlotMapKeyConverterV1(void);
XL6_TEXT_API xvalue* xl6SequenceNullableSlotMapKeyCreateV1(unsigned int,uint64_t);
#endif
