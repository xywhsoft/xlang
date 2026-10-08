#include "src/runtime/xl6_array_method_view.h"
#include "src/runtime/xl6_sequence_projection.h"
#include "src/api/xl_foreign_error.h"

/* -1 not found, -2 failure. Never remove a candidate before its projection
 * and equality finish; allocator/converter reentry cannot mutate native slots. */
static int64_t find(ArrayView* view, const xvalue* item,xl6_error_scope* operation)
{
    size_t size = count(view);
    if (size > INT64_MAX) { xrtSetErrorKind(XERR_RANGE); return -2; }
    for (size_t at = 0; at < size; ++at) {
        if (view->owner && !xl6SequenceAdmitSlotV1(view->owner,xrtTypedArrayConstGet(view->storage,at)))
            return -2;
        xvalue* current = view->owner
            ? xl6SequenceProjectionTakeV1(view->owner,
                xrtTypedArrayGetValue(view->storage, at, view->converter))
            : xrtValueRetain(xrtValueArrayGet(view->wire, at));
        if (!current) {
            if (!xrtGetError()) xrtSetErrorKind(view->owner ? XERR_MEMORY : XERR_STATE);
            return -2;
        }
        if (xrtGetError()) xl6ErrorScopeCaptureFailure(operation);
        bool equal = !operation->failed && xrtValueEqual(current, item);
        if (xrtGetError()) xl6ErrorScopeCaptureFailure(operation);
        xrtValueRelease(current);
        if (xrtGetError()) xl6ErrorScopeCaptureFailure(operation);
        if (operation->failed) return -2;
        if (equal) return (int64_t)at;
    }
    return -1;
}

int64_t xl6ArrayMethodIndexOfV1(const xvalue* source, const xvalue* item)
{
    xl6_error_scope operation={0}; ArrayView view={0}; int64_t at=-2;
    if (!xl6ErrorScopeBegin(&operation)) return -1;
    if (!item) xrtSetErrorKind(XERR_ARGUMENT);
    else if (begin(source,&view)) at=find(&view,item,&operation);
    if (at==-2 || xrtGetError()) xl6ErrorScopeCaptureFailure(&operation);
    finish(&view);
    xl6_error_result outcome=xl6ErrorScopeEnd(&operation);
    if (!outcome.ok) xl6ErrorScopePropagateFailure(outcome);
    return outcome.ok && at>=0 ? at : -1;
}


bool xl6ArrayMethodRemoveValueV1(xvalue* source, const xvalue* item)
{
    xl6_error_scope operation={0}; ArrayView view={0}; int64_t at=-2; bool ok=false;
    if (!xl6ErrorScopeBegin(&operation)) return false;
    if (!item) xrtSetErrorKind(XERR_ARGUMENT);
    else if (begin(source,&view)) at=find(&view,item,&operation);
    if (at==-2 || xrtGetError()) xl6ErrorScopeCaptureFailure(&operation);
    if (!operation.failed && at>=0) {
        ok=view.owner ? xrtTypedArrayRemove(view.storage,(size_t)at,1) :
            xrtValueArrayRemove(view.wire,(size_t)at,1);
        if (!ok || xrtGetError()) xl6ErrorScopeCaptureFailure(&operation);
    }
    finish(&view);
    xl6_error_result outcome=xl6ErrorScopeEnd(&operation);
    if (!outcome.ok) xl6ErrorScopePropagateFailure(outcome);
    return outcome.ok && ok;
}
