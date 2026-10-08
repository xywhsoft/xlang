#include "src/runtime/xl6_array_method_view.h"
#include "src/runtime/xl6_sequence_projection.h"
#include "src/api/xl_foreign_error.h"
#include <stdlib.h>

static bool endOperation(ArrayView* view,xl6_error_scope* operation,bool ok)
{
    if (!ok || xrtGetError()) xl6ErrorScopeCaptureFailure(operation);
    finish(view);
    xl6_error_result outcome=xl6ErrorScopeEnd(operation);
    if (!outcome.ok) xl6ErrorScopePropagateFailure(outcome);
    return outcome.ok;
}
bool xl6ArrayMethodRemoveV1(xvalue* source,int64_t at,int64_t amount)
{
    xl6_error_scope operation={0}; ArrayView view={0}; bool ok=false;
    if (!xl6ErrorScopeBegin(&operation)) return false;
    if (extent(at) && extent(amount) && begin(source,&view)) {
        size_t size=count(&view);
        if ((size_t)at>size || (size_t)amount>size-(size_t)at) xrtSetErrorKind(XERR_RANGE);
        else ok=view.owner ? xrtTypedArrayRemove(view.storage,(size_t)at,(size_t)amount) :
            xrtValueArrayRemove(view.wire,(size_t)at,(size_t)amount);
    }
    return endOperation(&view,&operation,ok);
}
xvalue* xl6ArrayMethodTakeV1(xvalue* source,int64_t at,bool pop)
{
    xl6_error_scope operation={0}; ArrayView view={0}; xvalue* result=NULL;
    if (!xl6ErrorScopeBegin(&operation)) return NULL;
    if (pop && at) xrtSetErrorKind(XERR_ARGUMENT);
    else if ((pop || extent(at)) && begin(source,&view)) {
        size_t size=count(&view);
        if (!size || (!pop && (size_t)at>=size)) xrtSetErrorKind(XERR_RANGE);
        else {
            size_t position=pop ? size-1 : (size_t)at;
            if (view.owner) {
                if (xl6SequenceAdmitSlotV1(view.owner,xrtTypedArrayConstGet(view.storage,position)))
                    result=xl6SequenceProjectionTakeV1(view.owner,
                        xrtTypedArrayGetValue(view.storage,position,view.converter));
                if (!result || xrtGetError()) xl6ErrorScopeCaptureFailure(&operation);
                /* A callback can fail without allocating a diagnostic. Never
                 * commit removal just because the projected shell is non-NULL. */
                if (!operation.failed && !xrtTypedArrayRemove(view.storage,position,1))
                    xl6ErrorScopeCaptureFailure(&operation);
            } else result=xrtValueArrayTake(view.wire,position);
        }
    }
    if (!result || xrtGetError()) xl6ErrorScopeCaptureFailure(&operation);
    finish(&view);
    if (xrtGetError()) xl6ErrorScopeCaptureFailure(&operation);
    if (operation.failed) { xrtValueRelease(result); result=NULL; }
    xl6_error_result outcome=xl6ErrorScopeEnd(&operation);
    if (!outcome.ok) xl6ErrorScopePropagateFailure(outcome);
    return result;
}
