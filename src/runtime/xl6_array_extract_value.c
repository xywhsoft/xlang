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

/* Prepare and authenticate the retained Error before committing removal.
 * The collection and its original diagnostic survive every preparation failure. */
xerror* xl6ArrayExtractErrorV1(const xvalue* source,uint64_t identity,
    uint64_t element,int64_t index,bool pop,bool nullable)
{
    XL6_ArrayExtractView view={0};xerror* result=NULL;bool ok=false;
    if(!xl6ArrayExtractBegin(source,identity,element,index,pop,&view)) goto done;
    if(view.owner) {
        view.item=xl6SequenceProjectionTakeV1(view.owner,
            xrtTypedArrayGetValue(xl6SequenceStorageV1(view.owner),view.at,view.converter));
        if(!view.item) goto done;
    }
    if(!xl6ErrorValueReadRefV1(view.item,element,&result)) goto done;
    if(!nullable && !result) {xrtSetErrorKind(XERR_TYPE);goto done;}
    ok=view.owner ? xrtTypedArrayRemove(xl6SequenceStorageV1(view.owner),view.at,1u) : xl6ArrayExtractWireCommit(&view);
done:
    if(!xl6ArrayExtractFinish(&view,ok)) {xrtErrorFree(result);result=NULL;}
    return result;
}
