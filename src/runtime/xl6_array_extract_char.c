#include "src/runtime/xl6_array_extract_view.h"
#include "src/runtime/xl6_scalar_type_abi.h"
uint32_t xl6ArrayExtractCharV1(const xvalue* source,uint64_t identity,
    uint64_t element,int64_t index,bool pop)
{
    XL6_ArrayExtractView view={0}; uint32_t result=0; bool ok=false;
    if (xl6ArrayExtractBegin(source,identity,element,index,pop,&view)) {
        if (view.owner) {
            if (view.layout.transfer!=1 || view.layout.size!=sizeof(result) ||
                !xl6CharTypeAbiV1(view.layout.type)) xrtSetErrorKind(XERR_TYPE);
            else ok=xrtTypedArrayTake(xl6SequenceStorageV1(view.owner),view.at,&result);
        } else {
            ok=xrtValueGetChar(view.item,&result);
            if (!ok && !xrtGetError()) xrtSetErrorKind(XERR_TYPE);
            if (ok) ok=xl6ArrayExtractWireCommit(&view);
        }
        if (!ok && !xrtGetError()) xrtSetErrorKind(XERR_STATE);
    }
    return xl6ArrayExtractFinish(&view,ok) ? result : 0;
}
