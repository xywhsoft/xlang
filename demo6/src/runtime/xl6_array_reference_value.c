#include "src/runtime/xl6_array_reference_value.h"

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
const xvalueconverter* xl6ArrayReferenceConverterV1(void) { return &referenceConverter; }
