#ifndef XL6_TCP_DATA_ABI_H
#define XL6_TCP_DATA_ABI_H
#include "xl6_runtime_services.h"
#include "xl6_native_class_abi.h"
XL6_TEXT_API int64_t xl6TcpStreamSendVectorTextV1(void*,const xvalue*,int64_t);
XL6_TEXT_API int64_t xl6TcpStreamSendVectorBytesV1(void*,const xvalue*,int64_t);
XL6_TEXT_API int64_t xl6TcpStreamSendVectorBufferV1(XL6_NativeClassArrayContextV1,void*,const xvalue*,int64_t);
XL6_TEXT_API int64_t xl6TcpStreamSendNetBufferV1(void*,void*,int64_t);
XL6_TEXT_API int64_t xl6TcpStreamSendFileV1(void*,void*,uint64_t,int64_t);
#define XL6_TCP_RECEIVE_INTO_API(Name) \
XL6_TEXT_API int64_t xl6TcpStreamRecvInto##Name##V1(void*,void*,int64_t,int64_t); \
XL6_TEXT_API struct xfuture* xl6TcpStreamRecvInto##Name##AsyncV1(XL6_RuntimeServicesV1,XL6_NativeClassFieldContextV1,void*,const xvalue*,int64_t);
XL6_TCP_RECEIVE_INTO_API(NetBuffer)
XL6_TCP_RECEIVE_INTO_API(Line)
XL6_TCP_RECEIVE_INTO_API(Length)
#undef XL6_TCP_RECEIVE_INTO_API
#endif
