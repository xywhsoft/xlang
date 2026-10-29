#ifndef XL6_ARRAY_METHOD_VIEW_H
#define XL6_ARRAY_METHOD_VIEW_H
#include "xl6_array_methods.h"
#include "xl6_sequence.h"
#include <limits.h>

typedef struct ArrayView {
    XL6_Sequence* owner;
    xtypedarray* storage;
    const xvalueconverter* converter;
    xvalue* wire;
} ArrayView;

static inline bool begin(const xvalue* source, ArrayView* view)
{
    if (!source) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
    XL6_SequenceLayoutV1 layout;
    int status = xl6SequenceBeginErasedArrayV1(source, &view->owner,
        &layout, &view->converter);
    if (status < 0) return false;
    if (status) {
        view->storage = xl6SequenceStorageV1(view->owner);
        return true;
    }
    if (xrtValueType(source) != XVALUE_ARRAY) {
        xrtSetErrorKind(XERR_TYPE); return false;
    }
    view->wire = xrtValueRetain(source);
    return view->wire != NULL;
}

static inline void finish(ArrayView* view)
{
    if (view->owner) xl6SequenceEndV1(view->owner);
    if (view->wire) {
        xerror* prior = xrtTakeError();
        xrtValueRelease(view->wire);
        if (prior) { xrtClearError(); xrtSetErrorTake(prior); }
    }
}

static inline size_t count(const ArrayView* view)
{ return view->owner ? xrtTypedArrayCount(view->storage) : xrtValueCount(view->wire); }

static inline bool extent(int64_t value)
{
    if (value >= 0 && (uint64_t)value <= SIZE_MAX) return true;
    xrtSetErrorKind(XERR_RANGE); return false;
}

static inline bool allocated(bool ok)
{
    if (!ok && !xrtGetError()) xrtSetErrorKind(XERR_MEMORY);
    return ok;
}

static inline bool arrayIndex(const ArrayView* view, int64_t value, bool insertion)
{
    if (!extent(value)) return false;
    size_t size = count(view), at = (size_t)value;
    if (insertion ? at <= size : at < size) return true;
    xrtSetErrorKind(XERR_RANGE); return false;
}

#endif
