#ifndef XL6_TLS_DATA_INTERNAL_H
#define XL6_TLS_DATA_INTERNAL_H
#include "xl6_tls_io_internal.h"
#include "xl6_tls_data_abi.h"
#include "xl6_net_vector_input.h"
XL6_EXECUTION_INTERNAL struct xfuture* xl6TlsVectorSourceV1(XL6_NetVectorCaptureV1,
    XL6_NativeClassArrayContextV1,void*,const xvalue*,int64_t,size_t*);
XL6_EXECUTION_INTERNAL void* xl6TlsRecvOwnedFuturePrepareV1(XL6_NativeClassResultContextV1,void*,int64_t);
XL6_EXECUTION_INTERNAL void* xl6TlsRecvOwnedSyncV1(void*,int64_t,uint64_t,xcancel*);
XL6_EXECUTION_INTERNAL int64_t xl6TlsVectorSyncV1(XL6_NetVectorCaptureV1,XL6_NativeClassArrayContextV1,
    void*,const xvalue*,int64_t,uint64_t,xcancel*);
XL6_EXECUTION_INTERNAL struct xfuture* xl6TlsWorkerSourceV1(void*,int64_t,unsigned);
enum {XL6_TLS_WORKER_PEEK_V1,XL6_TLS_WORKER_CONSUME_V1,XL6_TLS_WORKER_SEND_BOUND_V1,XL6_TLS_WORKER_PROTOCOL_V1};
#endif
