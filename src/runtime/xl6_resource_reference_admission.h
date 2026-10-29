#ifndef XL6_RESOURCE_REFERENCE_ADMISSION_H
#define XL6_RESOURCE_REFERENCE_ADMISSION_H
#include "xl6_sequence.h"
/* Metadata-only ARRAY/STACK reference domains; consumes the element witness.
 * No Channel/SlotMap instance, capacity, slots, key instance or generated
 * callback is kept. The immutable resident predicate checks the full schema. */
XL6_TEXT_API xvalue* xl6SequenceChannelCreateTakeV1(unsigned int,uint64_t,uint64_t,uint64_t,xvalue*,bool);
XL6_TEXT_API xvalue* xl6SequenceSlotMapCreateTakeV1(unsigned int,uint64_t,uint64_t,uint64_t,xvalue*,bool);
#endif
