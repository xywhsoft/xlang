#ifndef XL6_SCALAR_TYPE_ABI_H
#define XL6_SCALAR_TYPE_ABI_H
#include "xl6_text_value.h"
#include "xl6_value_admission_abi.h"
#include <string.h>

/* Closed physical metadata, NOT nominal language identities. Inline checks
 * deliberately do not reference optional descriptor factories. An int-only
 * owner must not link all nullable/string/char profiles as a side effect. */
#define XL6_CHAR_ID UINT64_C(0x76D1885EB4BC3548)
#define XL6_CHAR_ABI "xl6.char.v1"
#define XL6_NULLABLE_BOOL_ID UINT64_C(0x982CC6CF0D1F40FE)
#define XL6_NULLABLE_BOOL_ABI "xl6.nullable-bool.v1"
#define XL6_NULLABLE_I64_ID UINT64_C(0x5EB7B3DDC1FB0B57)
#define XL6_NULLABLE_I64_ABI "xl6.nullable-i64.v1"
#define XL6_NULLABLE_U64_ID UINT64_C(0x524609045148C52B)
#define XL6_NULLABLE_U64_ABI "xl6.nullable-u64.v1"
#define XL6_NULLABLE_F32_ID UINT64_C(0x79CB256EAD6A10D9)
#define XL6_NULLABLE_F32_ABI "xl6.nullable-f32.v1"
#define XL6_NULLABLE_F64_ID UINT64_C(0x8B8B1F128F7DA438)
#define XL6_NULLABLE_F64_ABI "xl6.nullable-f64.v1"
#define XL6_NULLABLE_TIME_ID UINT64_C(0xDE9CB1C2DE241F69)
#define XL6_NULLABLE_TIME_ABI "xl6.nullable-time.v1"
#define XL6_NULLABLE_CHAR_ID UINT64_C(0x6CCBB59F9C4D2AA2)
#define XL6_NULLABLE_CHAR_ABI "xl6.nullable-char.v1"
#define XL6_NULLABLE_NATIVE_I8_ID UINT64_C(0xB41942CA1964A165)
#define XL6_NULLABLE_NATIVE_I8_ABI "xl6.nullable-native-i8.v1"
#define XL6_NULLABLE_NATIVE_U8_ID UINT64_C(0xE4DCEAF2745E34F9)
#define XL6_NULLABLE_NATIVE_U8_ABI "xl6.nullable-native-u8.v1"
#define XL6_NULLABLE_NATIVE_I16_ID UINT64_C(0x34E8AC07AE094820)
#define XL6_NULLABLE_NATIVE_I16_ABI "xl6.nullable-native-i16.v1"
#define XL6_NULLABLE_NATIVE_U16_ID UINT64_C(0x69B417DE1A0242F4)
#define XL6_NULLABLE_NATIVE_U16_ABI "xl6.nullable-native-u16.v1"
#define XL6_NULLABLE_NATIVE_I32_ID UINT64_C(0xFF2B879B61EE1F4A)
#define XL6_NULLABLE_NATIVE_I32_ABI "xl6.nullable-native-i32.v1"
#define XL6_NULLABLE_NATIVE_U32_ID UINT64_C(0x2778C75D97D9F596)
#define XL6_NULLABLE_NATIVE_U32_ABI "xl6.nullable-native-u32.v1"
static inline bool xl6ClosedScalarTypeAbiV1(const xrttype* type, bool nullable,
    uint64_t id, const char* abi, size_t size, size_t align)
{
    return type && type->Id==id && type->Kind==(nullable ? XRT_TYPE_OPTIONAL : XRT_TYPE_RECORD) &&
        type->InstanceAlign==1 &&
        type->Size==size && type->Align==align &&
        type->Flags==(XRT_TYPE_FLAG_TRIVIAL_COPY|XRT_TYPE_FLAG_TRIVIAL_DROP|
            XRT_TYPE_FLAG_COPYABLE|XRT_TYPE_FLAG_FINAL|XRT_TYPE_FLAG_RELOCATABLE|
            (nullable ? XRT_TYPE_FLAG_NULLABLE : 0)) &&
        type->AbiName.Data && type->AbiName.Size==strlen(abi) &&
        !memcmp(type->AbiName.Data,abi,type->AbiName.Size) && type->Ops &&
        !type->Ops->Init && !type->Ops->Copy && !type->Ops->Move && !type->Ops->Drop &&
        !type->Ops->Clone && type->Ops->Compare && type->Ops->Hash;
}
static inline bool xl6CharTypeAbiV1(const xrttype* type)
{
    return xl6ClosedScalarTypeAbiV1(type,false,XL6_CHAR_ID,XL6_CHAR_ABI,
        sizeof(uint32_t),_Alignof(uint32_t));
}
/* Shape checks reference no optional factories, preserving per-profile
 * pruning. The private native caller supplies an immutable resident getter
 * result, not a descriptor with module-owned Compare/Hash callbacks. */
