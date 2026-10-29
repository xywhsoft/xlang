#define XL6_TLS_ENUM_BEGIN(Type,Id,Count) static const XL6_IrNumericEnumMemberContract xl6Ir##Id##Members[]={
#define XL6_TLS_ENUM_ITEM(Name,Value) {#Name,Value,0},
#define XL6_TLS_ENUM_END(Type,Id,Count) };static const XL6_IrNumericEnumContract xl6Ir##Id##Contract={#Type,XL6_IR_C_ABI_I64,XL6_IR_INTEGER_TARGET_SIGNED_64,xl6Ir##Id##Members,Count};
#include "src/abi/xl6_tls_numeric_enums.inc"
#undef XL6_TLS_ENUM_BEGIN
#undef XL6_TLS_ENUM_ITEM
#undef XL6_TLS_ENUM_END
