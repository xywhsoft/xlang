#define XL6_ATOMIC_FAMILY Int64
#define XL6_ATOMIC_WIDTH 64
#define XL6_ATOMIC_STORAGE xatomic64
#define XL6_ATOMIC_VALUE int64_t
#define XL6_ATOMIC_BITS uint64
#define XL6_ATOMIC_VALIDATE xl6AtomicAllI64
#define XL6_ATOMIC_DECODE xl6AtomicDecodeSigned64
#define XL6_ATOMIC_PRODUCT XL6_ProductBoolI64V1
#define XL6_ATOMIC_INTEGER 1
#define XL6_ATOMIC_POLICY xl6AtomicInt64HandlePolicyV1Get
#include "xl6_atomic_impl.h"
