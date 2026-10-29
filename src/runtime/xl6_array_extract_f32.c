#include "src/runtime/xl6_array_extract_view.h"
#include "src/runtime/xl6_value_admission.h"
float xl6ArrayExtractF32V1(const xvalue* source,uint64_t identity,
    uint64_t element,int64_t index,bool pop)
{
    XL6_ArrayExtractView view={0}; float result=0; bool ok=false;
    if (xl6ArrayExtractBegin(source,identity,element,index,pop,&view)) {
        if (view.owner) ok=xl6ArrayExtractSlotMatches(&view,1,sizeof(result),xrtTypeFloat32()) &&
            xrtTypedArrayTake(xl6SequenceStorageV1(view.owner),view.at,&result);
        else ok=xl6ValueReadF32V1(view.item,element,&result) && xl6ArrayExtractWireCommit(&view);
        if (!ok && !xrtGetError()) xrtSetErrorKind(XERR_STATE);
    }
    return xl6ArrayExtractFinish(&view,ok) ? result : 0;
}
