#include "src/runtime/xl6_dynamic_collection.h"
#include "src/runtime/xl6_text_value.h"
#include "src/runtime/xl6_array_erased.h"
#include "src/runtime/xl6_stack_erased.h"
#include "src/runtime/xl6_stack_erased_methods.h"
#include "src/runtime/xl6_stack.h"
#include "src/runtime/xl6_array_methods.h"
#include "src/runtime/xl6_list.h"
#include "src/runtime/xl6_list_methods.h"
#include "src/runtime/xl6_set.h"
#include "src/runtime/xl6_dict.h"
#include "src/runtime/xl6_value_contains.h"
#include "src/api/xl_foreign_error.h"
#include <stdint.h>
#include <stdlib.h>

bool xl6DynamicCollectionArityV1(unsigned operation,size_t count)
{
    switch (operation) {
    case XL6_DYNAMIC_COLLECTION_POP: case XL6_DYNAMIC_COLLECTION_CLEAR:
    case XL6_DYNAMIC_COLLECTION_COUNT: case XL6_DYNAMIC_COLLECTION_CAPACITY:
    case XL6_DYNAMIC_COLLECTION_PEEK: case XL6_DYNAMIC_COLLECTION_TRIM:
    case XL6_DYNAMIC_COLLECTION_CLONE: return count==0;
    case XL6_DYNAMIC_COLLECTION_INSERT: case XL6_DYNAMIC_COLLECTION_SWAP:
    case XL6_DYNAMIC_COLLECTION_PEEK_BATCH_AT: return count==2;
    case XL6_DYNAMIC_COLLECTION_REMOVE: return count==1 || count==2;
    case XL6_DYNAMIC_COLLECTION_PUSH: case XL6_DYNAMIC_COLLECTION_REMOVE_VALUE:
    case XL6_DYNAMIC_COLLECTION_TAKE: case XL6_DYNAMIC_COLLECTION_RESERVE:
    case XL6_DYNAMIC_COLLECTION_ADD: case XL6_DYNAMIC_COLLECTION_CONTAINS:
    case XL6_DYNAMIC_COLLECTION_INDEX_OF: case XL6_DYNAMIC_COLLECTION_PEEK_AT:
    case XL6_DYNAMIC_COLLECTION_PUSH_BATCH: case XL6_DYNAMIC_COLLECTION_POP_BATCH:
    case XL6_DYNAMIC_COLLECTION_PEEK_BATCH: case XL6_DYNAMIC_COLLECTION_EQUALS: return count==1;
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
static void discardArguments(xvalue* first,xvalue* second)
{
    xerror* incoming=xrtErrorRef(xrtGetError()); xl6_error_scope cleanup={0};
    if (xl6ErrorScopeBeginCleanup(&cleanup,true)) {
        xrtErrorFree(incoming); xrtValueRelease(first); xrtValueRelease(second);
        xl6ErrorScopePropagateFailure(xl6ErrorScopeEnd(&cleanup));
    } else {
        xrtClearError();
        xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtErrorRef(incoming)});
        xrtClearError(); xrtValueRelease(first); xrtValueRelease(second);
        xrtClearError(); xrtSetErrorTake(incoming);
    }
}
xvalue* xl6DynamicCollectionCallTakeV1(xvalue* source,unsigned operation,xvalue* arg0,xvalue* arg1)
{
    xl6_error_scope transaction={0}; xvalue* result=NULL; bool ok=false,required=true;
    int64_t first=0,second=1,index=-1; xstrview text={0};
    /* A scope failure still owns the arguments. Cleanup joins that failure
     * independently of whether a diagnostic object could be allocated. */
    if (!xl6ErrorScopeBegin(&transaction)) {
        xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});
        discardArguments(arg0,arg1); return NULL;
    }
    if (!source || (arg1 && !arg0) || !xl6DynamicCollectionArityV1(operation,!!arg0+!!arg1)) xrtSetErrorKind(XERR_ARGUMENT);
    else {
        xvaluetype type=xrtValueType(source);
        int list=xl6ListErasedProbeV1(source);
        int set=list ? 0 : xl6SetErasedProbeV1(source);
        int dict=list || set ? 0 : xl6DictErasedProbeV1(source);
        bool array=type==XVALUE_ARRAY,stack=false;
        XL6_SequenceLayoutV1 stackLayout={0};
        /* HANDLE is a physical carrier, never a collection family. Authenticate
         * the resident Array role before dispatch, without touching foreign raw
         * pointers or treating List/Stack/iterator handles as contiguous arrays. */
        if (!list && !set && !dict && type==XVALUE_HANDLE) {
            XL6_Sequence* owner=NULL; XL6_SequenceLayoutV1 layout;
            const xvalueconverter* converter=NULL;
            int status=xl6SequenceBeginErasedArrayV1(source,&owner,&layout,&converter);
            if (owner) xl6SequenceEndV1(owner);
            if (status<0) xl6ErrorScopeCaptureFailure(&transaction);
            array=status==1;
            if (!status) {
                status=xl6SequenceBeginErasedRoleV1(source,XL6_SEQUENCE_STACK_V1,&owner,&stackLayout,&converter);
                if (owner) xl6SequenceEndV1(owner);
                if (status<0) xl6ErrorScopeCaptureFailure(&transaction);
                stack=status==1;
            }
        }
#define STACK_LAYOUT stackLayout.identity,stackLayout.elementIdentity,stackLayout.transfer,stackLayout.size,stackLayout.type
        if (list<0 || set<0 || dict<0) xl6ErrorScopeCaptureFailure(&transaction);
        if (!transaction.failed) switch (operation) {
        case XL6_DYNAMIC_COLLECTION_PUSH:
            if (array) { ok=xl6ArrayValueAppendTakeV1(source,arg0); arg0=NULL; }
            else if (stack) ok=xl6StackErasedPushV1(source,arg0);
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
            } else if (!arg1 && set) {
                required=false; bool found=false;
                if (xl6SetErasedKeyV1(source,arg0,XL6_SET_REMOVE_V1,&found)) ok=found;
                else xl6ErrorScopeCaptureFailure(&transaction);
            } else if (!arg1 && type==XVALUE_SET) { required=false; ok=xrtValueSetRemove(source,arg0); }
            else if (!arg1 && dict && string(arg0,&text)) {
                required=false; bool found=false;
                if (xl6DictErasedKeyV1(source,text,true,&found)) ok=found;
                else xl6ErrorScopeCaptureFailure(&transaction);
            } else if (!arg1 && type==XVALUE_OBJECT && string(arg0,&text)) {
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
            else if (stack) result=xl6StackErasedReadV1(source,0,true);
            else xrtSetErrorKind(XERR_TYPE);
            ok=result!=NULL; break;
        case XL6_DYNAMIC_COLLECTION_TAKE:
            if (array && integer(arg0,&first)) result=xl6ArrayMethodTakeV1(source,first,false);
            else if ((list || type==XVALUE_INT_MAP) && integer(arg0,&first)) result=xl6ListValueTakeV1(source,first);
            else if (set) {
                bool found=false;
                if (!xl6SetErasedReadV1(source,arg0,true,&result,&found)) xl6ErrorScopeCaptureFailure(&transaction);
                else if (!found) result=xrtValueRetain(xrtValueNull());
            }
            else if (dict && string(arg0,&text)) {
                bool found=false;
                if (!xl6DictErasedReadV1(source,text,true,&result,&found)) xl6ErrorScopeCaptureFailure(&transaction);
                else if (!found) result=xrtValueRetain(xrtValueNull());
            } else if (type==XVALUE_OBJECT && string(arg0,&text)) {
                result=xrtValueObjectTake(source,text);
                if (!result && !xrtGetError() && !transaction.failed) result=xrtValueRetain(xrtValueNull());
            } else if (!xrtGetError()) xrtSetErrorKind(XERR_TYPE);
            ok=result!=NULL; break;
        case XL6_DYNAMIC_COLLECTION_CLEAR:
            if (array) ok=xl6ArrayMethodClearV1(source);
            else if (stack) ok=xl6StackClearV1(source,STACK_LAYOUT);
            else if (list || type==XVALUE_INT_MAP) ok=xl6ListValueClearV1(source);
            else if (set) ok=xl6SetErasedClearV1(source);
            else if (dict) ok=xl6DictErasedClearV1(source);
            else if (type==XVALUE_SET || type==XVALUE_OBJECT) ok=xrtValueClear(source);
            else xrtSetErrorKind(XERR_TYPE);
            break;
        case XL6_DYNAMIC_COLLECTION_RESERVE:
            if (array && integer(arg0,&first)) ok=xl6ArrayMethodReserveV1(source,first);
            else if (stack && integer(arg0,&first)) ok=xl6StackReserveV1(source,STACK_LAYOUT,first);
            else if (set && integer(arg0,&first)) ok=xl6SetErasedReserveV1(source,first);
            else if (dict && integer(arg0,&first)) ok=xl6DictErasedReserveV1(source,first);
            else if (!xrtGetError()) xrtSetErrorKind(XERR_TYPE);
            break;
        case XL6_DYNAMIC_COLLECTION_SWAP:
            if (array && integer(arg0,&first) && integer(arg1,&second)) ok=xl6ArrayMethodSwapV1(source,first,second);
            else if (!xrtGetError()) xrtSetErrorKind(XERR_TYPE);
            break;
        case XL6_DYNAMIC_COLLECTION_ADD:
            if (set) { bool added=false; ok=xl6SetErasedKeyV1(source,arg0,XL6_SET_ADD_V1,&added); }
            else if (type==XVALUE_SET) ok=xrtValueSetAddTake(source,&arg0);
            else xrtSetErrorKind(XERR_TYPE);
            break;
        case XL6_DYNAMIC_COLLECTION_CONTAINS:
            required=false; ok=list ? xl6ListValueContainsV1(source,arg0) : xl6ValueContainsV1(source,arg0); break;
        case XL6_DYNAMIC_COLLECTION_INDEX_OF:
            if (array) index=xl6ArrayMethodIndexOfV1(source,arg0);
            else xrtSetErrorKind(XERR_TYPE);
            if (!xrtGetError() && !transaction.failed) result=xrtValueInt(index);
            ok=result!=NULL; break;
        case XL6_DYNAMIC_COLLECTION_COUNT:
        case XL6_DYNAMIC_COLLECTION_CAPACITY:
            if (stack) {
                first=xl6StackSizeV1(source,STACK_LAYOUT,operation==XL6_DYNAMIC_COLLECTION_CAPACITY);
                if (!xrtGetError() && !transaction.failed) result=xrtValueInt(first);
            } else xrtSetErrorKind(XERR_TYPE);
            ok=result!=NULL; break;
        case XL6_DYNAMIC_COLLECTION_PEEK:
        case XL6_DYNAMIC_COLLECTION_PEEK_AT:
            if (stack && (operation==XL6_DYNAMIC_COLLECTION_PEEK || integer(arg0,&first)))
                result=xl6StackErasedReadV1(source,first,false);
            else if (!xrtGetError()) xrtSetErrorKind(XERR_TYPE);
            ok=result!=NULL; break;
        case XL6_DYNAMIC_COLLECTION_TRIM:
            if (stack) ok=xl6StackTrimV1(source,STACK_LAYOUT);
            else xrtSetErrorKind(XERR_TYPE);
            break;
        case XL6_DYNAMIC_COLLECTION_CLONE:
            if (stack) result=xl6StackCloneV1(source,STACK_LAYOUT);
            else xrtSetErrorKind(XERR_TYPE);
            ok=result!=NULL; break;
        case XL6_DYNAMIC_COLLECTION_PUSH_BATCH:
        case XL6_DYNAMIC_COLLECTION_POP_BATCH:
        case XL6_DYNAMIC_COLLECTION_PEEK_BATCH:
        case XL6_DYNAMIC_COLLECTION_PEEK_BATCH_AT:
        case XL6_DYNAMIC_COLLECTION_EQUALS:
            if (stack) {
                unsigned native=operation==XL6_DYNAMIC_COLLECTION_PUSH_BATCH ? XL6_STACK_ERASED_PUSH_BATCH :
                    operation==XL6_DYNAMIC_COLLECTION_POP_BATCH ? XL6_STACK_ERASED_POP_BATCH :
                    operation==XL6_DYNAMIC_COLLECTION_PEEK_BATCH ? XL6_STACK_ERASED_PEEK_BATCH :
                    operation==XL6_DYNAMIC_COLLECTION_PEEK_BATCH_AT ? XL6_STACK_ERASED_PEEK_BATCH_AT : XL6_STACK_ERASED_EQUALS;
                result=xl6StackErasedMethodV1(source,native,arg0,arg1);
            } else xrtSetErrorKind(XERR_TYPE);
            ok=result!=NULL; break;
        }
#undef STACK_LAYOUT
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
