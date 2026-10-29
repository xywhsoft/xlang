#include "src/runtime/xl6_extract_error.h"

xvalue* xl6CollectionExtractValueV1(const xvalue* source,unsigned int carrier,
    int64_t key,xstrview text)
{
    xvalue* receiver=NULL; xvalue* item=NULL; xvalue* result=NULL;
    xerror* prior=xrtTakeError(); xerror* failure=NULL;
    if (!source || (carrier!=XL6_COLLECTION_EXTRACT_LIST_V1 && carrier!=XL6_COLLECTION_EXTRACT_DICT_V1)) {
        xrtSetErrorKind(XERR_ARGUMENT); goto done;
    }
    if (xrtValueType(source)!=(carrier==XL6_COLLECTION_EXTRACT_LIST_V1 ? XVALUE_INT_MAP : XVALUE_OBJECT)) {
        xrtSetErrorKind(XERR_TYPE); goto done;
    }
    if (carrier==XL6_COLLECTION_EXTRACT_DICT_V1 && (!text.Data && text.Size)) {
        xrtSetErrorKind(XERR_ARGUMENT); goto done;
    }
    receiver=xrtValueRetain(source);
    if (!receiver) { if (!xrtGetError()) xrtSetErrorKind(XERR_MEMORY); goto done; }
    const xvalue* found=carrier==XL6_COLLECTION_EXTRACT_LIST_V1 ? xrtValueIntMapGet(receiver,key) :
        xrtValueObjectGet(receiver,text);
    if (!found) {
        if (!xrtGetError()) result=xrtValueRetain(xrtValueNull());
    } else {
        item=xrtValueRetain(found);
        if (item) result=carrier==XL6_COLLECTION_EXTRACT_LIST_V1 ? xrtValueIntMapTake(receiver,key) :
            xrtValueObjectTake(receiver,text);
        if (!result && !xrtGetError()) xrtSetErrorKind(XERR_MEMORY);
    }
done:
    failure=xrtTakeError();
    if (item) { xrtValueRelease(item); xl6ExtractKeepFirst(&failure); }
    if (receiver) { xrtValueRelease(receiver); xl6ExtractKeepFirst(&failure); }
    if (failure && result) { xrtValueRelease(result); result=NULL; xl6ExtractKeepFirst(&failure); }
    if (prior) { xrtErrorFree(failure); xrtSetErrorTake(prior); }
    else xrtSetErrorTake(failure);
    return result;
}
