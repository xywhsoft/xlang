#include "src/runtime/xl6_value_contains.h"
#include "src/runtime/xl6_text_value.h"
#include "src/runtime/xl6_array_methods.h"
#include "src/runtime/xl6_list.h"
#include "src/runtime/xl6_set.h"
#include "src/runtime/xl6_dict.h"
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
        int set=type==XVALUE_HANDLE && !list ? xl6SetErasedProbeV1(values) : 0;
        int dict=type==XVALUE_HANDLE && !list && !set ? xl6DictErasedProbeV1(values) : 0;
        if (list<0 || set<0 || dict<0) xl6ErrorScopeCaptureFailure(&operation);
        else if (list) {
            if (!xl6ListErasedContainsV1(values,item,&found)) xl6ErrorScopeCaptureFailure(&operation);
        } else if (set) {
            if (!xl6SetErasedKeyV1(values,item,XL6_SET_HAS_V1,&found)) xl6ErrorScopeCaptureFailure(&operation);
        } else if (dict) {
            xstrview key={0};
            if (!xrtValueGetString(item,&key)) { xrtSetErrorKind(XERR_TYPE); xl6ErrorScopeCaptureFailure(&operation); }
            else if (!xl6DictErasedKeyV1(values,key,false,&found)) xl6ErrorScopeCaptureFailure(&operation);
        } else if (type==XVALUE_ARRAY || type==XVALUE_HANDLE)
            found=xl6ArrayMethodIndexOfV1(values,item)>=0;
        else found=xl6ValueWireContainsV1(values,item);
    }
    if (xrtGetError()) xl6ErrorScopeCaptureFailure(&operation);
    xl6_error_result outcome=xl6ErrorScopeEnd(&operation);
    if (!outcome.ok) xl6ErrorScopePropagateFailure(outcome);
    return outcome.ok && found;
}
