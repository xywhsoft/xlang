#ifndef XL6_TCP_PROXY_ABI_H
#define XL6_TCP_PROXY_ABI_H
#include "xl6_runtime_services.h"
#include "xl6_native_class_abi.h"
XL6_TEXT_API void* xl6TcpProxyDialOptionsCreateV1(void);
XL6_TEXT_API void xl6TcpProxyDialOptionsDestroyV1(void*);
XL6_TEXT_API void* xl6TcpProxyDialOptionsTransportV1(const void*);
XL6_TEXT_API void* xl6TcpProxyDialOptionsWithTransportV1(const void*,const void*);
XL6_TEXT_API int64_t xl6TcpProxyDialOptionsTimeoutV1(const void*);
XL6_TEXT_API void* xl6TcpProxyDialOptionsWithTimeoutV1(const void*,int64_t);
XL6_TEXT_API uint64_t xl6TcpProxyDialOptionsReceiveLimitV1(const void*);
XL6_TEXT_API void* xl6TcpProxyDialOptionsWithReceiveLimitV1(const void*,uint64_t);
XL6_TEXT_API void* xl6TcpProxyDialDefaultV1(XL6_ExecutionServicesV1,void*,void*,const void*,xstrview,int64_t);
XL6_TEXT_API struct xfuture* xl6TcpProxyDialStartDefaultV1(XL6_RuntimeServicesV1,XL6_NativeClassFieldContextV1,void*,const xvalue*,const void*,xstrview,int64_t);
XL6_TEXT_API void* xl6TcpProxyDialV1(XL6_ExecutionServicesV1,void*,void*,const void*,xstrview,int64_t,const void*);
XL6_TEXT_API struct xfuture* xl6TcpProxyDialStartV1(XL6_RuntimeServicesV1,XL6_NativeClassFieldContextV1,void*,const xvalue*,const void*,xstrview,int64_t,const void*);
XL6_TEXT_API void* xl6TcpProxyDialCancelDefaultV1(XL6_ExecutionServicesV1,void*,void*,const void*,xstrview,int64_t,void*);
XL6_TEXT_API struct xfuture* xl6TcpProxyDialStartCancelDefaultV1(XL6_RuntimeServicesV1,XL6_NativeClassFieldContextV1,void*,const xvalue*,const void*,xstrview,int64_t,void*);
XL6_TEXT_API void* xl6TcpProxyDialCancelV1(XL6_ExecutionServicesV1,void*,void*,const void*,xstrview,int64_t,const void*,void*);
XL6_TEXT_API struct xfuture* xl6TcpProxyDialStartCancelV1(XL6_RuntimeServicesV1,XL6_NativeClassFieldContextV1,void*,const xvalue*,const void*,xstrview,int64_t,const void*,void*);
XL6_TEXT_API void* xl6TcpProxyDialUntilDefaultV1(XL6_ExecutionServicesV1,void*,void*,const void*,xstrview,int64_t,uint64_t);
XL6_TEXT_API struct xfuture* xl6TcpProxyDialStartUntilDefaultV1(XL6_RuntimeServicesV1,XL6_NativeClassFieldContextV1,void*,const xvalue*,const void*,xstrview,int64_t,uint64_t);
XL6_TEXT_API void* xl6TcpProxyDialUntilV1(XL6_ExecutionServicesV1,void*,void*,const void*,xstrview,int64_t,const void*,uint64_t);
XL6_TEXT_API struct xfuture* xl6TcpProxyDialStartUntilV1(XL6_RuntimeServicesV1,XL6_NativeClassFieldContextV1,void*,const xvalue*,const void*,xstrview,int64_t,const void*,uint64_t);
XL6_TEXT_API void* xl6TcpProxyDialUntilCancelDefaultV1(XL6_ExecutionServicesV1,void*,void*,const void*,xstrview,int64_t,uint64_t,void*);
XL6_TEXT_API struct xfuture* xl6TcpProxyDialStartUntilCancelDefaultV1(XL6_RuntimeServicesV1,XL6_NativeClassFieldContextV1,void*,const xvalue*,const void*,xstrview,int64_t,uint64_t,void*);
XL6_TEXT_API void* xl6TcpProxyDialUntilCancelV1(XL6_ExecutionServicesV1,void*,void*,const void*,xstrview,int64_t,const void*,uint64_t,void*);
XL6_TEXT_API struct xfuture* xl6TcpProxyDialStartUntilCancelV1(XL6_RuntimeServicesV1,XL6_NativeClassFieldContextV1,void*,const xvalue*,const void*,xstrview,int64_t,const void*,uint64_t,void*);
#endif
