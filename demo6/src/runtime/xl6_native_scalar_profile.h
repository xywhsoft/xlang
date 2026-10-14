#ifndef XL6_NATIVE_SCALAR_PROFILE_H
#define XL6_NATIVE_SCALAR_PROFILE_H
#include "xl6_native_scalar_value.h"
#include "xl6_c_scalar_domain.h"
#include <wchar.h>
/* Each descriptor writes a real union member. Unaligned slots are copied
 * here; callers publish bytes only after complete checked success. */
typedef union XL6_NativeScalarScratchV1 {
    bool boolean;int8_t i8;uint8_t u8;int16_t i16;uint16_t u16;
    int32_t i32;uint32_t u32;int64_t i64;uint64_t u64;float f32;double f64;
} XL6_NativeScalarScratchV1;
_Static_assert(CHAR_BIT==8 && sizeof(float)==4 && sizeof(double)==8,
    "Native scalar profiles require the supported XRT physical ABI");
static inline const xrttype* xl6NativeIntegerTypeV1(size_t size,bool signedValue)
{
    switch(size) {
    case 1:return signedValue ? xrtTypeInt8() : xrtTypeUInt8();
    case 2:return signedValue ? xrtTypeInt16() : xrtTypeUInt16();
    case 4:return signedValue ? xrtTypeInt32() : xrtTypeUInt32();
    case 8:return signedValue ? xrtTypeInt64() : xrtTypeUInt64();
    default:return NULL;
    }
}
static inline const xrttype* xl6NativeScalarTypeV1(unsigned profile)
{
    switch(profile) {
    case XL6_NATIVE_SCALAR_CHAR:return xl6NativeIntegerTypeV1(sizeof(char),CHAR_MIN<0);
    case XL6_NATIVE_SCALAR_SCHAR:return xl6NativeIntegerTypeV1(sizeof(signed char),true);
    case XL6_NATIVE_SCALAR_UCHAR:return xl6NativeIntegerTypeV1(sizeof(unsigned char),false);
    case XL6_NATIVE_SCALAR_SHORT:return xl6NativeIntegerTypeV1(sizeof(short),true);
    case XL6_NATIVE_SCALAR_USHORT:return xl6NativeIntegerTypeV1(sizeof(unsigned short),false);
    case XL6_NATIVE_SCALAR_INT:return xl6NativeIntegerTypeV1(sizeof(int),true);
    case XL6_NATIVE_SCALAR_UINT:return xl6NativeIntegerTypeV1(sizeof(unsigned int),false);
    case XL6_NATIVE_SCALAR_LONG:return xl6NativeIntegerTypeV1(sizeof(long),true);
    case XL6_NATIVE_SCALAR_ULONG:return xl6NativeIntegerTypeV1(sizeof(unsigned long),false);
    case XL6_NATIVE_SCALAR_LONGLONG:return xl6NativeIntegerTypeV1(sizeof(long long),true);
    case XL6_NATIVE_SCALAR_ULONGLONG:return xl6NativeIntegerTypeV1(sizeof(unsigned long long),false);
    case XL6_NATIVE_SCALAR_SIZE:return xl6NativeIntegerTypeV1(sizeof(size_t),false);
    case XL6_NATIVE_SCALAR_SSIZE:case XL6_NATIVE_SCALAR_PTRDIFF:return xl6NativeIntegerTypeV1(sizeof(ptrdiff_t),true);
    case XL6_NATIVE_SCALAR_INTPTR:return xl6NativeIntegerTypeV1(sizeof(intptr_t),true);
    case XL6_NATIVE_SCALAR_UINTPTR:return xl6NativeIntegerTypeV1(sizeof(uintptr_t),false);
    case XL6_NATIVE_SCALAR_BOOL:return xrtTypeBool();
    case XL6_NATIVE_SCALAR_FLOAT:return xrtTypeFloat32();
    case XL6_NATIVE_SCALAR_DOUBLE:return xrtTypeFloat64();
    case XL6_NATIVE_SCALAR_WCHAR:return xl6NativeIntegerTypeV1(sizeof(wchar_t),WCHAR_MIN<0);
    case XL6_NATIVE_SCALAR_TYPE_ID:return xrtTypeType();
    default:return NULL;
    }
}
#endif
