#define XL6_ATOMIC_FAMILY UInt32
#define XL6_ATOMIC_WIDTH 32
#define XL6_ATOMIC_STORAGE xatomic32
#define XL6_ATOMIC_VALUE int64_t
#define XL6_ATOMIC_BITS uint32
#define XL6_ATOMIC_VALIDATE xl6AtomicUnsigned32
#define XL6_ATOMIC_DECODE(Value) ((int64_t)(Value))
#define XL6_ATOMIC_PRODUCT XL6_ProductBoolU32V1
#define XL6_ATOMIC_INTEGER 1
#define XL6_ATOMIC_POLICY xl6AtomicUInt32HandlePolicyV1Get
#include "xl6_atomic_impl.h"
