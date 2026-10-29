#ifndef XL6_UDP_INTERNAL_H
#define XL6_UDP_INTERNAL_H
#include "xl6_buffer_internal.h"
#include "xl6_execution_udp_internal.h"
#include "xl6_native_handle.h"
#include "src/api/xl_foreign_error.h"
static inline void xl6UdpFailedV1(void)
{xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});}
static inline bool xl6UdpEnterV1(void* raw,bool submit,XL6_UdpScopeV1* scope)
{bool ok=xl6ExecutionUdpEnterInternalV1(raw,submit,scope);if(!ok)xl6UdpFailedV1();return ok;}
static inline bool xl6UdpSliceV1(xbytesview* bytes,int64_t size,int64_t offset)
{
    if(offset<0||(uint64_t)offset>bytes->Size){xrtSetErrorKind(XERR_RANGE);return false;}
    if(bytes->Data)bytes->Data+=(size_t)offset;
    bytes->Size-=(size_t)offset;return xl6BufferSelect(bytes,size);
}
#endif
