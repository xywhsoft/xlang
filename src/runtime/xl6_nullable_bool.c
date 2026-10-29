#define XL6_NULLABLE_CARRIER XL6_NullableBoolV1
#define XL6_NULLABLE_ELEMENT xrtTypeBool
#define XL6_NULLABLE_GET xl6NullableBoolTypeV1
#define XL6_NULLABLE_COMPARE xl6NullableBoolCompareV1
#define XL6_NULLABLE_HASH xl6NullableBoolHashV1
#define XL6_NULLABLE_NAME "nullable<bool>"
#include "src/runtime/xl6_scalar_type_abi.h"
#define XL6_NULLABLE_ABI XL6_NULLABLE_BOOL_ABI
#define XL6_NULLABLE_ID XL6_NULLABLE_BOOL_ID
#include "src/runtime/xl6_nullable_scalar_impl.h"
