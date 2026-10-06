#include "src/runtime/xl6_array_extract_view.h"
#include "src/runtime/xl6_string_type.h"
#include "src/runtime/xl6_nullable_string.h"

xstrview xl6ArrayExtractStringV1(const xvalue* source,uint64_t identity,
    uint64_t element,int64_t index,bool pop,bool nullable)
{
    XL6_ArrayExtractView view={0}; xstrview result={0}; bool ok=false;
    if (!xl6ArrayExtractBegin(source,identity,element,index,pop,&view)) goto done;
    if (view.owner) {
        const xrttype* type=view.layout.type;
        if (view.layout.transfer!=2 || view.layout.size!=sizeof(result) ||
            !(nullable ? xl6NullableStringTypeAbiV1(type) : xl6StringTypeAbiV1(type)) ) {
            xrtSetErrorKind(XERR_TYPE); goto done;
        }
        ok=xrtTypedArrayTake(xl6SequenceStorageV1(view.owner),view.at,&result);
    } else {
        xstrview text={0};
        if (nullable && xrtValueType(view.item)==XVALUE_NULL) ok=true;
        else if (!xrtValueGetString(view.item,&text)) xrtSetErrorKind(XERR_TYPE);
        else {
            result.Data=xrtStrDupN(text.Data,text.Size);
            if (result.Data) { result.Size=text.Size; ok=true; }
            else if (!xrtGetError()) xrtSetErrorKind(XERR_MEMORY);
        }
        if (ok) ok=xl6ArrayExtractWireCommit(&view);
    }
    if (!ok && !xrtGetError()) xrtSetErrorKind(XERR_STATE);
done:
    if (!xl6ArrayExtractFinish(&view,ok)) {
        if (result.Data) xrtFree((void*)result.Data);
        result=(xstrview){0};
    }
    return result;
}
