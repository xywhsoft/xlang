#include "src/runtime/xl6_list_typed_read.h"
#include "src/runtime/xl6_list_typed_read_internal.h"

#define READ(Name,CType,Type,Get) \
CType Name(const xvalue* source,uint64_t identity,uint64_t schema,uint64_t element,int64_t key) \
{ \
    CType result=0; xl6_error_scope scope={0}; \
    if (!xl6ErrorScopeBegin(&scope)) { \
        xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()}); return result; \
    } \
    const xvalue* wire=NULL; \
    int status=xl6ListTypedReadSlot(source,identity,schema,element,key,1,sizeof(result),Type(),&result,&wire); \
    bool success=status==1 || (status==0 && Get(wire,&result)); \
    if (status==0 && !success && !xrtGetError()) xrtSetErrorKind(XERR_TYPE); \
    return xl6ListTypedReadFinish(&scope,success) ? result : 0; \
}
READ(xl6ListReadI64V1,int64_t,xrtTypeInt64,xrtValueGetInt)
READ(xl6ListReadU64V1,uint64_t,xrtTypeUInt64,xrtValueGetUInt)
READ(xl6ListReadBoolV1,bool,xrtTypeBool,xrtValueGetBool)
READ(xl6ListReadF64V1,double,xrtTypeFloat64,xrtValueGetFloat)
#undef READ
