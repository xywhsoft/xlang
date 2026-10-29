#include "src/runtime/xl6_dynamic_collection_vector.h"
#include "src/api/xl_foreign_error.h"
bool xl6DynamicCollectionVectorArityV1(unsigned operation,size_t count)
{
    return (operation==XL6_DYNAMIC_COLLECTION_TRY_POP || operation==XL6_DYNAMIC_COLLECTION_TRY_PEEK)?count==0:
        operation==XL6_DYNAMIC_COLLECTION_TRY_PEEK_AT && count==1;
}
int xl6DynamicStackMethodProbeV1(const xvalue* source)
{
    XL6_Sequence* pin=NULL; XL6_SequenceLayoutV1 layout={0}; const xvalueconverter* converter=NULL;
    int status=xl6SequenceBeginErasedRoleV1(source,XL6_SEQUENCE_STACK_V1,&pin,&layout,&converter);
    if(pin)xl6SequenceEndV1(pin);
    return status;
}
bool xl6DynamicCollectionVectorTakeV1(xvalue* source,unsigned operation,xvalue* first,xvalue* second,xrtcallresult* result)
{
    xl6_error_scope scope={0};
    if(!xl6ErrorScopeBegin(&scope)) {
        xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});
        xl6_error_scope cleanup={0};
        if(xl6ErrorScopeBeginCleanup(&cleanup,true)) {
            xrtValueRelease(first); if(xrtGetError())xl6ErrorScopeCaptureFailure(&cleanup);
            xrtValueRelease(second); if(xrtGetError())xl6ErrorScopeCaptureFailure(&cleanup);
            xl6ErrorScopePropagateFailure(xl6ErrorScopeEnd(&cleanup));
        } else { xerror* error=xrtTakeError(); xrtValueRelease(first); xrtValueRelease(second); xrtClearError(); xl6ErrorScopePropagateFailure((xl6_error_result){false,error}); }
        return false;
    }
    bool success=false,attempted=false; int64_t depth=0;
    if(!result || xrtCallResultCount(result) || operation<XL6_DYNAMIC_COLLECTION_TRY_POP ||
       operation>XL6_DYNAMIC_COLLECTION_TRY_PEEK_AT || second ||
       ((operation==XL6_DYNAMIC_COLLECTION_TRY_PEEK_AT)!=(first!=NULL)))xrtSetErrorKind(XERR_ARGUMENT);
    else if(first && xrtValueType(first)!=XVALUE_INT)xrtSetErrorKind(XERR_TYPE);
    else if(!first || xrtValueGetInt(first,&depth)) {
        if(!scope.failed && !xrtGetError()) {
            attempted=true; success=xl6StackErasedTryCallV1(source,depth,operation==XL6_DYNAMIC_COLLECTION_TRY_POP,result);
        }
    }
    if(!success || xrtGetError())xl6ErrorScopeCaptureFailure(&scope);
    xrtValueRelease(first); if(xrtGetError())xl6ErrorScopeCaptureFailure(&scope);
    xrtValueRelease(second); if(xrtGetError())xl6ErrorScopeCaptureFailure(&scope);
    if(scope.failed && attempted)xrtCallResultClear(result);
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    if(!outcome.ok)xl6ErrorScopePropagateFailure(outcome);
    return success && outcome.ok;
}
