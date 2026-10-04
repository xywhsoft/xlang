#define XL6_NULLABLE_CARRIER XL6_NullableF64V1
#define XL6_NULLABLE_ELEMENT xrtTypeFloat64
#define XL6_NULLABLE_GET xl6NullableF64TypeV1
#define XL6_NULLABLE_COMPARE xl6NullableF64CompareV1
#define XL6_NULLABLE_HASH xl6NullableF64HashV1
#define XL6_NULLABLE_NAME "nullable<f64>"
#include "src/runtime/xl6_scalar_type_abi.h"
#define XL6_NULLABLE_ABI XL6_NULLABLE_F64_ABI
#define XL6_NULLABLE_ID XL6_NULLABLE_F64_ID
#include "src/runtime/xl6_nullable_scalar_impl.h"
