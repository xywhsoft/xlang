#include "src/runtime/xl6_dict_value_index.h"
#include "src/runtime/xl6_dict.h"
#include "src/api/xl_foreign_error.h"

static xvalue* read(const xvalue* source,xstrview key,bool safe,bool required)
{
    xl6_error_scope scope={0};
    if (!xl6ErrorScopeBegin(&scope)) {
        xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()}); return NULL;
    }
    xvalue* result=NULL; bool found=false,success=false;
    if (key.Size && !key.Data) xrtSetErrorKind(XERR_ARGUMENT);
    else if (safe && (!source || xrtValueType(source)==XVALUE_NULL)) success=true;
    else if (!source) xrtSetErrorKind(XERR_ARGUMENT);
    else {
        int native=xl6DictErasedProbeV1(source);
        if (native>0) success=xl6DictErasedReadV1(source,key,false,&result,&found);
        else if (!native && xrtValueType(source)==XVALUE_OBJECT) {
            const xvalue* item=xrtValueObjectGet(source,key);
            found=item!=NULL; result=item ? xrtValueRetain(item) : NULL;
            success=!found || result!=NULL;
        } else if (!native) xrtSetErrorKind(XERR_TYPE);
    }
    if (success && !found && !scope.failed && !xrtGetError()) {
        if (required) { xrtSetErrorKind(XERR_RANGE); success=false; }
        else result=xrtValueRetain(xrtValueNull());
    }
    if (!success || !result || scope.failed || xrtGetError()) {
        xl6ErrorScopeCaptureFailure(&scope); xrtValueRelease(result); result=NULL;
    }
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    if (!outcome.ok) xl6ErrorScopePropagateFailure(outcome);
    return result;
}
xvalue* xl6DictValueReadCopyV1(const xvalue* source,xstrview key)
{ return read(source,key,false,false); }
xvalue* xl6DictValueReadOrNullCopyV1(const xvalue* source,xstrview key)
{ return read(source,key,true,false); }
xvalue* xl6DictValueReadRequiredCopyV1(const xvalue* source,xstrview key)
{ return read(source,key,false,true); }

bool xl6DictValueSetTakeV1(xvalue* source,xstrview key,xvalue* item)
{
    xl6_error_scope scope={0};
    if (!xl6ErrorScopeBegin(&scope)) {
        xerror* primary=xrtTakeError();
        xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtErrorRef(primary)});
        xrtClearError(); xrtValueRelease(item); xrtClearError(); xrtSetErrorTake(primary);
        return false;
    }
    bool success=false; xvalue* receiver=NULL; xvalue* retired=NULL;
    if (!source || !item || (key.Size && !key.Data)) xrtSetErrorKind(XERR_ARGUMENT);
    else {
        int native=xl6DictErasedProbeV1(source);
        if (native>0) success=xl6DictErasedSetV1(source,key,item);
        else if (!native && xrtValueType(source)==XVALUE_OBJECT) {
            receiver=xrtValueRetain(source);
            if (receiver) {
                const xvalue* old=xrtValueObjectGet(source,key);
                if (!xrtGetError() && old) retired=xrtValueRetain(old);
                if (!xrtGetError() && (!old || retired))
                    success=xrtValueObjectSetTake(source,key,&item);
            }
        } else if (!native) xrtSetErrorKind(XERR_TYPE);
    }
    if (!success || xrtGetError()) xl6ErrorScopeCaptureFailure(&scope);
    xrtValueRelease(item);
    if (xrtGetError()) xl6ErrorScopeCaptureFailure(&scope);
    xrtValueRelease(retired);
    if (xrtGetError()) xl6ErrorScopeCaptureFailure(&scope);
    xrtValueRelease(receiver);
    if (xrtGetError()) xl6ErrorScopeCaptureFailure(&scope);
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    if (!outcome.ok) xl6ErrorScopePropagateFailure(outcome);
    return outcome.ok;
}
