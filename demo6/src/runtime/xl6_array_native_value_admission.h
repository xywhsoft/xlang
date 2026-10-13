#ifndef XL6_ARRAY_NATIVE_VALUE_ADMISSION_H
#define XL6_ARRAY_NATIVE_VALUE_ADMISSION_H
#include "xl6_sequence.h"
#include "xl6_native_collection_value.h"
/* Exact immutable SlotMapKey value schema. Actual owning records, no boxing
 * at native insert/read boundaries, no ordering, no generated descriptor.
 * Growth produces the valid empty key. Graph admission is an independent
 * capability: resolvers must explicitly authorize the resident ops below. */
XL6_TEXT_API xvalue* xl6SequenceSlotMapKeyCreateV1(unsigned int,uint64_t);
XL6_TEXT_API const XL6_SequenceAdmissionOpsV2* xl6ArraySlotMapKeyAdmissionOpsV2Get(void);
XL6_TEXT_API const XL6_NativeCollectionValueProfileV1* xl6SlotMapKeyCollectionProfileV1Get(void);
#endif
