#include "src/runtime/xl6_collection_count.h"
#include "src/runtime/xl6_sequence_projection.h"
#include <limits.h>

static bool count(const xvalue* source, int64_t* output)
{
    if (!source) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
    XL6_Sequence* owner=NULL;
    XL6_SequenceLayoutV1 layout;
    const xvalueconverter* converter=NULL;
    int status=xl6SequenceBeginErasedArrayV1(source,&owner,&layout,&converter);
    if (status<0) return false;
    size_t size;
    if (status) {
        size=xrtTypedArrayCount(xl6SequenceStorageV1(owner));
        xl6SequenceEndV1(owner);
    } else {
        xvaluetype type=xrtValueType(source);
        if (type!=XVALUE_ARRAY && type!=XVALUE_INT_MAP && type!=XVALUE_OBJECT && type!=XVALUE_SET) {
            xrtSetErrorKind(XERR_TYPE); return false;
        }
        size=xrtValueCount(source);
    }
    if (size>INT64_MAX) { xrtSetErrorKind(XERR_RANGE); return false; }
    *output=(int64_t)size;
    return true;
}
int64_t xl6CollectionCountV1(const xvalue* source)
{
    int64_t result=0;
    (void)count(source,&result);
    return result;
}
bool xl6CollectionCountIfPresentV1(const xvalue* source, int64_t* output)
{
    if (!output) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
    return source && xrtValueType(source)!=XVALUE_NULL && count(source,output);
}
xvalue* xl6CollectionCountOrNullV1(const xvalue* source)
{
    if (!source || xrtValueType(source)==XVALUE_NULL) return xrtValueRetain(xrtValueNull());
    int64_t size;
    if (!count(source,&size)) return NULL;
    xvalue* result=xrtValueInt(size);
    if (!result && !xrtGetError()) xrtSetErrorKind(XERR_MEMORY);
    return result;
}
