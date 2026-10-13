#include "src/runtime/xl6_stack_erased.h"
#include "src/runtime/xl6_array_reference_value.h"
#include "src/runtime/xl6_sequence_projection.h"
#include "src/api/xl_foreign_error.h"

static bool enter(xl6_error_scope* scope)
{
    if (xl6ErrorScopeBegin(scope)) return true;
    xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});
    return false;
}
static XL6_Sequence* begin(const xvalue* value, xl6_error_scope* scope,
    const xvalueconverter** converter)
{
    XL6_Sequence* owner=NULL;
    XL6_SequenceLayoutV1 layout;
    int status=xl6SequenceBeginErasedRoleV1(value,XL6_SEQUENCE_STACK_V1,
        &owner,&layout,converter);
    if (status!=1) {
        if (!status && !scope->failed) xrtSetErrorKind(XERR_TYPE);
        xl6ErrorScopeCaptureFailure(scope);
    }
    return owner;
}
static xvalue* finish(xl6_error_scope* scope, xvalue* result)
{
    if (!result || xrtGetError()) xl6ErrorScopeCaptureFailure(scope);
    if (scope->failed) {
        xrtValueRelease(result); result=NULL;
        if (xrtGetError()) xl6ErrorScopeCaptureFailure(scope);
    }
    xl6_error_result outcome=xl6ErrorScopeEnd(scope);
    if (!outcome.ok) xl6ErrorScopePropagateFailure(outcome);
    return result;
}
xvalue* xl6StackEraseV1(const xvalue* source, uint64_t identity,
    uint64_t elementIdentity, unsigned int transfer, size_t size,
    const xrttype* type, const xvalueconverter* converter)
{
    xl6_error_scope scope={0};
    if (!enter(&scope)) return NULL;
    xvalue* result=NULL;
    if (transfer==3 && converter) xrtSetErrorKind(XERR_ARGUMENT);
    else {
        XL6_Sequence* owner=xl6SequenceBeginRoleV1(source,XL6_SEQUENCE_STACK_V1,
            identity,elementIdentity,transfer,size,type);
        if (owner) {
            const xvalueconverter* policy=transfer==3
                ? xl6ArrayReferenceConverterV1() : converter;
            if (!scope.failed && xl6SequenceBindProjectionRoleV1(owner,XL6_SEQUENCE_STACK_V1,policy))
                result=xrtValueRetain(source);
            xl6SequenceEndV1(owner);
        }
    }
    return finish(&scope,result);
}
bool xl6StackErasedPushV1(const xvalue* source, const xvalue* item)
{
    xl6_error_scope scope={0};
    if (!enter(&scope)) return false;
    bool ok=false;
    const xvalueconverter* converter=NULL;
    XL6_Sequence* owner=begin(source,&scope,&converter);
    if (owner) {
        if (!item) xrtSetErrorKind(XERR_ARGUMENT);
        else if (!scope.failed && xl6SequenceAdmitValueV1(owner,item)) {
            if (!scope.failed) ok=xrtTypedArrayPushValue(xl6SequenceStorageV1(owner),item,converter);
        }
        xl6SequenceEndV1(owner);
    }
    if (!ok || xrtGetError()) xl6ErrorScopeCaptureFailure(&scope);
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    if (!outcome.ok) xl6ErrorScopePropagateFailure(outcome);
    return outcome.ok && ok;
}
xvalue* xl6StackErasedReadV1(const xvalue* source, int64_t depth, bool pop)
{
    xl6_error_scope scope={0};
    if (!enter(&scope)) return NULL;
    xvalue* result=NULL;
    const xvalueconverter* converter=NULL;
    XL6_Sequence* owner=begin(source,&scope,&converter);
    if (owner) {
        xtypedarray* storage=xl6SequenceStorageV1(owner);
        size_t count=xrtTypedArrayCount(storage);
        if (depth<0 || (uint64_t)depth>=count) xrtSetErrorKind(XERR_RANGE);
        else if (pop && depth!=0) xrtSetErrorKind(XERR_ARGUMENT);
        else if (!scope.failed) {
            size_t at=count-1-(size_t)depth;
            result=xl6SequenceProjectionTakeV1(owner,xrtTypedArrayGetValue(storage,at,converter));
            if (!result || xrtGetError()) xl6ErrorScopeCaptureFailure(&scope);
            if (result && pop && !scope.failed && !xrtTypedArrayRemove(storage,at,1))
                xl6ErrorScopeCaptureFailure(&scope);
        }
        xl6SequenceEndV1(owner);
    }
    return finish(&scope,result);
}
