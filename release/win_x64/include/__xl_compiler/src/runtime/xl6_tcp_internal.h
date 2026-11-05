#ifndef XL6_TCP_INTERNAL_H
#define XL6_TCP_INTERNAL_H
#include "xl6_buffer_internal.h"
#include "xl6_execution_tcp_internal.h"
#include "xl6_native_handle.h"
#include "src/api/xl_foreign_error.h"
static inline void xl6TcpFailedV1(void)
{xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});}
static inline bool xl6TcpEnterV1(void* raw,XL6_ExecutionKindV1 kind,bool submit,XL6_TcpScopeV1* scope)
{bool ok=xl6ExecutionTcpEnterInternalV1(raw,kind,submit,scope);if(!ok)xl6TcpFailedV1();return ok;}
static inline bool xl6TcpPositiveSizeV1(int64_t value,size_t* output)
{
    if(value<=0||(uint64_t)value>SIZE_MAX){xrtSetErrorKind(XERR_RANGE);return false;}
    *output=(size_t)value;return true;
}
#endif
