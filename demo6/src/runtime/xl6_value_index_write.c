#include "src/runtime/xl6_value_index_write.h"
#include "src/runtime/xl6_array_value_write.h"
#include "src/runtime/xl6_list.h"
#include "src/runtime/xl6_dict.h"
#include "src/api/xl_foreign_error.h"

bool xl6ValueIndexSetTakeV1(xvalue* source,const xvalue* key,xvalue* item)
{
    xl6_error_scope scope={0};
    if (!xl6ErrorScopeBegin(&scope)) {
        /* No new boundary exists. Latch the primary failure before callbacks,
         * then consume the input without replacing its first diagnostic. */
        xerror* primary=xrtTakeError();
        xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtErrorRef(primary)});
        xrtClearError(); xrtValueRelease(item); xrtClearError(); xrtSetErrorTake(primary);
        return false;
    }
    bool success=false;
    xvalue* receiverPin=NULL;
    xvalue* retiredPin=NULL;
    if (!source || !key || !item) xrtSetErrorKind(XERR_ARGUMENT);
    else if (xrtValueType(key)==XVALUE_INT) {
        int64_t index;
        if (xrtValueGetInt(key,&index)) {
            int list=xl6ListErasedProbeV1(source);
            if (list>0) success=xl6ListErasedSetV1(source,index,item);
            else if (!list && xrtValueType(source)==XVALUE_INT_MAP)
                success=xrtValueIntMapSetTake(source,index,&item);
            else if (!list) { success=xl6ArrayValueSetTakeV1(source,index,item); item=NULL; }
        }
    } else if (xrtValueType(key)==XVALUE_STRING) {
        xstrview text;
        if (xrtValueGetString(key,&text)) {
            int dict=xl6DictErasedProbeV1(source);
            if (dict>0) success=xl6DictErasedSetV1(source,text,item);
            else if (!dict && xrtValueType(source)==XVALUE_OBJECT) {
                /* Keep the old child alive across SetTake's mutation. Its last
             * release, including a destructor that releases a back-reference
             * or reenters this object, then happens outside the BUSY fence.
             * Take leaves a rejected input with us: latch the operation's
             * first failure BEFORE either retirement can report a callback
             * failure. These pins only retain owners; no field snapshot or
             * allocation is added to an ordinary replacement. */
                receiverPin=xrtValueRetain(source);
                if (receiverPin) {
                    const xvalue* old=xrtValueObjectGet(source,text);
                    if (!xrtGetError() && old) retiredPin=xrtValueRetain(old);
                    if (!xrtGetError() && (!old || retiredPin))
                        success=xrtValueObjectSetTake(source,text,&item);
                }
            } else if (!dict) xrtSetErrorKind(XERR_TYPE);
        }
    } else xrtSetErrorKind(XERR_TYPE);
    if (!success || xrtGetError()) xl6ErrorScopeCaptureFailure(&scope);
    xrtValueRelease(item);
    if (xrtGetError()) xl6ErrorScopeCaptureFailure(&scope);
    xrtValueRelease(retiredPin);
    if (xrtGetError()) xl6ErrorScopeCaptureFailure(&scope);
    xrtValueRelease(receiverPin);
    if (xrtGetError()) xl6ErrorScopeCaptureFailure(&scope);
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    if (!outcome.ok) xl6ErrorScopePropagateFailure(outcome);
    return outcome.ok;
}
