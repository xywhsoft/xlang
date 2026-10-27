#include "src/runtime/xl6_array_extract_view.h"
#include "src/runtime/xl6_sequence_projection.h"
#include "src/runtime/xl6_error_value.h"

xvalue* xl6ArrayExtractValueV1(const xvalue* source,uint64_t identity,
    uint64_t element,int64_t index,bool pop)
{
    XL6_ArrayExtractView view={0}; xvalue* result=NULL; bool ok=false;
    if (!xl6ArrayExtractBegin(source,identity,element,index,pop,&view)) goto done;
    if (view.owner) {
        xtypedarray* storage=xl6SequenceStorageV1(view.owner);
        result=xl6SequenceProjectionTakeV1(view.owner,xrtTypedArrayGetValue(storage,view.at,view.converter));
        if (result) ok=xrtTypedArrayRemove(storage,view.at,1u);
    } else {
        result=xrtValueArrayTake(view.wire,view.at); ok=result!=NULL;
    }
    if (!ok && !xrtGetError()) xrtSetErrorKind(XERR_MEMORY);
done:
    if (!xl6ArrayExtractFinish(&view,ok)) { xl6ExtractRelease(result); result=NULL; }
    return result;
}

/* The admitted Error box owns an immutable resident reference. Decoding
 * retains that reference, never its text; releasing the extracted box retires
 * only the collection's ownership. Canonical null remains a valid payload. */
xerror* xl6ArrayExtractErrorV1(const xvalue* source,uint64_t identity,
    uint64_t element,int64_t index,bool pop,bool nullable)
{
    xvalue* box=xl6ArrayExtractValueV1(source,identity,element,index,pop);
    xerror* result=NULL;
    if (!box) return NULL;
    bool ok=xl6ErrorValueReadRefV1(box,element,&result);
    if (ok && !nullable && !result) { xrtSetErrorKind(XERR_TYPE); ok=false; }
    xl6ExtractRelease(box);
    if (!ok || xrtGetError()) {
        xrtErrorFree(result);
        return NULL;
    }
    return result;
}
