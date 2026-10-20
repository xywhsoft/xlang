#ifndef XL6_PRODUCT_ABI_H
#define XL6_PRODUCT_ABI_H
#include <stdint.h>
#include <stdbool.h>
/* Closed primitive product ABI, shared by native declarations and generated
 * C. Field order is the source multi-result order. No pointers/owners/padding
 * semantics are hidden in the carrier. Extend profiles explicitly, never by
 * guessing a foreign symbol or assigning a per-module TypeId spelling. */
#ifndef XL6_PRODUCT_ABI_I64X3_V1
#define XL6_PRODUCT_ABI_I64X3_V1
typedef struct XL6_ProductI64x3V1 { int64_t e0; int64_t e1; int64_t e2; } XL6_ProductI64x3V1;
#endif
/* Canonical tagged scalar carriers. Language TypeIds (including integer
 * widths and time/char identity) remain distinct; these only fix C layout.
 * Resident immutable descriptors never borrow a generated module's data. */
typedef struct XL6_NullableBoolV1 { bool hasValue; bool value; } XL6_NullableBoolV1;
typedef struct XL6_NullableI64V1 { bool hasValue; int64_t value; } XL6_NullableI64V1;
typedef struct XL6_NullableU64V1 { bool hasValue; uint64_t value; } XL6_NullableU64V1;
typedef struct XL6_NullableF32V1 { bool hasValue; float value; } XL6_NullableF32V1;
typedef struct XL6_NullableF64V1 { bool hasValue; double value; } XL6_NullableF64V1;
typedef struct XL6_NullableTimeV1 { bool hasValue; int64_t value; } XL6_NullableTimeV1;
typedef struct XL6_NullableCharV1 { bool hasValue; uint32_t value; } XL6_NullableCharV1;
typedef struct XL6_NullableNativeI8V1 { bool hasValue; int8_t value; } XL6_NullableNativeI8V1;
typedef struct XL6_NullableNativeU8V1 { bool hasValue; uint8_t value; } XL6_NullableNativeU8V1;
typedef struct XL6_NullableNativeI16V1 { bool hasValue; int16_t value; } XL6_NullableNativeI16V1;
typedef struct XL6_NullableNativeU16V1 { bool hasValue; uint16_t value; } XL6_NullableNativeU16V1;
typedef struct XL6_NullableNativeI32V1 { bool hasValue; int32_t value; } XL6_NullableNativeI32V1;
typedef struct XL6_NullableNativeU32V1 { bool hasValue; uint32_t value; } XL6_NullableNativeU32V1;
#ifndef XL6_PRODUCT_ABI_TIMEX2_V1
#define XL6_PRODUCT_ABI_TIMEX2_V1
typedef struct XL6_ProductTimeX2V1 { int64_t e0; int64_t e1; } XL6_ProductTimeX2V1;
#endif
#ifndef XL6_PRODUCT_ABI_BOOLTIME_V1
#define XL6_PRODUCT_ABI_BOOLTIME_V1
typedef struct XL6_ProductBoolTimeV1 { bool e0; int64_t e1; } XL6_ProductBoolTimeV1;
#endif
#ifndef XL6_PRODUCT_ABI_BOOLI64_V1
#define XL6_PRODUCT_ABI_BOOLI64_V1
typedef struct XL6_ProductBoolI64V1 { bool e0; int64_t e1; } XL6_ProductBoolI64V1;
#endif
/* Logical i32/u32 stay distinct while using xlang's normalized I64 carrier. */
#ifndef XL6_PRODUCT_ABI_BOOLI32_V1
#define XL6_PRODUCT_ABI_BOOLI32_V1
typedef struct XL6_ProductBoolI32V1 { bool e0; int64_t e1; } XL6_ProductBoolI32V1;
#endif
#ifndef XL6_PRODUCT_ABI_BOOLU32_V1
#define XL6_PRODUCT_ABI_BOOLU32_V1
typedef struct XL6_ProductBoolU32V1 { bool e0; int64_t e1; } XL6_ProductBoolU32V1;
#endif
#ifndef XL6_PRODUCT_ABI_BOOLU64_V1
#define XL6_PRODUCT_ABI_BOOLU64_V1
typedef struct XL6_ProductBoolU64V1 { bool e0; uint64_t e1; } XL6_ProductBoolU64V1;
#endif
#ifndef XL6_PRODUCT_ABI_BOOLBOOL_V1
#define XL6_PRODUCT_ABI_BOOLBOOL_V1
typedef struct XL6_ProductBoolBoolV1 { bool e0; bool e1; } XL6_ProductBoolBoolV1;
#endif
#ifndef XL6_PRODUCT_ABI_I64U64_V1
#define XL6_PRODUCT_ABI_I64U64_V1
typedef struct XL6_ProductI64U64V1 { int64_t e0; uint64_t e1; } XL6_ProductI64U64V1;
#endif
#endif
