#include "src/runtime/xl6_native_scalar_profile.h"
#include "src/api/xl_foreign_error.h"
#include <math.h>
static const xrttype* conversionInputType(unsigned input)
{
    if(input>XL6_SCALAR_INPUT_NATIVE_BASE && input<=XL6_SCALAR_INPUT_NATIVE_BASE+XL6_NATIVE_SCALAR_WCHAR)
        return xl6NativeScalarTypeV1(input-XL6_SCALAR_INPUT_NATIVE_BASE);
    switch(input) {
    case XL6_SCALAR_INPUT_BOOL:return xrtTypeBool();
    case XL6_SCALAR_INPUT_I64:return xrtTypeInt64();
    case XL6_SCALAR_INPUT_U64:return xrtTypeUInt64();
    case XL6_SCALAR_INPUT_F32:return xrtTypeFloat32();
    case XL6_SCALAR_INPUT_F64:return xrtTypeFloat64();
    case XL6_SCALAR_INPUT_TIME:return xrtTypeTime();
    case XL6_SCALAR_INPUT_CHAR:return xrtTypeUInt32();
    default:return NULL;
    }
}
/* Preserve direct integer -> binary32 rounding and special floating values. */
static bool explicitNumericConvert(const xrttype* source,const XL6_NativeScalarScratchV1* input,
    const xrttype* target,XL6_NativeScalarScratchV1* output)
{
    if(target==xrtTypeFloat32()) {
        if(source==xrtTypeInt64() || source==xrtTypeTime()) { output->f32=(float)input->i64;return true; }
        if(source==xrtTypeUInt64()) { output->f32=(float)input->u64;return true; }
        if(source==xrtTypeFloat64() && isinf(input->f64)) {
            output->f32=signbit(input->f64) ? -INFINITY : INFINITY;return true;
        }
    }
    return xrtTypeConvert(source,input,target,output,XTYPE_CONVERT_EXPLICIT);
}
bool xl6NativeScalarConvertV1(const void* source,unsigned inputProfile,unsigned targetProfile,void* output)
{
    xl6_error_scope scope={0};XL6_NativeScalarScratchV1 input={0},value={0};bool ok=false;
    if(!xl6ErrorScopeBegin(&scope)) { xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});return false; }
    const xrttype* from=conversionInputType(inputProfile);
    const xrttype* to=targetProfile && targetProfile<=XL6_NATIVE_SCALAR_WCHAR ? xl6NativeScalarTypeV1(targetProfile) : NULL;
    if(!source || !output || !from || !to || from->Size>sizeof(input) || to->Size>sizeof(value))xrtSetErrorKind(XERR_ARGUMENT);
    else if(!xl6CScalarSlotV1(from,source))xrtSetErrorKind(XERR_TYPE);
    else {
        memcpy(&input,source,from->Size);
        if(inputProfile==XL6_SCALAR_INPUT_CHAR && !xl6CharDomainV1(input.u32))xrtSetErrorKind(XERR_TYPE);
        else ok=explicitNumericConvert(from,&input,to,&value);
    }
    if(!ok || xrtGetError())xl6ErrorScopeCaptureFailure(&scope);
    xl6_error_result result=xl6ErrorScopeEnd(&scope);
    if(!result.ok) { xl6ErrorScopePropagateFailure(result);return false; }
    memcpy(output,&value,to->Size);return true;
}
