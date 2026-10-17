#ifndef XL6_COMPRESS_ABI_H
#define XL6_COMPRESS_ABI_H
XL6_TEXT_API xvalue* xl6DeflateBytesV1(const xvalue*,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t);
XL6_TEXT_API xvalue* xl6DeflateTextV1(xstrview,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t);
XL6_TEXT_API xvalue* xl6DeflateBufferV1(void*,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t);
XL6_TEXT_API xvalue* xl6InflateBytesV1(const xvalue*,int64_t,int64_t,int64_t,int64_t,int64_t);
XL6_TEXT_API xvalue* xl6InflateBufferV1(void*,int64_t,int64_t,int64_t,int64_t,int64_t);
XL6_TEXT_API void* xl6DeflaterCreateV1(int64_t,int64_t,int64_t,int64_t,int64_t);
XL6_TEXT_API void xl6DeflaterDestroyV1(void*);
XL6_TEXT_API bool xl6DeflaterResetV1(void*,int64_t,int64_t,int64_t,int64_t,int64_t);
XL6_TEXT_API bool xl6DeflaterWriteBytesV1(void*,const xvalue*,void*,int64_t,int64_t);
XL6_TEXT_API bool xl6DeflaterWriteTextV1(void*,xstrview,void*,int64_t,int64_t);
XL6_TEXT_API bool xl6DeflaterWriteBufferV1(void*,void*,void*,int64_t,int64_t);
XL6_TEXT_API bool xl6DeflaterFinishV1(void*,void*);
XL6_TEXT_API bool xl6DeflaterDoneV1(void*);
XL6_TEXT_API bool xl6DeflaterFailedV1(void*);
XL6_TEXT_API uint64_t xl6DeflaterOutputSizeV1(void*);
XL6_TEXT_API bool xl6DeflaterCloseV1(void*);
XL6_TEXT_API bool xl6DeflaterClosedV1(void*);
XL6_TEXT_API void* xl6InflaterCreateV1(int64_t,int64_t,int64_t,int64_t);
XL6_TEXT_API void xl6InflaterDestroyV1(void*);
XL6_TEXT_API bool xl6InflaterResetV1(void*,int64_t,int64_t,int64_t,int64_t);
XL6_TEXT_API bool xl6InflaterWriteBytesV1(void*,const xvalue*,void*,bool,int64_t);
XL6_TEXT_API bool xl6InflaterWriteBufferV1(void*,void*,void*,bool,int64_t);
XL6_TEXT_API bool xl6InflaterFinishV1(void*,void*);
XL6_TEXT_API bool xl6InflaterDoneV1(void*);
XL6_TEXT_API bool xl6InflaterFailedV1(void*);
XL6_TEXT_API uint64_t xl6InflaterOutputSizeV1(void*);
XL6_TEXT_API bool xl6InflaterCloseV1(void*);
XL6_TEXT_API bool xl6InflaterClosedV1(void*);
#endif
