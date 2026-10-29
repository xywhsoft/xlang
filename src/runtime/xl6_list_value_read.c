#include "src/runtime/xl6_list_value_read.h"
#include "src/runtime/xl6_list.h"
#include "src/api/xl_foreign_error.h"

static xvalue* read(const xvalue* source,int64_t key,bool safe,bool required)
{
    xl6_error_scope scope={0};
    if (!xl6ErrorScopeBegin(&scope)) return NULL;
    xvalue* result=NULL; bool found=false,success=false;
    if (safe && (!source || xrtValueType(source)==XVALUE_NULL)) success=true;
    else if (!source) xrtSetErrorKind(XERR_ARGUMENT);
    else {
        int native=xl6ListErasedProbeV1(source);
        if (native>0) success=xl6ListErasedReadV1(source,key,false,&result,&found);
        else if (!native && xrtValueType(source)==XVALUE_INT_MAP) {
            const xvalue* item=xrtValueIntMapGet(source,key);
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
    if (!outcome.ok) xrtSetErrorTake(outcome.error);
    return result;
}
xvalue* xl6ListValueReadCopyV1(const xvalue* source,int64_t key)
{ return read(source,key,false,false); }
xvalue* xl6ListValueReadOrNullCopyV1(const xvalue* source,int64_t key)
{ return read(source,key,true,false); }
xvalue* xl6ListValueReadRequiredCopyV1(const xvalue* source,int64_t key)
{ return read(source,key,false,true); }
