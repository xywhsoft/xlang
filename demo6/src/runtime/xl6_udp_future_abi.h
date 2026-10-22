#ifndef XL6_UDP_FUTURE_ABI_H
#define XL6_UDP_FUTURE_ABI_H
#include "xl6_runtime_services.h"
#include "xl6_native_class_abi.h"
XL6_TEXT_API xfuture* xl6UdpReceiveAsyncV1(XL6_RuntimeServicesV1,XL6_NativeClassResultContextV1,void*);
XL6_TEXT_API xfuture* xl6UdpReceiveAsyncCancelV1(XL6_RuntimeServicesV1,XL6_NativeClassResultContextV1,void*,void*);
XL6_TEXT_API xfuture* xl6UdpReceiveAsyncUntilV1(XL6_RuntimeServicesV1,XL6_NativeClassResultContextV1,void*,uint64_t);
XL6_TEXT_API xfuture* xl6UdpReceiveAsyncUntilCancelV1(XL6_RuntimeServicesV1,XL6_NativeClassResultContextV1,void*,uint64_t,void*);
XL6_TEXT_API xfuture* xl6UdpReceiveAsyncTimeoutV1(XL6_RuntimeServicesV1,XL6_NativeClassResultContextV1,void*,int64_t);
XL6_TEXT_API xfuture* xl6UdpReceiveAsyncTimeoutCancelV1(XL6_RuntimeServicesV1,XL6_NativeClassResultContextV1,void*,void*,int64_t);
XL6_TEXT_API xfuture* xl6UdpReceiveErrorAsyncV1(XL6_RuntimeServicesV1,XL6_NativeClassResultContextV1,void*);
XL6_TEXT_API xfuture* xl6UdpReceiveErrorAsyncCancelV1(XL6_RuntimeServicesV1,XL6_NativeClassResultContextV1,void*,void*);
XL6_TEXT_API xfuture* xl6UdpReceiveErrorAsyncUntilV1(XL6_RuntimeServicesV1,XL6_NativeClassResultContextV1,void*,uint64_t);
XL6_TEXT_API xfuture* xl6UdpReceiveErrorAsyncUntilCancelV1(XL6_RuntimeServicesV1,XL6_NativeClassResultContextV1,void*,uint64_t,void*);
XL6_TEXT_API xfuture* xl6UdpReceiveErrorAsyncTimeoutV1(XL6_RuntimeServicesV1,XL6_NativeClassResultContextV1,void*,int64_t);
XL6_TEXT_API xfuture* xl6UdpReceiveErrorAsyncTimeoutCancelV1(XL6_RuntimeServicesV1,XL6_NativeClassResultContextV1,void*,void*,int64_t);
XL6_TEXT_API xfuture* xl6UdpReceiveBatchAsyncV1(XL6_RuntimeServicesV1,XL6_NativeClassResultContextV1,void*,int64_t);
XL6_TEXT_API xfuture* xl6UdpReceiveBatchAsyncCancelV1(XL6_RuntimeServicesV1,XL6_NativeClassResultContextV1,void*,void*,int64_t);
XL6_TEXT_API xfuture* xl6UdpReceiveBatchAsyncUntilV1(XL6_RuntimeServicesV1,XL6_NativeClassResultContextV1,void*,uint64_t,int64_t);
XL6_TEXT_API xfuture* xl6UdpReceiveBatchAsyncUntilCancelV1(XL6_RuntimeServicesV1,XL6_NativeClassResultContextV1,void*,uint64_t,void*,int64_t);
XL6_TEXT_API xfuture* xl6UdpReceiveBatchAsyncTimeoutV1(XL6_RuntimeServicesV1,XL6_NativeClassResultContextV1,void*,int64_t,int64_t);
XL6_TEXT_API xfuture* xl6UdpReceiveBatchAsyncTimeoutCancelV1(XL6_RuntimeServicesV1,XL6_NativeClassResultContextV1,void*,void*,int64_t,int64_t);
XL6_TEXT_API xfuture* xl6UdpWaitAsyncV1(XL6_RuntimeServicesV1,void*,int64_t);
XL6_TEXT_API xfuture* xl6UdpWaitAsyncCancelV1(XL6_RuntimeServicesV1,void*,void*,int64_t);
XL6_TEXT_API xfuture* xl6UdpWaitAsyncUntilV1(XL6_RuntimeServicesV1,void*,uint64_t,int64_t);
XL6_TEXT_API xfuture* xl6UdpWaitAsyncUntilCancelV1(XL6_RuntimeServicesV1,void*,uint64_t,void*,int64_t);
XL6_TEXT_API xfuture* xl6UdpWaitAsyncTimeoutV1(XL6_RuntimeServicesV1,void*,int64_t,int64_t);
XL6_TEXT_API xfuture* xl6UdpWaitAsyncTimeoutCancelV1(XL6_RuntimeServicesV1,void*,void*,int64_t,int64_t);
XL6_TEXT_API xfuture* xl6UdpWaitWritableAsyncV1(XL6_RuntimeServicesV1,void*,int64_t);
XL6_TEXT_API xfuture* xl6UdpWaitWritableAsyncCancelV1(XL6_RuntimeServicesV1,void*,void*,int64_t);
XL6_TEXT_API xfuture* xl6UdpWaitWritableAsyncUntilV1(XL6_RuntimeServicesV1,void*,uint64_t,int64_t);
XL6_TEXT_API xfuture* xl6UdpWaitWritableAsyncUntilCancelV1(XL6_RuntimeServicesV1,void*,uint64_t,void*,int64_t);
XL6_TEXT_API xfuture* xl6UdpWaitWritableAsyncTimeoutV1(XL6_RuntimeServicesV1,void*,int64_t,int64_t);
XL6_TEXT_API xfuture* xl6UdpWaitWritableAsyncTimeoutCancelV1(XL6_RuntimeServicesV1,void*,void*,int64_t,int64_t);
#endif
