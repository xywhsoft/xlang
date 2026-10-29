#ifndef XL6_ARRAY_NATIVE_VALUE_POLICY_H
#define XL6_ARRAY_NATIVE_VALUE_POLICY_H
#include "xl6_sequence.h"
#include "xl6_native_collection_value.h"
/* The owned policy retains only permanent native metadata, never code leases
 * or language Values. Record and optional-record factories share this engine. */
XL6_TEXT_API xvalue* xl6SequenceNativeValueCreateV1(unsigned int,uint64_t,const XL6_NativeCollectionValueProfileV1*);
XL6_TEXT_API const XL6_SequenceAdmissionOpsV2* xl6ArrayNativeValueAdmissionOpsV2Get(void);
#endif
