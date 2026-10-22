#ifndef XL6_UDP_FUTURE_INTERNAL_H
#define XL6_UDP_FUTURE_INTERNAL_H
#include "xl6_udp_internal.h"
#include "xl6_native_class_result.h"
#include "xl6_future_callback.h"
typedef struct XL6_UdpFutureCaptureV1 XL6_UdpFutureCaptureV1;
typedef enum XL6_UdpFutureKindV1 {XL6_UDP_FUTURE_PACKET,XL6_UDP_FUTURE_ERROR,
    XL6_UDP_FUTURE_BATCH,XL6_UDP_FUTURE_WAIT,XL6_UDP_FUTURE_WRITABLE} XL6_UdpFutureKindV1;
XL6_EXECUTION_INTERNAL XL6_UdpFutureCaptureV1* xl6UdpFuturePrepareInternalV1(
    XL6_UdpFutureKindV1,void*,int64_t,const XL6_NativeClassResultContextV1*);
XL6_EXECUTION_INTERNAL xfuture* xl6UdpFutureSourceInternalV1(XL6_UdpFutureKindV1,void*,int64_t);
XL6_EXECUTION_INTERNAL xfuture* xl6UdpFutureFailInternalV1(XL6_UdpFutureCaptureV1*,xfuture*);
XL6_EXECUTION_INTERNAL xfuture* xl6UdpFuturePublishInternalV1(XL6_RuntimeServicesV1,XL6_UdpFutureCaptureV1*,xfuture*);
#endif
