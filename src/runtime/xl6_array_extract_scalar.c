#include "src/runtime/xl6_array_extract_view.h"

#define EXTRACT(Name,CType,Type,Get) \
CType Name(const xvalue* source,uint64_t identity,uint64_t element,int64_t index,bool pop) \
{ \
    XL6_ArrayExtractView view={0}; CType result=0; bool ok=false; \
    if (xl6ArrayExtractBegin(source,identity,element,index,pop,&view)) { \
        if (view.owner) { \
            ok=xl6ArrayExtractSlotMatches(&view,1,sizeof(result),Type()) && \
                xrtTypedArrayTake(xl6SequenceStorageV1(view.owner),view.at,&result); \
        } else { \
            ok=Get(view.item,&result); \
            if (!ok && !xrtGetError()) xrtSetErrorKind(XERR_TYPE); \
            if (ok) ok=xl6ArrayExtractWireCommit(&view); \
        } \
        if (!ok && !xrtGetError()) xrtSetErrorKind(XERR_STATE); \
    } \
    return xl6ArrayExtractFinish(&view,ok) ? result : (CType)0; \
}
EXTRACT(xl6ArrayExtractBoolV1,bool,xrtTypeBool,xrtValueGetBool)
EXTRACT(xl6ArrayExtractI64V1,int64_t,xrtTypeInt64,xrtValueGetInt)
EXTRACT(xl6ArrayExtractU64V1,uint64_t,xrtTypeUInt64,xrtValueGetUInt)
EXTRACT(xl6ArrayExtractF64V1,double,xrtTypeFloat64,xrtValueGetFloat)
EXTRACT(xl6ArrayExtractTimeV1,int64_t,xrtTypeTime,xrtValueGetTime)
#undef EXTRACT
