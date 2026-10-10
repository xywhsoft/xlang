#include "src/runtime/xl6_list_value_write.h"
#include "src/runtime/xl6_list.h"
#include "src/api/xl_foreign_error.h"

bool xl6ListValueSetTakeV1(xvalue* source,int64_t key,xvalue* item)
{
    xl6_error_scope scope={0};
    if (!xl6ErrorScopeBegin(&scope)) {
        xerror* primary=xrtTakeError();
        xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtErrorRef(primary)});
        xrtClearError(); xrtValueRelease(item); xrtClearError(); xrtSetErrorTake(primary);
        return false;
    }
    bool success=false; xvalue* receiver=NULL; xvalue* retired=NULL;
    if (!source || !item) xrtSetErrorKind(XERR_ARGUMENT);
    else {
        int native=xl6ListErasedProbeV1(source);
        if (native>0) success=xl6ListErasedSetV1(source,key,item);
        else if (!native && xrtValueType(source)==XVALUE_INT_MAP) {
            receiver=xrtValueRetain(source);
            if (receiver) {
                const xvalue* old=xrtValueIntMapGet(source,key);
                if (!xrtGetError() && old) retired=xrtValueRetain(old);
                if (!xrtGetError() && (!old || retired))
                    success=xrtValueIntMapSetTake(source,key,&item);
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
