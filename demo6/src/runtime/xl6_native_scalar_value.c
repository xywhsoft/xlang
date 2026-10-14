#include "src/runtime/xl6_native_scalar_profile.h"

bool xl6ValueReadNativeScalarV1(const xvalue* source, unsigned int profile, uint64_t identity, void* output)
{
    const xrttype* type = xl6NativeScalarTypeV1(profile);
    if (!type || !identity || !output || type->Size > sizeof(uint64_t)) {
        xrtSetErrorKind(XERR_ARGUMENT); return false;
    }
    if (!xl6CScalarValueV1(type, identity, source)) { xrtSetErrorKind(XERR_TYPE); return false; }
    XL6_NativeScalarScratchV1 temporary = {0};
    if (!xrtValueToTyped(source, type, &temporary, NULL)) return false;
    memcpy(output, &temporary, type->Size); return true;
}
bool xl6ValueAdmitsNativeScalarV1(const xvalue* source,unsigned int profile,uint64_t identity,bool nullable)
{
    const xrttype* type=xl6NativeScalarTypeV1(profile);
    return source && identity && type && type->Size<=sizeof(uint64_t) &&
        (xrtValueType(source)==XVALUE_NULL ? nullable : xl6CScalarValueV1(type,identity,source));
}
bool xl6ValueTryReadNativeScalarV1(const xvalue* source,unsigned profile,uint64_t identity,bool* present,void* output)
{
    const xrttype* type=xl6NativeScalarTypeV1(profile);
    if(!source || !present || !output || !identity || !type || type->Size>sizeof(XL6_NativeScalarScratchV1)) {
        xrtSetErrorKind(XERR_ARGUMENT);return false;
    }
    if(!xl6ValueAdmitsNativeScalarV1(source,profile,identity,false)) { *present=false;return true; }
    if(!xl6ValueReadNativeScalarV1(source,profile,identity,output))return false;
    *present=true;return true;
}
xvalue* xl6ValueBoxNativeScalarV1(const void* source, unsigned int profile, uint64_t identity)
{
    const xrttype* type = xl6NativeScalarTypeV1(profile);
    if (!type || !identity || !source || type->Size > sizeof(uint64_t)) {
        xrtSetErrorKind(XERR_ARGUMENT); return NULL;
    }
    if (!xl6CScalarSlotV1(type, source)) { xrtSetErrorKind(XERR_TYPE); return NULL; }
    XL6_NativeScalarScratchV1 temporary = {0}; memcpy(&temporary, source, type->Size);
    xvalue* value = xrtValueFromTyped(type, &temporary, NULL);
    if (!value || xrtValueType(value) == XVALUE_BOOL || xrtValueTypeIdBind(value, identity)) return value;
    xerror* error = xrtTakeError(); xrtValueRelease(value); xrtClearError(); xrtSetErrorTake(error); return NULL;
}
