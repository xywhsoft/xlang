#include "xl6_value_named_read.h"
#include "xl6_dict.h"
#include "xl6_value_object_fields.h"
#include "src/api/xl_foreign_error.h"

xvalue* xl6ValueNameReadCopyV1(const xvalue* source,const char* name,size_t size,bool requireRecord)
{
    xl6_error_scope scope={0};
    if(!xl6ErrorScopeBegin(&scope)) {
        xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()}); return NULL;
    }
    xvalue* result=NULL;
    if(!name && size) xrtSetErrorKind(XERR_ARGUMENT);
    else if(!source || xrtValueType(source)==XVALUE_NULL) result=xrtValueRetain(xrtValueNull());
    else {
        int dict=xl6DictErasedProbeV1(source);
        if(dict>0) {
            bool found=false;
            if(xl6DictErasedReadV1(source,(xstrview){name,size},false,&result,&found) && !found)
                result=xrtValueRetain(xrtValueNull());
        } else if(!dict && xrtValueType(source)==XVALUE_OBJECT) {
            const xvalue* item=xl6ValueObjectFieldFindV1(source,name,size);
            if(!xrtGetError() && !scope.failed) result=xrtValueRetain(item ? item : xrtValueNull());
        } else if(!dict) {
            if(requireRecord) xrtSetErrorKind(XERR_TYPE);
            else result=xrtValueRetain(xrtValueNull());
        }
    }
    if(!result || scope.failed || xrtGetError()) {
        xl6ErrorScopeCaptureFailure(&scope); xrtValueRelease(result); result=NULL;
    }
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    if(!outcome.ok) xl6ErrorScopePropagateFailure(outcome);
    return result;
}
