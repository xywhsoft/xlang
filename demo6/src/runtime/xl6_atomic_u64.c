#define XL6_ATOMIC_FAMILY UInt64
#define XL6_ATOMIC_WIDTH 64
#define XL6_ATOMIC_STORAGE xatomic64
#define XL6_ATOMIC_VALUE uint64_t
#define XL6_ATOMIC_BITS uint64
#define XL6_ATOMIC_VALIDATE xl6AtomicAllU64
#define XL6_ATOMIC_DECODE(Value) ((uint64_t)(Value))
#define XL6_ATOMIC_PRODUCT XL6_ProductBoolU64V1
#define XL6_ATOMIC_INTEGER 1
#define XL6_ATOMIC_POLICY xl6AtomicUInt64HandlePolicyV1Get
#include "xl6_atomic_impl.h"
