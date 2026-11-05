#ifndef XL6_TLS_TRANSPORT_INTERNAL_H
#define XL6_TLS_TRANSPORT_INTERNAL_H
#include "xl6_execution_tls_internal.h"
#include "xl6_tls_role_context_internal.h"
#include "xl6_tls_transport_abi.h"
#include "xl6_buffer_internal.h"
static inline void xl6TlsTransportFailedV1(void)
{xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});}
static inline bool xl6TlsTransportNameV1(xstrview* name,int64_t size)
{
    xbytesview view={(const uint8*)name->Data,name->Size};
    if(!xl6BufferSelect(&view,size))return false;
    *name=(xstrview){(const char*)view.Data,view.Size};return true;
}
const XL6_NativeHandlePolicyV1* xl6TlsStreamHandlePolicyV1Get(void);
const XL6_NativeHandlePolicyV1* xl6TlsListenerHandlePolicyV1Get(void);
#endif
