#include "src/runtime/xl6_stack_erased_try.h"
#include "src/runtime/xl6_sequence_projection.h"
#include "src/api/xl_foreign_error.h"

_Static_assert(XRT_CALL_RESULT_INLINE_COUNT>=2,"Stack try requires two inline result slots");

bool xl6StackErasedTryCallV1(const xvalue* source,int64_t depth,bool pop,xrtcallresult* result)
{
    xl6_error_scope scope={0};
    if(!xl6ErrorScopeBegin(&scope)) {
        xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});
        return false;
    }
    XL6_Sequence* pin=NULL;
    XL6_SequenceLayoutV1 layout={0};
    const xvalueconverter* converter=NULL;
    xrtcallresult prepared=XRT_CALL_RESULT_INIT;
    xvalue* item=NULL;
    bool found=false,success=false,publicationAttempted=false;
    if(!result || xrtCallResultCount(result)!=0) { xrtSetErrorKind(XERR_ARGUMENT); goto done; }
    if(depth<0 || (uint64_t)depth>SIZE_MAX) { xrtSetErrorKind(XERR_RANGE); goto done; }
    if(pop && depth!=0) { xrtSetErrorKind(XERR_ARGUMENT); goto done; }
    int status=xl6SequenceBeginErasedRoleV1(source,XL6_SEQUENCE_STACK_V1,&pin,&layout,&converter);
    if(status!=1) { if(!status)xrtSetErrorKind(XERR_TYPE); goto done; }
    xtypedarray* storage=xl6SequenceStorageV1(pin);
    size_t count=xrtTypedArrayCount(storage);
    found=(size_t)depth<count;
    if(found) {
        item=xl6SequenceProjectionTakeV1(pin,xrtTypedArrayGetValue(storage,count-1-(size_t)depth,converter));
    } else item=xrtValueRetain(xrtValueNull());
    if(!item || xrtGetError() || scope.failed) goto done;
    /* Both values are prepared in inline slots before any destructive step.
     * Publication uses the native vector move, not an allocated Array/tuple. */
    if(!xrtCallResultSet(&prepared,0,xrtValueBool(found)) ||
       !xrtCallResultSetTake(&prepared,1,&item) || xrtGetError() || scope.failed) goto done;
    if(found && pop && (!xrtTypedArrayRemove(storage,count-1,1) || xrtGetError() || scope.failed)) goto done;
    xl6SequenceEndV1(pin); pin=NULL;
    if(xrtGetError() || scope.failed) goto done;
    publicationAttempted=true;
    if(!xrtCallResultMove(result,&prepared)) goto done;
    success=!xrtGetError() && !scope.failed;
done:
    if(!success || xrtGetError()) xl6ErrorScopeCaptureFailure(&scope);
    if(pin)xl6SequenceEndV1(pin);
    xrtValueRelease(item);
    xrtCallResultUnit(&prepared);
    if(xrtGetError())xl6ErrorScopeCaptureFailure(&scope);
    if(scope.failed && publicationAttempted) {
        xrtCallResultClear(result);
        if(xrtGetError())xl6ErrorScopeCaptureFailure(&scope);
    }
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    if(!outcome.ok)xl6ErrorScopePropagateFailure(outcome);
    return success && outcome.ok;
}
