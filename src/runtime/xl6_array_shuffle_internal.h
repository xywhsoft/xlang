#ifndef XL6_ARRAY_SHUFFLE_INTERNAL_H
#define XL6_ARRAY_SHUFFLE_INTERNAL_H
#include "xl6_array_shuffle.h"
#include "xl6_sequence.h"
#include <limits.h>

/* Shared carrier admission/cleanup, independent of random-source selection.
 * The native pin owns the wrapper even if an allocator retires its last shell.
 * No raw storage address survives this synchronous operation. */
typedef struct XL6_ArrayShuffleTargetV1 {
    XL6_Sequence* owner;
    xvalue* wire;
    void* data;
    size_t count, size;
} XL6_ArrayShuffleTargetV1;

static inline bool xl6ArrayShuffleEndV1(XL6_ArrayShuffleTargetV1* target, bool ok)
{
    XL6_ArrayShuffleTargetV1 saved=*target; *target=(XL6_ArrayShuffleTargetV1){0};
    xerror* error=xrtTakeError();
    if (saved.owner) xl6SequenceEndV1(saved.owner);
    xrtValueRelease(saved.wire);
    if (error) { xrtClearError(); xrtSetErrorTake(error); }
    else if (xrtGetError()) ok=false;
    return ok;
}

static inline bool xl6ArrayShuffleBeginV1(const xvalue* source, XL6_ArrayShuffleTargetV1* target)
{
    XL6_SequenceLayoutV1 layout={0}; const xvalueconverter* converter=NULL;
    int status=xl6SequenceBeginErasedArrayV1(source,&target->owner,&layout,&converter);
    if (status<0) return false;
    if (status) {
        xtypedarray* storage=xl6SequenceStorageV1(target->owner);
        target->count=xrtTypedArrayCount(storage);
        target->size=layout.size;
        target->data=xrtTypedArrayData(storage);
        if (!target->size || target->count>SIZE_MAX/target->size ||
            (target->count && !target->data)) {
            xrtSetErrorKind(XERR_STATE); return xl6ArrayShuffleEndV1(target,false);
        }
    } else {
        if (!source || xrtValueType(source)!=XVALUE_ARRAY) {
            xrtSetErrorKind(XERR_TYPE); return false;
        }
        target->wire=xrtValueRetain(source);
        if (!target->wire) return false;
        target->count=xrtValueCount(target->wire);
    }
    /* Language Array length and indices are signed 64-bit on every host. */
    if (target->count>INT64_MAX) {
        xrtSetErrorKind(XERR_RANGE); return xl6ArrayShuffleEndV1(target,false);
    }
    return true;
}

/* Only the two resident random-source adapters call this synchronous helper.
 * Its function/context are never stored or supplied by generated/user code.
 * Wire slots use XRT's COW/ownership mutation operation, not byte swapping. */
static inline bool xl6ArrayShuffleWireV1(XL6_ArrayShuffleTargetV1* target,
    uint64_t (*below)(void*, uint64_t), void* context)
{
    for (size_t i=target->count;i>1;--i) {
        uint64_t at=below(context,(uint64_t)i);
        if (at>=(uint64_t)i || !xrtValueArraySwap(target->wire,i-1,(size_t)at)) return false;
    }
    return true;
}
#endif
