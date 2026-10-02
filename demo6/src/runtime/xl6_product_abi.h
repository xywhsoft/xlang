#ifndef XL6_PRODUCT_ABI_H
#define XL6_PRODUCT_ABI_H
#include <stdint.h>
/* Closed primitive product ABI, shared by native declarations and generated
 * C. Field order is the source multi-result order. No pointers/owners/padding
 * semantics are hidden in the carrier. Extend profiles explicitly, never by
 * guessing a foreign symbol or assigning a per-module TypeId spelling. */
#ifndef XL6_PRODUCT_ABI_I64X3_V1
#define XL6_PRODUCT_ABI_I64X3_V1
typedef struct XL6_ProductI64x3V1 { int64_t e0; int64_t e1; int64_t e2; } XL6_ProductI64x3V1;
#endif
#ifndef XL6_PRODUCT_ABI_TIMEX2_V1
#define XL6_PRODUCT_ABI_TIMEX2_V1
typedef struct XL6_ProductTimeX2V1 { int64_t e0; int64_t e1; } XL6_ProductTimeX2V1;
#endif
#endif
