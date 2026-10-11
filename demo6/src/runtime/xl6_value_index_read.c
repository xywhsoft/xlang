#include "src/runtime/xl6_value_index_read.h"
#include "src/runtime/xl6_array_value_read.h"
#include "src/runtime/xl6_list_value_read.h"
#include "src/runtime/xl6_list.h"
#include "src/runtime/xl6_dict.h"
#include "src/api/xl_foreign_error.h"

static xvalue* read(const xvalue* source,const xvalue* key)
{
    if (xrtValueType(key)==XVALUE_INT) {
        int64_t index;
        if (!xrtValueGetInt(key,&index)) return NULL;
        int list=xl6ListErasedProbeV1(source);
        if (list<0) return NULL;
        if (list || xrtValueType(source)==XVALUE_INT_MAP) return xl6ListValueReadCopyV1(source,index);
        return xl6ArrayValueReadIfArrayCopyV1(source,index);
    }
    if (xrtValueType(key)==XVALUE_STRING) {
        xstrview text;
        if (!xrtValueGetString(key,&text)) return NULL;
        int dict=xl6DictErasedProbeV1(source);
        if (dict<0) return NULL;
        if (dict) {
            xvalue* result=NULL; bool found=false;
            if (!xl6DictErasedReadV1(source,text,false,&result,&found)) return NULL;
            return found ? result : xrtValueRetain(xrtValueNull());
        }
        if (xrtValueType(source)==XVALUE_OBJECT) {
            const xvalue* item=xrtValueObjectGet(source,text);
            return xrtValueRetain(item ? item : xrtValueNull());
        }
    }
    return xrtValueRetain(xrtValueNull());
}
xvalue* xl6ValueIndexReadCopyV1(const xvalue* source,const xvalue* key)
{
    xl6_error_scope scope={0};
    if (!xl6ErrorScopeBegin(&scope)) {
        xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()}); return NULL;
    }
    xvalue* result=read(source,key);
    if (!result || scope.failed || xrtGetError()) {
        xl6ErrorScopeCaptureFailure(&scope); xrtValueRelease(result); result=NULL;
    }
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    if (!outcome.ok) xl6ErrorScopePropagateFailure(outcome);
    return result;
}
