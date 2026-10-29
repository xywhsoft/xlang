#define XL6_NULLABLE_CARRIER XL6_NullableTimeV1
#define XL6_NULLABLE_ELEMENT xrtTypeTime
#define XL6_NULLABLE_GET xl6NullableTimeTypeV1
#define XL6_NULLABLE_COMPARE xl6NullableTimeCompareV1
#define XL6_NULLABLE_HASH xl6NullableTimeHashV1
#define XL6_NULLABLE_NAME "nullable<time>"
#include "src/runtime/xl6_scalar_type_abi.h"
#define XL6_NULLABLE_ABI XL6_NULLABLE_TIME_ABI
#define XL6_NULLABLE_ID XL6_NULLABLE_TIME_ID
#include "src/runtime/xl6_nullable_scalar_impl.h"
