#ifndef XL6_TCP_RECV_OWNED_ABI_H
#define XL6_TCP_RECV_OWNED_ABI_H
#include "xl6_runtime_services.h"
#include "xl6_native_class_abi.h"
XL6_TEXT_API void* xl6TcpStreamRecvOwnedV1(void*,int64_t,int64_t);
XL6_TEXT_API void* xl6TcpStreamRecvOwnedCancelV1(void*,void*,int64_t,int64_t);
XL6_TEXT_API void* xl6TcpStreamRecvOwnedUntilV1(void*,uint64_t,int64_t);
XL6_TEXT_API void* xl6TcpStreamRecvOwnedUntilCancelV1(void*,uint64_t,void*,int64_t);
XL6_TEXT_API struct xfuture* xl6TcpStreamRecvOwnedStartV1(XL6_RuntimeServicesV1,XL6_NativeClassFieldContextV1,void*,const xvalue*,int64_t);
XL6_TEXT_API struct xfuture* xl6TcpStreamRecvOwnedStartCancelV1(XL6_RuntimeServicesV1,XL6_NativeClassFieldContextV1,void*,const xvalue*,void*,int64_t);
XL6_TEXT_API struct xfuture* xl6TcpStreamRecvOwnedStartUntilV1(XL6_RuntimeServicesV1,XL6_NativeClassFieldContextV1,void*,const xvalue*,uint64_t,int64_t);
XL6_TEXT_API struct xfuture* xl6TcpStreamRecvOwnedStartUntilCancelV1(XL6_RuntimeServicesV1,XL6_NativeClassFieldContextV1,void*,const xvalue*,uint64_t,void*,int64_t);
#endif
