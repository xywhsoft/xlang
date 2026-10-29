#define XL6_NULLABLE_CARRIER XL6_NullableU64V1
#define XL6_NULLABLE_ELEMENT xrtTypeUInt64
#define XL6_NULLABLE_GET xl6NullableU64TypeV1
#define XL6_NULLABLE_COMPARE xl6NullableU64CompareV1
#define XL6_NULLABLE_HASH xl6NullableU64HashV1
#define XL6_NULLABLE_NAME "nullable<u64>"
#include "src/runtime/xl6_scalar_type_abi.h"
#define XL6_NULLABLE_ABI XL6_NULLABLE_U64_ABI
#define XL6_NULLABLE_ID XL6_NULLABLE_U64_ID
#include "src/runtime/xl6_nullable_scalar_impl.h"
