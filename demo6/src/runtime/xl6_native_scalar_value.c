#include "src/runtime/xl6_native_scalar_value.h"
#include "src/runtime/xl6_c_scalar_domain.h"
#include <wchar.h>

static const xrttype* integerType(size_t size, bool signedValue)
{
    switch (size) {
    case 1: return signedValue ? xrtTypeInt8() : xrtTypeUInt8();
    case 2: return signedValue ? xrtTypeInt16() : xrtTypeUInt16();
    case 4: return signedValue ? xrtTypeInt32() : xrtTypeUInt32();
    case 8: return signedValue ? xrtTypeInt64() : xrtTypeUInt64();
    default: return NULL;
    }
}
static const xrttype* profileType(unsigned int profile)
{
    switch (profile) {
    case XL6_NATIVE_SCALAR_CHAR: return integerType(sizeof(char), CHAR_MIN < 0);
    case XL6_NATIVE_SCALAR_SCHAR: return integerType(sizeof(signed char), true);
    case XL6_NATIVE_SCALAR_UCHAR: return integerType(sizeof(unsigned char), false);
    case XL6_NATIVE_SCALAR_SHORT: return integerType(sizeof(short), true);
    case XL6_NATIVE_SCALAR_USHORT: return integerType(sizeof(unsigned short), false);
    case XL6_NATIVE_SCALAR_INT: return integerType(sizeof(int), true);
    case XL6_NATIVE_SCALAR_UINT: return integerType(sizeof(unsigned int), false);
    case XL6_NATIVE_SCALAR_LONG: return integerType(sizeof(long), true);
    case XL6_NATIVE_SCALAR_ULONG: return integerType(sizeof(unsigned long), false);
    case XL6_NATIVE_SCALAR_LONGLONG: return integerType(sizeof(long long), true);
    case XL6_NATIVE_SCALAR_ULONGLONG: return integerType(sizeof(unsigned long long), false);
    case XL6_NATIVE_SCALAR_SIZE: return integerType(sizeof(size_t), false);
    case XL6_NATIVE_SCALAR_SSIZE: case XL6_NATIVE_SCALAR_PTRDIFF:
        return integerType(sizeof(ptrdiff_t), true);
    case XL6_NATIVE_SCALAR_INTPTR: return integerType(sizeof(intptr_t), true);
    case XL6_NATIVE_SCALAR_UINTPTR: return integerType(sizeof(uintptr_t), false);
    case XL6_NATIVE_SCALAR_BOOL: return xrtTypeBool();
    case XL6_NATIVE_SCALAR_FLOAT: return xrtTypeFloat32();
    case XL6_NATIVE_SCALAR_DOUBLE: return xrtTypeFloat64();
    case XL6_NATIVE_SCALAR_WCHAR: return integerType(sizeof(wchar_t), WCHAR_MIN < 0);
    case XL6_NATIVE_SCALAR_TYPE_ID: return xrtTypeType();
    default: return NULL;
    }
}
/* Builtin descriptors require naturally aligned storage. The caller's physical
 * native slot need not be aligned, and failed conversions cannot alter it. */
/* Each builtin target has an actual union member. An aligned character array
 * alone is not permission to write through an unrelated scalar lvalue. */
typedef union NativeScratch {
    bool boolean; int8_t i8; uint8_t u8; int16_t i16; uint16_t u16;
    int32_t i32; uint32_t u32; int64_t i64; uint64_t u64; float f32; double f64;
} NativeScratch;
_Static_assert(CHAR_BIT == 8 && sizeof(float) == 4 && sizeof(double) == 8,
    "Native scalar profiles require the supported XRT builtin physical ABI");

bool xl6ValueReadNativeScalarV1(const xvalue* source, unsigned int profile, uint64_t identity, void* output)
{
    const xrttype* type = profileType(profile);
    if (!type || !identity || !output || type->Size > sizeof(uint64_t)) {
        xrtSetErrorKind(XERR_ARGUMENT); return false;
    }
    if (!xl6CScalarValueV1(type, identity, source)) { xrtSetErrorKind(XERR_TYPE); return false; }
    NativeScratch temporary = {0};
    if (!xrtValueToTyped(source, type, &temporary, NULL)) return false;
    memcpy(output, &temporary, type->Size); return true;
}
bool xl6ValueAdmitsNativeScalarV1(const xvalue* source,unsigned int profile,uint64_t identity,bool nullable)
{
    const xrttype* type=profileType(profile);
    return source && identity && type && type->Size<=sizeof(uint64_t) &&
        (xrtValueType(source)==XVALUE_NULL ? nullable : xl6CScalarValueV1(type,identity,source));
}
xvalue* xl6ValueBoxNativeScalarV1(const void* source, unsigned int profile, uint64_t identity)
{
    const xrttype* type = profileType(profile);
    if (!type || !identity || !source || type->Size > sizeof(uint64_t)) {
        xrtSetErrorKind(XERR_ARGUMENT); return NULL;
    }
    if (!xl6CScalarSlotV1(type, source)) { xrtSetErrorKind(XERR_TYPE); return NULL; }
    NativeScratch temporary = {0}; memcpy(&temporary, source, type->Size);
    xvalue* value = xrtValueFromTyped(type, &temporary, NULL);
    if (!value || xrtValueType(value) == XVALUE_BOOL || xrtValueTypeIdBind(value, identity)) return value;
    xerror* error = xrtTakeError(); xrtValueRelease(value); xrtClearError(); xrtSetErrorTake(error); return NULL;
}
