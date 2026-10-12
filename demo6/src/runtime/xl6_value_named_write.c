#include "xl6_value_named_write.h"
#include "xl6_dict.h"
#include "xl6_value_object_fields.h"
#include "src/api/xl_foreign_error.h"

bool xl6ValueNameSetTakeV1(xvalue* source,const char* name,size_t size,xvalue* item)
{
    xl6_error_scope scope={0};
    if(!xl6ErrorScopeBegin(&scope)) {
        xerror* primary=xrtTakeError();
        xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtErrorRef(primary)});
        xrtClearError(); xrtValueRelease(item); xrtClearError(); xrtSetErrorTake(primary);
        return false;
    }
    bool success=false;
    if(!source || !item || (!name && size)) xrtSetErrorKind(XERR_ARGUMENT);
    else {
        int dict=xl6DictErasedProbeV1(source);
        if(dict>0) success=xl6DictErasedSetV1(source,(xstrview){name,size},item);
        else if(!dict && xrtValueType(source)==XVALUE_OBJECT) {
            success=xl6ValueObjectFieldSetTakeV1(source,name,size,item); item=NULL;
        } else if(!dict) xrtSetErrorKind(XERR_TYPE);
    }
    if(!success || xrtGetError()) xl6ErrorScopeCaptureFailure(&scope);
    xrtValueRelease(item);
    if(xrtGetError()) xl6ErrorScopeCaptureFailure(&scope);
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    if(!outcome.ok) xl6ErrorScopePropagateFailure(outcome);
    return outcome.ok;
}
