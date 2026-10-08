#include "src/runtime/xl6_value_contains.h"
#include "src/runtime/xl6_text_value.h"
#include "src/runtime/xl6_array_methods.h"
#include "src/api/xl_foreign_error.h"

bool xl6ValueContainsV1(const xvalue* values,const xvalue* item)
{
    xl6_error_scope operation={0}; bool found=false; xvalueiter* iterator=NULL;
    if (!xl6ErrorScopeBegin(&operation)) return false;
    if (!values || !item) xrtSetErrorKind(XERR_ARGUMENT);
    else {
        xvaluetype type=xrtValueType(values); xstrview text={0},needle={0};
        if (type==XVALUE_ARRAY || type==XVALUE_HANDLE)
            found=xl6ArrayMethodIndexOfV1(values,item)>=0;
        else if (type==XVALUE_SET) found=xrtValueSetHas(values,item);
        else if (type==XVALUE_OBJECT && xrtValueType(item)==XVALUE_STRING && xrtValueGetString(item,&needle))
            found=xrtValueObjectHas(values,needle);
        else if (type==XVALUE_STRING && xrtValueType(item)==XVALUE_STRING &&
            xrtValueGetString(values,&text) && xrtValueGetString(item,&needle))
            found=xrtStrContains(text,needle);
        else if (type==XVALUE_INT_MAP) {
            iterator=xrtValueIterCreate(values);
            if (!iterator) xl6ErrorScopeCaptureFailure(&operation);
            while (iterator && !operation.failed && !found) {
                xvalue* current=xrtValueIterNext(iterator,NULL);
                if (xrtGetError()) xl6ErrorScopeCaptureFailure(&operation);
                if (!current || operation.failed) break;
                found=xrtValueEqual(current,item);
                if (xrtGetError()) xl6ErrorScopeCaptureFailure(&operation);
            }
        }
    }
    if (xrtGetError()) xl6ErrorScopeCaptureFailure(&operation);
    xrtValueIterDestroy(iterator);
    xl6_error_result outcome=xl6ErrorScopeEnd(&operation);
    if (!outcome.ok) xl6ErrorScopePropagateFailure(outcome);
    return outcome.ok && found;
}
