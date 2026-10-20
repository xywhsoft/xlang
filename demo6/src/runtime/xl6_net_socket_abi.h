#ifndef XL6_NET_SOCKET_ABI_H
#define XL6_NET_SOCKET_ABI_H
#include "xl6_product_abi.h"
XL6_TEXT_API void* xl6NetSocketEmptyV1(void);
XL6_TEXT_API void* xl6NetSocketCreateV1(int64_t kind,int64_t family,bool nonblocking);
XL6_TEXT_API void xl6NetSocketDestroyV1(void*);
XL6_TEXT_API void xl6NetSocketCloseV1(void*);
XL6_TEXT_API bool xl6NetSocketClosedV1(const void*);
XL6_TEXT_API int64_t xl6NetSocketFamilyV1(void*);
XL6_TEXT_API int64_t xl6NetSocketKindV1(void*);
XL6_TEXT_API uint64_t xl6NetSocketAvailableV1(void*);
XL6_TEXT_API void xl6NetSocketSetV1(void*,int64_t option,int64_t value);
XL6_TEXT_API int64_t xl6NetSocketGetV1(void*,int64_t option);
XL6_TEXT_API void xl6NetSocketBindV1(void*,const void* address);
XL6_TEXT_API void xl6NetSocketListenV1(void*,int64_t backlog);
XL6_TEXT_API int64_t xl6NetSocketConnectV1(void*,const void* address);
XL6_TEXT_API int64_t xl6NetSocketFinishConnectV1(void*);
XL6_TEXT_API int64_t xl6NetSocketAcceptIntoV1(void*,void* destination);
XL6_TEXT_API void xl6NetSocketShutdownV1(void*,int64_t direction);
XL6_TEXT_API void* xl6NetSocketLocalV1(void*);
XL6_TEXT_API void* xl6NetSocketRemoteV1(void*);
XL6_TEXT_API XL6_ProductI64U64V1 xl6NetSocketSendBytesV1(void*,const struct xvalue*,int64_t size);
XL6_TEXT_API XL6_ProductI64U64V1 xl6NetSocketSendTextV1(void*,xstrview,int64_t size);
XL6_TEXT_API XL6_ProductI64U64V1 xl6NetSocketSendBufferV1(void*,void*,int64_t size);
XL6_TEXT_API XL6_ProductI64U64V1 xl6NetSocketSendToBytesV1(void*,const struct xvalue*,const void*,int64_t size);
XL6_TEXT_API XL6_ProductI64U64V1 xl6NetSocketSendToTextV1(void*,xstrview,const void*,int64_t size);
XL6_TEXT_API XL6_ProductI64U64V1 xl6NetSocketSendToBufferV1(void*,void*,const void*,int64_t size);
XL6_TEXT_API XL6_ProductI64U64V1 xl6NetSocketReceiveIntoV1(void*,void* buffer,int64_t maxBytes);
#endif