static inline bool xl6NullableCScalarTypeAbiV1(const xrttype* type,const xrttype* base)
{
#define XL6_NATIVE_PROFILE(BASE,NAME,CARRIER) \
    if (base==xrtType##BASE()) return xl6ClosedScalarTypeAbiV1(type,true, \
        XL6_NULLABLE_##NAME##_ID,XL6_NULLABLE_##NAME##_ABI,sizeof(CARRIER),_Alignof(CARRIER))
    XL6_NATIVE_PROFILE(Bool,BOOL,XL6_NullableBoolV1);
    XL6_NATIVE_PROFILE(Int8,NATIVE_I8,XL6_NullableNativeI8V1);
    XL6_NATIVE_PROFILE(UInt8,NATIVE_U8,XL6_NullableNativeU8V1);
    XL6_NATIVE_PROFILE(Int16,NATIVE_I16,XL6_NullableNativeI16V1);
    XL6_NATIVE_PROFILE(UInt16,NATIVE_U16,XL6_NullableNativeU16V1);
    XL6_NATIVE_PROFILE(Int32,NATIVE_I32,XL6_NullableNativeI32V1);
    XL6_NATIVE_PROFILE(UInt32,NATIVE_U32,XL6_NullableNativeU32V1);
    XL6_NATIVE_PROFILE(Int64,I64,XL6_NullableI64V1);
    XL6_NATIVE_PROFILE(UInt64,U64,XL6_NullableU64V1);
    XL6_NATIVE_PROFILE(Type,U64,XL6_NullableU64V1);
    XL6_NATIVE_PROFILE(Float32,F32,XL6_NullableF32V1);
    XL6_NATIVE_PROFILE(Float64,F64,XL6_NullableF64V1);
#undef XL6_NATIVE_PROFILE
    return false;
}
static inline bool xl6NullableScalarTypeAbiV1(const xrttype* type, unsigned int rule)
{
    if (rule>=XL6_VALUE_ADMIT_I8 && rule<=XL6_VALUE_ADMIT_U32) rule=XL6_VALUE_ADMIT_I64;
#define XL6_PROFILE(RULE,NAME,CARRIER) \
    case XL6_VALUE_ADMIT_##RULE: return xl6ClosedScalarTypeAbiV1(type,true, \
        XL6_NULLABLE_##NAME##_ID,XL6_NULLABLE_##NAME##_ABI,sizeof(CARRIER),_Alignof(CARRIER))
    switch (rule) {
    XL6_PROFILE(BOOL,BOOL,XL6_NullableBoolV1);
    XL6_PROFILE(I64,I64,XL6_NullableI64V1);
    XL6_PROFILE(U64,U64,XL6_NullableU64V1);
    XL6_PROFILE(F32,F32,XL6_NullableF32V1);
    XL6_PROFILE(F64,F64,XL6_NullableF64V1);
    XL6_PROFILE(TIME,TIME,XL6_NullableTimeV1);
    XL6_PROFILE(CHAR,CHAR,XL6_NullableCharV1);
    default: return false;
    }
#undef XL6_PROFILE
}
#endif
