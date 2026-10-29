#include "src/runtime/xl6_array_string_read.h"
#include "src/runtime/xl6_array_index.h"

xstrview xl6ArrayReadStringV1(const xvalue* source, uint64_t identity,
    uint64_t elementIdentity, int64_t index)
{
    xstrview result={0}, view={0};
    if (!source || !identity || !elementIdentity) {
        xrtSetErrorKind(XERR_ARGUMENT); return result;
    }
    XL6_Sequence* owner=NULL; xvalue* wireItem=NULL; XL6_SequenceLayoutV1 layout;
    const xvalueconverter* converter=NULL;
    int status=xl6SequenceBeginErasedArrayV1(source,&owner,&layout,&converter);
    if (status<0) return result;
    if (status) {
        if (layout.identity!=identity || layout.elementIdentity!=elementIdentity ||
            layout.transfer!=2 || layout.size!=sizeof(xstrview) ||
            (layout.type!=xl6StringTypeV1() && layout.type!=xrtTypeStringView())) {
            xrtSetErrorKind(XERR_TYPE); goto done;
        }
        xtypedarray* storage=xl6SequenceStorageV1(owner); size_t at;
        if (!xl6ArrayIndexV1(index,xrtTypedArrayCount(storage),false,&at)) goto done;
        const xstrview* slot=xrtTypedArrayConstGet(storage,at);
        if (!slot || !slot->Data) { xrtSetErrorKind(XERR_TYPE); goto done; }
        view=*slot;
    } else {
        if (xrtValueType(source)!=XVALUE_ARRAY) { xrtSetErrorKind(XERR_TYPE); return result; }
        size_t at;
        if (!xl6ArrayIndexV1(index,xrtValueCount(source),false,&at)) return result;
        wireItem=xrtValueRetain(xrtValueArrayGet(source,at));
        if (!wireItem || !xrtValueGetString(wireItem,&view)) {
            if (!xrtGetError()) xrtSetErrorKind(XERR_TYPE);
            goto done;
        }
    }
    /* The native pin spans allocation and copy. Reentrant allocators cannot
     * mutate or retire the authoritative source slot while it is observed. */
    result.Data=xrtStrDupN(view.Data,view.Size);
    if (result.Data) result.Size=view.Size;
    else if (!xrtGetError()) xrtSetErrorKind(XERR_MEMORY);
done:
    if (owner) xl6SequenceEndV1(owner);
    if (wireItem) {
        xerror* saved=xrtTakeError(); xrtValueRelease(wireItem);
        if (saved) { xrtClearError(); xrtSetErrorTake(saved); }
    }
    return result;
}
