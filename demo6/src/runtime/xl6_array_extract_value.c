#include "src/runtime/xl6_array_extract_view.h"
#include "src/runtime/xl6_sequence_projection.h"

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
