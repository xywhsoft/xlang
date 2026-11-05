#ifndef XL6_TCP_RECEIVE_INTO_INTERNAL_H
#define XL6_TCP_RECEIVE_INTO_INTERNAL_H
#include "xl6_tcp_internal.h"
#include "xl6_net_buffer_internal.h"
#include "xl6_native_class_abi.h"
#include "xl6_runtime_services.h"
/* Trusted family wrappers supply either NetBuffer or the embedded FIRST input
 * record of a line/length framer. No reinterpretation of arbitrary user types. */
bool xl6TcpReceiveIntoAppendInternalV1(void*,xbytesview);
bool xl6TcpReceiveIntoPrepareInternalV1(void*,size_t*);
xfuture* xl6TcpReceiveIntoStartInternalV1(XL6_RuntimeServicesV1,XL6_NativeClassFieldContextV1,
    void*,const xvalue*,int64_t,const XL6_NativeHandlePolicyV1*);
#endif
