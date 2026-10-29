#include "src/runtime/xl6_array_value.h"
#include "src/runtime/xl6_array_reference_value.h"
#include "src/runtime/xl6_sequence_projection.h"
#include <limits.h>

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
    if (xrtTypedArrayCount(xl6SequenceStorageV1(owner))>(size_t)INT64_MAX) {
        xrtSetErrorKind(XERR_RANGE); xl6SequenceEndV1(owner); return NULL;
    }
    /* Callback execution may release the last source shell or attempt reentry.
     * Sequence's operation pin keeps storage alive and rejects mutation.
     * Bind each independently owned scalar BEFORE transferring it to the
     * snapshot. A second pass over retained children would violate the first
     * binding's unique, unpublished-owner requirement. */
    xtypedarray* storage=xl6SequenceStorageV1(owner);
    size_t count=xrtTypedArrayCount(storage);
    const xvalueconverter* codec=transfer==3 ? xl6ArrayReferenceConverterV1() : converter;
    xvalue* result=xrtValueArray();
    xvalue* item=NULL;
    bool ok=result && xrtValueReserve(result,count);
    for (size_t i=0;ok && i<count;++i) {
        item=xl6SequenceProjectionTakeV1(owner,xrtTypedArrayGetValue(storage,i,codec));
        ok=item && xrtValueArrayAppendTake(result,&item);
    }
    if (!ok) {
        xerror* error=xrtTakeError();
        xrtValueRelease(item);
        xrtValueRelease(result);
        result=NULL;
        xrtClearError(); xrtSetErrorTake(error);
    }
    xl6SequenceEndV1(owner); return result;
}
#undef LAYOUT
#undef PARAMS
