#include "src/runtime/xl6_array_value.h"
#include "src/runtime/xl6_array_reference_value.h"

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
        transfer==3 ? xl6ArrayReferenceConverterV1() : converter);
    return xl6SequenceWrapV1(storage,LAYOUT);
}
XL6_TEXT_API xvalue* xl6ArrayToValueV1(const xvalue* source, PARAMS, const xvalueconverter* converter)
{
    if (!converterValid(transfer,converter)) return NULL;
    XL6_Sequence* owner=xl6SequenceBeginRoleV1(source,XL6_SEQUENCE_ARRAY_V1,LAYOUT);
    if (!owner) return NULL;
    /* Callback execution may release the last source shell or attempt reentry.
     * Sequence's operation pin keeps storage alive and rejects mutation. */
    xvalue* result=xrtTypedArrayToValue(xl6SequenceStorageV1(owner),
        transfer==3 ? xl6ArrayReferenceConverterV1() : converter);
    xl6SequenceEndV1(owner); return result;
}
#undef LAYOUT
#undef PARAMS
