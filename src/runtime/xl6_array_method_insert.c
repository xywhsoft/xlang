#include "src/runtime/xl6_array_method_view.h"

bool xl6ArrayMethodInsertV1(xvalue* source, int64_t at, const xvalue* item)
{
    if (!item) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
    ArrayView view = {0};
    if (!begin(source, &view)) return false;
    bool ok = index(&view, at, true) && (view.owner
        ? xl6SequenceAdmitValueV1(view.owner, item) &&
            allocated(xrtTypedArrayInsertValue(view.storage, (size_t)at, item, view.converter))
        : allocated(xrtValueArrayInsert(view.wire, (size_t)at, item)));
    finish(&view); return ok;
}
