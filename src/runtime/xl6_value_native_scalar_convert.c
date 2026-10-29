#include "src/runtime/xl6_native_scalar_profile.h"
#include "src/api/xl_foreign_error.h"
/* Numeric tags exclude reflection's UInt carrier by verified identity. */
bool xl6ValueConvertNativeScalarV2(const xvalue* source,unsigned targetProfile,uint64_t reflectionIdentity,void* output)
{
    xl6_error_scope scope={0};XL6_NativeScalarScratchV1 input={0};bool ok=false;unsigned profile=0;
    if(!xl6ErrorScopeBegin(&scope)) { xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});return false; }
    if(!source || !output || !reflectionIdentity || !targetProfile || targetProfile>XL6_NATIVE_SCALAR_WCHAR)xrtSetErrorKind(XERR_ARGUMENT);
    else {
        xvaluetype tag=xrtValueType(source);
        if(tag==XVALUE_INVALID)xl6ErrorScopeCaptureFailure(&scope);
        else if(xrtValueTypeId(source)==reflectionIdentity)xrtSetErrorKind(XERR_TYPE);
        else switch(tag) {
        case XVALUE_BOOL:profile=XL6_SCALAR_INPUT_BOOL;ok=xrtValueGetBool(source,&input.boolean);break;
        case XVALUE_INT:profile=XL6_SCALAR_INPUT_I64;ok=xrtValueGetInt(source,&input.i64);break;
        case XVALUE_UINT:profile=XL6_SCALAR_INPUT_U64;ok=xrtValueGetUInt(source,&input.u64);break;
        case XVALUE_FLOAT:profile=XL6_SCALAR_INPUT_F64;ok=xrtValueGetFloat(source,&input.f64);break;
        case XVALUE_TIME:profile=XL6_SCALAR_INPUT_TIME;ok=xrtValueGetTime(source,&input.i64);break;
        case XVALUE_CHAR:profile=XL6_SCALAR_INPUT_CHAR;ok=xrtValueGetChar(source,&input.u32);break;
        default:xrtSetErrorKind(XERR_TYPE);break;
        }
        if(ok)ok=xl6NativeScalarConvertV1(&input,profile,targetProfile,output);
    }
    if(!ok || xrtGetError())xl6ErrorScopeCaptureFailure(&scope);
    xl6_error_result result=xl6ErrorScopeEnd(&scope);
    if(!result.ok) { xl6ErrorScopePropagateFailure(result);return false; }
    return true;
}
