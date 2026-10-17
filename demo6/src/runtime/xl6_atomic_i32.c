#define XL6_ATOMIC_FAMILY Int32
#define XL6_ATOMIC_WIDTH 32
#define XL6_ATOMIC_STORAGE xatomic32
#define XL6_ATOMIC_VALUE int64_t
#define XL6_ATOMIC_BITS uint32
#define XL6_ATOMIC_VALIDATE xl6AtomicSigned32
#define XL6_ATOMIC_DECODE xl6AtomicDecodeSigned32
#define XL6_ATOMIC_PRODUCT XL6_ProductBoolI32V1
#define XL6_ATOMIC_INTEGER 1
#define XL6_ATOMIC_POLICY xl6AtomicInt32HandlePolicyV1Get
#include "xl6_atomic_impl.h"
