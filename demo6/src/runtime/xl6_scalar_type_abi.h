#ifndef XL6_SCALAR_TYPE_ABI_H
#define XL6_SCALAR_TYPE_ABI_H
#include "src/runtime/xl6_text_value.h"
#include "src/runtime/xl6_value_admission_abi.h"
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
static inline bool xl6ClosedScalarTypeAbiV1(const xrttype* type, bool nullable,
    uint64_t id, const char* abi, size_t size, size_t align)
{
    return type && type->Id==id && type->Kind==(nullable ? XRT_TYPE_OPTIONAL : XRT_TYPE_RECORD) &&
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
