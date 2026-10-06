#ifndef XL6_ARRAY_EXTRACT_VIEW_H
#define XL6_ARRAY_EXTRACT_VIEW_H
#include "src/runtime/xl6_array_extract.h"
#include "src/runtime/xl6_extract_error.h"
#include "src/runtime/xl6_sequence.h"

typedef struct XL6_ArrayExtractView {
    XL6_Sequence* owner;
    XL6_SequenceLayoutV1 layout;
    const xvalueconverter* converter;
    xvalue* wire;
    xvalue* item;
    size_t at;
    xerror* prior;
} XL6_ArrayExtractView;

static inline bool xl6ArrayExtractFinish(XL6_ArrayExtractView* view,bool ok)
{
    xerror* failure=xrtTakeError();
    if (view->item) { xrtValueRelease(view->item); xl6ExtractKeepFirst(&failure); }
    if (view->wire) { xrtValueRelease(view->wire); xl6ExtractKeepFirst(&failure); }
    if (view->owner) { xl6SequenceEndV1(view->owner); xl6ExtractKeepFirst(&failure); }
    ok=ok && !failure;
    if (view->prior) { xrtErrorFree(failure); xrtSetErrorTake(view->prior); }
    else xrtSetErrorTake(failure);
    return ok;
}
static inline bool xl6ArrayExtractBegin(const xvalue* source,uint64_t identity,
    uint64_t element,int64_t index,bool pop,XL6_ArrayExtractView* view)
{
    view->prior=xrtTakeError();
    if (!source || !identity || !element || (pop && index)) {
        xrtSetErrorKind(XERR_ARGUMENT); return false;
    }
    int status=xl6SequenceBeginErasedArrayV1(source,&view->owner,&view->layout,&view->converter);
    if (status<0) return false;
    if (status) {
        if (view->layout.identity!=identity || view->layout.elementIdentity!=element) {
            xrtSetErrorKind(XERR_TYPE); return false;
        }
    } else {
        if (xrtValueType(source)!=XVALUE_ARRAY) { xrtSetErrorKind(XERR_TYPE); return false; }
        view->wire=xrtValueRetain(source);
        if (!view->wire) { if (!xrtGetError()) xrtSetErrorKind(XERR_MEMORY); return false; }
    }
    size_t count=view->owner ? xrtTypedArrayCount(xl6SequenceStorageV1(view->owner)) : xrtValueCount(view->wire);
    if (!count || (!pop && (index<0 || (uint64_t)index>SIZE_MAX || (size_t)index>=count))) {
        xrtSetErrorKind(XERR_RANGE); return false;
    }
    view->at=pop ? count-1u : (size_t)index;
    /* Admission precedes conversion and removal, even for reference policies
     * whose boxed identity is intentionally zero. Never bless or extract a
     * corrupted native slot merely because its physical descriptor matches. */
    if (view->owner && !xl6SequenceAdmitSlotV1(view->owner,
        xrtTypedArrayConstGet(xl6SequenceStorageV1(view->owner),view->at))) return false;
    if (view->wire) {
        const xvalue* item=xrtValueArrayGet(view->wire,view->at);
        view->item=item ? xrtValueRetain(item) : NULL;
        if (!view->item) { if (!xrtGetError()) xrtSetErrorKind(XERR_STATE); return false; }
    }
    return true;
}
static inline bool xl6ArrayExtractSlotMatches(const XL6_ArrayExtractView* view,
    unsigned transfer,size_t size,const xrttype* type)
{
    if (view->layout.transfer==transfer && view->layout.size==size && view->layout.type==type) return true;
    xrtSetErrorKind(XERR_TYPE); return false;
}
static inline bool xl6ArrayExtractWireCommit(XL6_ArrayExtractView* view)
{
    xvalue* removed=xrtValueArrayTake(view->wire,view->at);
    if (!removed) { if (!xrtGetError()) xrtSetErrorKind(XERR_MEMORY); return false; }
    xerror* failure=xrtTakeError(); xrtValueRelease(removed);
    xl6ExtractKeepFirst(&failure); xrtSetErrorTake(failure);
    return !failure;
}
#endif
