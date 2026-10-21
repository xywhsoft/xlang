#ifndef XL6_TCP_RECV_OWNED_INTERNAL_H
#define XL6_TCP_RECV_OWNED_INTERNAL_H
#include "xl6_tcp_future_internal.h"
XL6_EXECUTION_INTERNAL XL6_TcpClassCaptureV1* xl6TcpRecvOwnedPrepareInternalV1(
    XL6_NativeClassFieldContextV1,void*,const xvalue*,int64_t,size_t*);
XL6_EXECUTION_INTERNAL struct xfuture* xl6TcpRecvOwnedSourceInternalV1(void*,size_t);
#endif
