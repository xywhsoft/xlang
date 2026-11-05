#ifndef XL6_NET_VECTOR_ABI_H
#define XL6_NET_VECTOR_ABI_H
#include "xl6_native_class_abi.h"
struct xvalue;
#define XL6_NET_VECTOR_PROTOTYPES(Name) \
XL6_TEXT_API XL6_ProductI64U64V1 xl6NetSocketSendVector##Name##V1(void*,const struct xvalue*,int64_t); \
XL6_TEXT_API XL6_ProductI64U64V1 xl6NetSocketSendVectorTo##Name##V1(void*,const struct xvalue*,const void*,int64_t); \
XL6_TEXT_API XL6_ProductI64U64V1 xl6NetSocketSendMessageVector##Name##V1(void*,const struct xvalue*,const void*,const void*,int64_t); \
XL6_TEXT_API XL6_ProductI64U64V1 xl6NetSocketSendConnectedMessageVector##Name##V1(void*,const struct xvalue*,const void*,int64_t);
XL6_NET_VECTOR_PROTOTYPES(Text)
XL6_NET_VECTOR_PROTOTYPES(Bytes)
#undef XL6_NET_VECTOR_PROTOTYPES
XL6_TEXT_API XL6_ProductI64U64V1 xl6NetSocketSendVectorBufferV1(XL6_NativeClassArrayContextV1,void*,const struct xvalue*,int64_t);
XL6_TEXT_API XL6_ProductI64U64V1 xl6NetSocketSendVectorToBufferV1(XL6_NativeClassArrayContextV1,void*,const struct xvalue*,const void*,int64_t);
XL6_TEXT_API XL6_ProductI64U64V1 xl6NetSocketSendMessageVectorBufferV1(XL6_NativeClassArrayContextV1,void*,const struct xvalue*,const void*,const void*,int64_t);
XL6_TEXT_API XL6_ProductI64U64V1 xl6NetSocketSendConnectedMessageVectorBufferV1(XL6_NativeClassArrayContextV1,void*,const struct xvalue*,const void*,int64_t);
XL6_TEXT_API XL6_ProductI64U64V1 xl6NetSocketReceiveVectorIntoV1(XL6_NativeClassArrayContextV1,void*,const struct xvalue*,int64_t);
XL6_TEXT_API XL6_ProductI64U64V1 xl6NetSocketReceiveFromVectorIntoV1(XL6_NativeClassArrayContextV1,void*,const struct xvalue*,void*,int64_t);
#endif
