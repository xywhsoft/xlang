#include "src/runtime/xl6_array_method_view.h"
#include "src/runtime/xl6_sequence_projection.h"

/* -1 not found, -2 failure. Never remove a candidate before its projection
 * and equality finish; allocator/converter reentry cannot mutate native slots. */
static int64_t find(ArrayView* view, const xvalue* item)
{
    size_t size = count(view);
    if (size > INT64_MAX) { xrtSetErrorKind(XERR_RANGE); return -2; }
    for (size_t at = 0; at < size; ++at) {
        xvalue* current = view->owner
            ? xl6SequenceProjectionTakeV1(view->owner,
                xrtTypedArrayGetValue(view->storage, at, view->converter))
            : xrtValueRetain(xrtValueArrayGet(view->wire, at));
        if (!current) {
            if (!xrtGetError()) xrtSetErrorKind(view->owner ? XERR_MEMORY : XERR_STATE);
            return -2;
        }
        xerror* prior = xrtTakeError();
        bool equal = xrtValueEqual(current, item);
        xerror* failure = xrtTakeError();
        xrtValueRelease(current);
        xerror* cleanup = xrtTakeError();
        if (failure) xrtErrorFree(cleanup);
        else failure = cleanup;
        bool failed = failure != NULL;
        if (prior) { xrtErrorFree(failure); xrtSetErrorTake(prior); }
        else xrtSetErrorTake(failure);
        if (failed) return -2;
        if (equal) return (int64_t)at;
    }
    return -1;
}

int64_t xl6ArrayMethodIndexOfV1(const xvalue* source, const xvalue* item)
{
    if (!item) { xrtSetErrorKind(XERR_ARGUMENT); return -1; }
    ArrayView view = {0};
    if (!begin(source, &view)) return -1;
    int64_t at = find(&view, item);
    finish(&view); return at >= 0 ? at : -1;
}


bool xl6ArrayMethodRemoveValueV1(xvalue* source, const xvalue* item)
{
    if (!item) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
    ArrayView view = {0};
    if (!begin(source, &view)) return false;
    int64_t at = find(&view, item);
    bool ok = at >= 0 && (view.owner ? xrtTypedArrayRemove(view.storage, (size_t)at, 1)
        : xrtValueArrayRemove(view.wire, (size_t)at, 1));
    finish(&view); return ok;
}
