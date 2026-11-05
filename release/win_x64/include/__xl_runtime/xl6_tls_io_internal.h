#ifndef XL6_TLS_IO_INTERNAL_H
#define XL6_TLS_IO_INTERNAL_H
#include "xl6_tls_transport_internal.h"
#include "xl6_tls_io_abi.h"
#include "xl6_concurrent_deadline.h"
#include "xl6_future_callback.h"
#include "xl6_future_payload.h"
#include "xl6_native_class_result.h"
/* All inputs below are call-only views. XRT copies send data synchronously
 * before publishing its operation, never retaining a language bytes/string. */
typedef enum XL6_TlsIoKindV1 {XL6_TLS_IO_RECEIVE_V1,XL6_TLS_IO_WAIT_V1,XL6_TLS_IO_SEND_V1} XL6_TlsIoKindV1;
static inline bool xl6TlsIoArgumentV1(XL6_TlsIoKindV1 kind,int64_t argument)
{
    bool ok=kind==XL6_TLS_IO_WAIT_V1?(argument>=XTLS_STREAM_WAIT_OPEN&&argument<=XTLS_STREAM_WAIT_CLOSE):
        (argument>=0&&(uint64_t)argument<=SIZE_MAX);
    if(!ok)xrtSetErrorKind(XERR_RANGE);
    return ok;
}
XL6_EXECUTION_INTERNAL struct xfuture* xl6TlsIoSourceV1(XL6_TlsIoKindV1,void*,int64_t,xbytesview);
XL6_EXECUTION_INTERNAL void* xl6TlsIoFuturePrepareV1(XL6_TlsIoKindV1,void*,int64_t,xbytesview);
/* Private SEND capture only, before Start/Publish. The native gather-copy may
 * complete first; no completion can read this slot before it is published. */
XL6_EXECUTION_INTERNAL bool xl6TlsIoFutureSetSendSizeV1(void*,size_t);
XL6_EXECUTION_INTERNAL struct xfuture* xl6TlsFutureFailV1(void*,struct xfuture*);
XL6_EXECUTION_INTERNAL struct xfuture* xl6TlsFuturePublishV1(XL6_RuntimeServicesV1,void*,struct xfuture*);
XL6_EXECUTION_INTERNAL bool xl6TlsIoMayWaitV1(XL6_ExecutionResourceV1*);
XL6_EXECUTION_INTERNAL bool xl6TlsIoWaitResultV1(struct xfuture*,uint64_t,xcancel*,struct xfutureresult*);
XL6_EXECUTION_INTERNAL bool xl6TlsIoPreWaitV1(uint64_t,xcancel*);
XL6_EXECUTION_INTERNAL void xl6TlsIoSourceDropV1(struct xfuture*,bool);
XL6_EXECUTION_INTERNAL xvalue* xl6TlsRecvSyncV1(void*,int64_t,uint64_t,xcancel*);
XL6_EXECUTION_INTERNAL bool xl6TlsWaitSyncV1(void*,int64_t,uint64_t,xcancel*);
XL6_EXECUTION_INTERNAL int64_t xl6TlsSendSyncV1(void*,xbytesview,uint64_t,xcancel*);
XL6_EXECUTION_INTERNAL void* xl6TlsAcceptSyncV1(XL6_ExecutionServicesV1,void*,uint64_t,xcancel*);
XL6_EXECUTION_INTERNAL void* xl6TlsAcceptFuturePrepareV1(XL6_NativeClassResultContextV1,void*);
XL6_EXECUTION_INTERNAL struct xfuture* xl6TlsAcceptSourceV1(XL6_ExecutionServicesV1,void*,void*);
static inline bool xl6TlsTextInputV1(xstrview input,int64_t size,xbytesview* view)
{*view=(xbytesview){(const uint8_t*)input.Data,input.Size};return xl6BufferSelect(view,size);}
static inline bool xl6TlsBytesInputV1(const xvalue* input,int64_t size,xbytesview* view)
{return xl6BufferBytesInput(input,size,view);}
static inline bool xl6TlsBufferInputV1(void* input,int64_t size,xbytesview* view)
{return xl6BufferInput(input,size,view);}
#endif
