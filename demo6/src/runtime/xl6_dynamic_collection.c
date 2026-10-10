#include "src/runtime/xl6_dynamic_collection.h"
#include "src/runtime/xl6_text_value.h"
#include "src/runtime/xl6_array_erased.h"
#include "src/runtime/xl6_array_methods.h"
#include "src/runtime/xl6_list.h"
#include "src/runtime/xl6_list_methods.h"
#include "src/runtime/xl6_value_contains.h"
#include "src/api/xl_foreign_error.h"
#include <stdint.h>
#include <stdlib.h>

bool xl6DynamicCollectionArityV1(unsigned operation,size_t count)
{
    switch (operation) {
    case XL6_DYNAMIC_COLLECTION_POP: case XL6_DYNAMIC_COLLECTION_CLEAR: return count==0;
    case XL6_DYNAMIC_COLLECTION_INSERT: case XL6_DYNAMIC_COLLECTION_SWAP: return count==2;
    case XL6_DYNAMIC_COLLECTION_REMOVE: return count==1 || count==2;
    case XL6_DYNAMIC_COLLECTION_PUSH: case XL6_DYNAMIC_COLLECTION_REMOVE_VALUE:
    case XL6_DYNAMIC_COLLECTION_TAKE: case XL6_DYNAMIC_COLLECTION_RESERVE:
    case XL6_DYNAMIC_COLLECTION_ADD: case XL6_DYNAMIC_COLLECTION_CONTAINS:
    case XL6_DYNAMIC_COLLECTION_INDEX_OF: return count==1;
    default: return false;
    }
}
static bool integer(const xvalue* value,int64_t* output)
{
    if (value && xrtValueGetInt(value,output)) return true;
    if (!xrtGetError()) xrtSetErrorKind(XERR_TYPE);
    return false;
}
static bool string(const xvalue* value,xstrview* output)
{
    if (value && xrtValueGetString(value,output)) return true;
    if (!xrtGetError()) xrtSetErrorKind(XERR_TYPE);
    return false;
}
xvalue* xl6DynamicCollectionCallTakeV1(xvalue* source,unsigned operation,xvalue* arg0,xvalue* arg1)
{
    xl6_error_scope transaction={0}; xvalue* result=NULL; bool ok=false,required=true;
    int64_t first=0,second=1,index=-1; xstrview text={0};
    /* A scope failure still owns the arguments. Cleanup joins that failure
     * independently of whether a diagnostic object could be allocated. */
    if (!xl6ErrorScopeBegin(&transaction)) {
        xl6_error_scope cleanup={0};
        if (!xl6ErrorScopeBeginCleanup(&cleanup,true)) abort();
        xrtValueRelease(arg0); xrtValueRelease(arg1);
        xl6ErrorScopePropagateFailure(xl6ErrorScopeEnd(&cleanup)); return NULL;
    }
    if (!source || (arg1 && !arg0) || !xl6DynamicCollectionArityV1(operation,!!arg0+!!arg1)) xrtSetErrorKind(XERR_ARGUMENT);
    else {
        xvaluetype type=xrtValueType(source);
        int list=xl6ListErasedProbeV1(source);
        bool array=type==XVALUE_ARRAY;
        /* HANDLE is a physical carrier, never a collection family. Authenticate
         * the resident Array role before dispatch, without touching foreign raw
         * pointers or treating List/Stack/iterator handles as contiguous arrays. */
        if (!list && type==XVALUE_HANDLE) {
            XL6_Sequence* owner=NULL; XL6_SequenceLayoutV1 layout;
            const xvalueconverter* converter=NULL;
            int status=xl6SequenceBeginErasedArrayV1(source,&owner,&layout,&converter);
            if (owner) xl6SequenceEndV1(owner);
            if (status<0) xl6ErrorScopeCaptureFailure(&transaction);
            array=status==1;
        }
        if (list<0) xl6ErrorScopeCaptureFailure(&transaction);
        if (!transaction.failed) switch (operation) {
        case XL6_DYNAMIC_COLLECTION_PUSH:
            if (array) { ok=xl6ArrayValueAppendTakeV1(source,arg0); arg0=NULL; }
            else xrtSetErrorKind(XERR_TYPE);
            break;
        case XL6_DYNAMIC_COLLECTION_INSERT:
            if (array && integer(arg0,&first)) ok=xl6ArrayMethodInsertV1(source,first,arg1);
            else if (!xrtGetError()) xrtSetErrorKind(XERR_TYPE);
            break;
        case XL6_DYNAMIC_COLLECTION_REMOVE:
            if (array && integer(arg0,&first) && (!arg1 || integer(arg1,&second)))
                ok=xl6ArrayMethodRemoveV1(source,first,second);
            else if (!arg1 && (list || type==XVALUE_INT_MAP) && integer(arg0,&first)) {
                required=false; ok=xl6ListValueRemoveV1(source,first);
            } else if (!arg1 && type==XVALUE_SET) { required=false; ok=xrtValueSetRemove(source,arg0); }
            else if (!arg1 && type==XVALUE_OBJECT && string(arg0,&text)) {
                required=false; ok=xrtValueObjectRemove(source,text);
            }
            else if (!xrtGetError()) xrtSetErrorKind(XERR_TYPE);
            break;
        case XL6_DYNAMIC_COLLECTION_REMOVE_VALUE:
            required=false;
            if (array) ok=xl6ArrayMethodRemoveValueV1(source,arg0);
            else xrtSetErrorKind(XERR_TYPE);
            break;
        case XL6_DYNAMIC_COLLECTION_POP:
            if (array) result=xl6ArrayMethodTakeV1(source,0,true);
            else xrtSetErrorKind(XERR_TYPE);
            ok=result!=NULL; break;
        case XL6_DYNAMIC_COLLECTION_TAKE:
            if (array && integer(arg0,&first)) result=xl6ArrayMethodTakeV1(source,first,false);
            else if ((list || type==XVALUE_INT_MAP) && integer(arg0,&first)) result=xl6ListValueTakeV1(source,first);
            else if (type==XVALUE_OBJECT && string(arg0,&text)) {
                result=xrtValueObjectTake(source,text);
                if (!result && !xrtGetError() && !transaction.failed) result=xrtValueRetain(xrtValueNull());
            } else if (!xrtGetError()) xrtSetErrorKind(XERR_TYPE);
            ok=result!=NULL; break;
        case XL6_DYNAMIC_COLLECTION_CLEAR:
            if (array) ok=xl6ArrayMethodClearV1(source);
            else if (list || type==XVALUE_INT_MAP) ok=xl6ListValueClearV1(source);
            else if (type==XVALUE_SET || type==XVALUE_OBJECT) ok=xrtValueClear(source);
            else xrtSetErrorKind(XERR_TYPE);
            break;
        case XL6_DYNAMIC_COLLECTION_RESERVE:
            if (array && integer(arg0,&first)) ok=xl6ArrayMethodReserveV1(source,first);
            else if (!xrtGetError()) xrtSetErrorKind(XERR_TYPE);
            break;
        case XL6_DYNAMIC_COLLECTION_SWAP:
            if (array && integer(arg0,&first) && integer(arg1,&second)) ok=xl6ArrayMethodSwapV1(source,first,second);
            else if (!xrtGetError()) xrtSetErrorKind(XERR_TYPE);
            break;
        case XL6_DYNAMIC_COLLECTION_ADD:
            if (type==XVALUE_SET) ok=xrtValueSetAddTake(source,&arg0);
            else xrtSetErrorKind(XERR_TYPE);
            break;
        case XL6_DYNAMIC_COLLECTION_CONTAINS:
            required=false; ok=list ? xl6ListValueContainsV1(source,arg0) : xl6ValueContainsV1(source,arg0); break;
        case XL6_DYNAMIC_COLLECTION_INDEX_OF:
            if (array) index=xl6ArrayMethodIndexOfV1(source,arg0);
            else xrtSetErrorKind(XERR_TYPE);
            if (!xrtGetError() && !transaction.failed) result=xrtValueInt(index);
            ok=result!=NULL; break;
        }
        if ((!ok && required) || xrtGetError()) xl6ErrorScopeCaptureFailure(&transaction);
        if (!transaction.failed && !result) result=xrtValueBool(ok);
    }
    if (!result || xrtGetError()) xl6ErrorScopeCaptureFailure(&transaction);
    xrtValueRelease(arg0);
    if (xrtGetError()) xl6ErrorScopeCaptureFailure(&transaction);
    xrtValueRelease(arg1);
    if (xrtGetError()) xl6ErrorScopeCaptureFailure(&transaction);
    if (transaction.failed) { xrtValueRelease(result); result=NULL; }
    xl6_error_result outcome=xl6ErrorScopeEnd(&transaction);
    if (!outcome.ok) xl6ErrorScopePropagateFailure(outcome);
    return result;
}
