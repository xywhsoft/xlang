#include "src/runtime/xl6_value_contains.h"
#include "src/runtime/xl6_text_value.h"
#include "src/runtime/xl6_array_methods.h"
#include "src/runtime/xl6_list.h"
#include "src/api/xl_foreign_error.h"

bool xl6ValueContainsV1(const xvalue* values,const xvalue* item)
{
    xl6_error_scope operation={0}; bool found=false;
    if (!xl6ErrorScopeBegin(&operation)) {
        xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()}); return false;
    }
    if (!values || !item) xrtSetErrorKind(XERR_ARGUMENT);
    else {
        xvaluetype type=xrtValueType(values);
        int list=type==XVALUE_HANDLE ? xl6ListErasedProbeV1(values) : 0;
        if (list<0) xl6ErrorScopeCaptureFailure(&operation);
        else if (list) {
            if (!xl6ListErasedContainsV1(values,item,&found)) xl6ErrorScopeCaptureFailure(&operation);
        } else if (type==XVALUE_ARRAY || type==XVALUE_HANDLE)
            found=xl6ArrayMethodIndexOfV1(values,item)>=0;
        else found=xl6ValueWireContainsV1(values,item);
    }
    if (xrtGetError()) xl6ErrorScopeCaptureFailure(&operation);
    xl6_error_result outcome=xl6ErrorScopeEnd(&operation);
    if (!outcome.ok) xl6ErrorScopePropagateFailure(outcome);
    return outcome.ok && found;
}
