#ifndef XL6_NET_BATCH_ABI_H
#define XL6_NET_BATCH_ABI_H
#include "xl6_native_class_abi.h"
struct xvalue;
XL6_TEXT_API void* xl6NetDatagramPacketCreateV1(int64_t);
XL6_TEXT_API void* xl6NetDatagramPacketCreateTextV1(xstrview,int64_t);
XL6_TEXT_API void* xl6NetDatagramPacketCreateBytesV1(const struct xvalue*,int64_t);
XL6_TEXT_API void* xl6NetDatagramPacketCreateBufferV1(XL6_NativeClassFieldContextV1,const struct xvalue*);
XL6_TEXT_API void xl6NetDatagramPacketDestroyV1(void*);
XL6_TEXT_API struct xvalue* xl6NetDatagramPacketBufferHandleV1(const void*);
XL6_TEXT_API void xl6NetDatagramPacketTargetSetV1(void*,const void*);
XL6_TEXT_API void xl6NetDatagramPacketTargetClearV1(void*);
XL6_TEXT_API bool xl6NetDatagramPacketHasTargetV1(const void*);
XL6_TEXT_API void* xl6NetDatagramPacketTargetV1(const void*);
XL6_TEXT_API bool xl6NetDatagramPacketValidV1(const void*);
XL6_TEXT_API int64_t xl6NetDatagramPacketStatusV1(const void*);
XL6_TEXT_API void* xl6NetDatagramPacketInfoV1(const void*);
XL6_TEXT_API void xl6NetDatagramPacketClearV1(void*);
XL6_TEXT_API void* xl6NetBatchProgressCreateV1(void);
XL6_TEXT_API void xl6NetBatchProgressDestroyV1(void*);
XL6_TEXT_API void xl6NetBatchProgressClearV1(void*);
XL6_TEXT_API bool xl6NetBatchProgressAttemptedV1(const void*);
XL6_TEXT_API uint64_t xl6NetBatchProgressCountV1(const void*);
XL6_TEXT_API uint64_t xl6NetBatchProgressStartV1(const void*);
XL6_TEXT_API int64_t xl6NetBatchProgressStatusV1(const void*);
XL6_TEXT_API XL6_ProductI64U64V1 xl6NetSocketSendBatchV1(XL6_NativeClassArrayContextV1,void*,const struct xvalue*,void*,int64_t,int64_t);
XL6_TEXT_API XL6_ProductI64U64V1 xl6NetSocketReceiveBatchV1(XL6_NativeClassArrayContextV1,void*,const struct xvalue*,void*,int64_t,int64_t,int64_t);
#endif
