#ifndef XL6_NET_BUFFER_ABI_H
#define XL6_NET_BUFFER_ABI_H
XL6_TEXT_API void* xl6NetBufferCreateV1(int64_t);
XL6_TEXT_API void xl6NetBufferDestroyV1(void*);
XL6_TEXT_API int64_t xl6NetBufferLengthV1(void*);
XL6_TEXT_API int64_t xl6NetBufferSpanCountV1(void*);
XL6_TEXT_API int64_t xl6NetBufferLimitV1(const void*);
XL6_TEXT_API void xl6NetBufferClearV1(void*);
XL6_TEXT_API void xl6NetBufferCompactV1(void*,int64_t);
XL6_TEXT_API void xl6NetBufferAppendMoveV1(void*,void*);
XL6_TEXT_API void xl6NetBufferAppendRefV1(void*,void*,int64_t,int64_t);
XL6_TEXT_API int64_t xl6NetBufferConsumeV1(void*,int64_t);
XL6_TEXT_API int64_t xl6NetBufferFindV1(void*,int64_t,int64_t);
XL6_TEXT_API xvalue* xl6NetBufferPeekV1(void*,int64_t,int64_t);
XL6_TEXT_API xvalue* xl6NetBufferReadV1(void*,int64_t);
XL6_TEXT_API int64_t xl6NetBufferPeekIntoV1(void*,void*,int64_t,int64_t);
XL6_TEXT_API int64_t xl6NetBufferReadIntoV1(void*,void*,int64_t);
#define XL6_NET_BUFFER_INPUT(Verb) \
XL6_TEXT_API void xl6NetBuffer##Verb##TextV1(void*,xstrview,int64_t); \
XL6_TEXT_API void xl6NetBuffer##Verb##BytesV1(void*,const xvalue*,int64_t); \
XL6_TEXT_API void xl6NetBuffer##Verb##BufferV1(void*,void*,int64_t);
XL6_NET_BUFFER_INPUT(Append)
XL6_NET_BUFFER_INPUT(Prepend)
#undef XL6_NET_BUFFER_INPUT
XL6_TEXT_API void xl6NetFrameResultDestroyV1(void*);
XL6_TEXT_API int64_t xl6NetFrameResultStatusV1(const void*);
XL6_TEXT_API int64_t xl6NetFrameResultPayloadOffsetV1(const void*);
XL6_TEXT_API int64_t xl6NetFrameResultPayloadSizeV1(const void*);
XL6_TEXT_API int64_t xl6NetFrameResultFrameSizeV1(const void*);
XL6_TEXT_API uint64_t xl6NetFrameResultDeclaredV1(const void*);
XL6_TEXT_API xvalue* xl6NetFrameResultBytesV1(const void*);
XL6_TEXT_API xstrview xl6NetFrameResultTextV1(const void*);
XL6_TEXT_API void xl6NetFrameResultCopyIntoV1(const void*,void*);
XL6_TEXT_API void* xl6NetLineFramerCreateTextV1(xstrview,int64_t,bool,int64_t,int64_t);
XL6_TEXT_API void* xl6NetLineFramerCreateBytesV1(const xvalue*,int64_t,bool,int64_t,int64_t);
XL6_TEXT_API void* xl6NetLengthFramerCreateV1(int64_t,int64_t,int64_t,int64_t,int64_t,bool,int64_t);
XL6_TEXT_API void xl6NetLineFramerAppendRefV1(void*,void*,int64_t,int64_t);
XL6_TEXT_API void xl6NetLengthFramerAppendRefV1(void*,void*,int64_t,int64_t);
#define XL6_NET_FRAMER_DECL(Name) \
XL6_TEXT_API void xl6Net##Name##FramerDestroyV1(void*); \
XL6_TEXT_API void xl6Net##Name##FramerResetV1(void*); \
XL6_TEXT_API void xl6Net##Name##FramerClearV1(void*); \
XL6_TEXT_API int64_t xl6Net##Name##FramerBufferedV1(void*); \
XL6_TEXT_API int64_t xl6Net##Name##FramerLimitV1(const void*); \
XL6_TEXT_API bool xl6Net##Name##FramerFailedV1(void*); \
XL6_TEXT_API void* xl6Net##Name##FramerNextV1(void*); \
XL6_TEXT_API void xl6Net##Name##FramerCommitV1(void*,const void*); \
XL6_TEXT_API bool xl6Net##Name##FramerNextIntoV1(void*,void*,bool); \
XL6_TEXT_API void xl6Net##Name##FramerAppendTextV1(void*,xstrview,int64_t); \
XL6_TEXT_API void xl6Net##Name##FramerAppendBytesV1(void*,const xvalue*,int64_t); \
XL6_TEXT_API void xl6Net##Name##FramerAppendBufferV1(void*,void*,int64_t); \
XL6_TEXT_API void xl6Net##Name##FramerAppendMoveV1(void*,void*);
XL6_NET_FRAMER_DECL(Line)
XL6_NET_FRAMER_DECL(Length)
#undef XL6_NET_FRAMER_DECL
#endif
