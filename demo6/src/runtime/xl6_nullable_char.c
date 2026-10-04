#define XL6_NULLABLE_CARRIER XL6_NullableCharV1
#define XL6_NULLABLE_ELEMENT xrtTypeUInt32
#define XL6_NULLABLE_GET xl6NullableCharTypeV1
#define XL6_NULLABLE_COMPARE xl6NullableCharCompareV1
#define XL6_NULLABLE_HASH xl6NullableCharHashV1
#define XL6_NULLABLE_NAME "nullable<char>"
#include "src/runtime/xl6_scalar_type_abi.h"
#define XL6_NULLABLE_ABI XL6_NULLABLE_CHAR_ABI
#define XL6_NULLABLE_ID XL6_NULLABLE_CHAR_ID
#include "src/runtime/xl6_nullable_scalar_impl.h"
