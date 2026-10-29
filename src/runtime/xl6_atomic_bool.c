#define XL6_ATOMIC_FAMILY Bool
#define XL6_ATOMIC_WIDTH 32
#define XL6_ATOMIC_STORAGE xatomic32
#define XL6_ATOMIC_VALUE bool
#define XL6_ATOMIC_BITS uint32
#define XL6_ATOMIC_VALIDATE xl6AtomicAllBool
#define XL6_ATOMIC_DECODE(Value) ((bool)(Value))
#define XL6_ATOMIC_PRODUCT XL6_ProductBoolBoolV1
#define XL6_ATOMIC_INTEGER 0
#define XL6_ATOMIC_POLICY xl6AtomicBoolHandlePolicyV1Get
#include "xl6_atomic_impl.h"
