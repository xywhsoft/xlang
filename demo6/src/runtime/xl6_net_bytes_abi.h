#ifndef XL6_NET_BYTES_ABI_H
#define XL6_NET_BYTES_ABI_H
XL6_TEXT_API void* xl6NetBytesCreateV1(void);
XL6_TEXT_API void* xl6NetBytesAdoptV1(void*);
XL6_TEXT_API void* xl6NetBytesFromTextV1(xstrview,int64_t);
XL6_TEXT_API void* xl6NetBytesFromBytesV1(const xvalue*,int64_t);
XL6_TEXT_API void* xl6NetBytesFromBufferV1(void*,int64_t);
XL6_TEXT_API void xl6NetBytesDestroyV1(void*);
XL6_TEXT_API int64_t xl6NetBytesLengthV1(const void*);
XL6_TEXT_API int64_t xl6NetBytesByteAtV1(const void*,int64_t);
XL6_TEXT_API xvalue* xl6NetBytesToBytesV1(const void*,int64_t,int64_t);
XL6_TEXT_API int64_t xl6NetBytesAppendToV1(const void*,void*,int64_t,int64_t);
XL6_TEXT_API int64_t xl6TcpStreamSendRefV1(void*,void*,int64_t,int64_t);
XL6_TEXT_API int64_t xl6TcpStreamSendMoveV1(void*,void*);
#endif
