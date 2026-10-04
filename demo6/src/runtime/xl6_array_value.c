#include "src/runtime/xl6_array_value.h"

static bool toReference(const xvalue* source, const xrttype* type, ptr target, ptr context)
{
    (void)context;
    if (!source || !target) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
    if (type != xl6SequenceStorageTypeV1(3,xrtTypeValue())) {
        xrtSetErrorKind(XERR_TYPE); return false;
    }
    /* Use the shared descriptor's mutation scope and transactional retain,
     * never xrtTypeValue's graph-cloning dynamic value contract. */
    return xrtTypeCopyValue(type,target,&source);
}
static xvalue* fromReference(const xrttype* type, const void* source, ptr context)
{
    (void)context;
    if (!source) { xrtSetErrorKind(XERR_ARGUMENT); return NULL; }
    if (type != xl6SequenceStorageTypeV1(3,xrtTypeValue())) {
        xrtSetErrorKind(XERR_TYPE); return NULL;
    }
    const xvalue* item=*(xvalue* const*)source;
    return item ? xrtValueRetain(item) : xrtValueNull();
}
static const xvalueconverter referenceConverter={.ToTyped=toReference,.FromTyped=fromReference};
static bool converterValid(unsigned int transfer, const xvalueconverter* converter)
{
    if ((transfer==3 && converter) ||
        (converter && (!converter->ToTyped || !converter->FromTyped))) {
        xrtSetErrorKind(XERR_ARGUMENT); return false;
    }
    return true;
}
#define PARAMS uint64_t identity, uint64_t elementIdentity, unsigned int transfer, size_t size, const xrttype* type
#define LAYOUT identity,elementIdentity,transfer,size,type
XL6_TEXT_API xvalue* xl6ArrayFromValueV1(const xvalue* source, PARAMS, const xvalueconverter* converter)
{
    if (!xl6SequenceTupleV1(LAYOUT) || !converterValid(transfer,converter)) return NULL;
    xtypedarray* storage=xrtTypedArrayFromValue(source,xl6SequenceStorageTypeV1(transfer,type),
        transfer==3 ? &referenceConverter : converter);
    return xl6SequenceWrapV1(storage,LAYOUT);
}
XL6_TEXT_API xvalue* xl6ArrayToValueV1(const xvalue* source, PARAMS, const xvalueconverter* converter)
{
    if (!converterValid(transfer,converter)) return NULL;
    XL6_Sequence* owner=xl6SequenceBeginV1(source,LAYOUT);
    if (!owner) return NULL;
    /* Callback execution may release the last source shell or attempt reentry.
     * Sequence's operation pin keeps storage alive and rejects mutation. */
    xvalue* result=xrtTypedArrayToValue(xl6SequenceStorageV1(owner),
        transfer==3 ? &referenceConverter : converter);
    xl6SequenceEndV1(owner); return result;
}
#undef LAYOUT
#undef PARAMS
