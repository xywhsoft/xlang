#include "src/runtime/xl6_sequence.h"
#include "src/runtime/xl6_array_value_read.h"
#include "src/runtime/xl6_array_index.h"
#include "src/runtime/xl6_sequence_projection.h"

static xvalue* missing(void) { return xrtValueRetain(xrtValueNull()); }
static xvalue* read(const xvalue* source,int64_t index,bool safe,bool otherIsNull)
{
    if (!source || xrtValueType(source)==XVALUE_NULL) {
        if (safe) return missing();
        xrtSetErrorKind(XERR_ARGUMENT); return NULL;
    }
    XL6_Sequence* owner=NULL; XL6_SequenceLayoutV1 layout;
    const xvalueconverter* converter=NULL;
    int status=xl6SequenceBeginErasedArrayV1(source,&owner,&layout,&converter);
    if (status<0) return NULL;
    if (!status && xrtValueType(source)!=XVALUE_ARRAY) {
        if (otherIsNull) return missing();
        xrtSetErrorKind(XERR_TYPE); return NULL;
    }
    xtypedarray* storage=owner ? xl6SequenceStorageV1(owner) : NULL;
    size_t count=owner ? xrtTypedArrayCount(storage) : xrtValueCount(source), at;
    xvalue* result=NULL;
    if (xl6ArrayIndexTryV1(index,count,false,&at)) {
        result=owner ? xl6SequenceProjectionTakeV1(owner,xrtTypedArrayGetValue(storage,at,converter)) :
            xrtValueRetain(xrtValueArrayGet(source,at));
        if (!result && !xrtGetError()) xrtSetErrorKind(owner ? XERR_MEMORY : XERR_STATE);
    } else if (safe) result=missing();
    else xrtSetErrorKind(XERR_RANGE);
    if (owner) xl6SequenceEndV1(owner);
    return result;
}
xvalue* xl6ArrayValueReadCopyV1(const xvalue* source,int64_t index)
{ return read(source,index,false,false); }
xvalue* xl6ArrayValueReadOrNullCopyV1(const xvalue* source,int64_t index)
{ return read(source,index,true,false); }
xvalue* xl6ArrayValueReadIfArrayCopyV1(const xvalue* source,int64_t index)
{ return read(source,index,true,true); }
