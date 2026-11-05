#ifndef XL6_NET_PROXY_ABI_H
#define XL6_NET_PROXY_ABI_H
XL6_TEXT_API void* xl6NetProxyCreateTextV1(int64_t,xstrview,int64_t,xstrview,xstrview,int64_t);
XL6_TEXT_API void* xl6NetProxyCreateBytesV1(int64_t,xstrview,int64_t,const xvalue*,const xvalue*,int64_t);
XL6_TEXT_API void xl6NetProxyDestroyV1(void*);
XL6_TEXT_API void* xl6NetProxyCloneV1(void*);
XL6_TEXT_API int64_t xl6NetProxyTypeV1(const void*);
XL6_TEXT_API int64_t xl6NetProxyAuthV1(const void*);
XL6_TEXT_API int64_t xl6NetProxyPortV1(const void*);
XL6_TEXT_API xstrview xl6NetProxyHostV1(const void*);
XL6_TEXT_API xvalue* xl6NetProxyUsernameV1(const void*);
XL6_TEXT_API xvalue* xl6NetProxyPasswordV1(const void*);
XL6_TEXT_API void* xl6NetProxyHandshakeCreateV1(const void*,xstrview,int64_t,int64_t,int64_t);
XL6_TEXT_API void xl6NetProxyHandshakeDestroyV1(void*);
XL6_TEXT_API int64_t xl6NetProxyHandshakeStateV1(void*);
XL6_TEXT_API int64_t xl6NetProxyHandshakeStepV1(void*);
XL6_TEXT_API int64_t xl6NetProxyHandshakeSentV1(void*,int64_t);
XL6_TEXT_API XL6_ProductBoolU32V1 xl6NetProxyHandshakeCodeV1(void*);
XL6_TEXT_API void xl6NetProxyHandshakeThrowV1(void*);
XL6_TEXT_API int64_t xl6NetProxyHandshakeBufferedV1(void*);
XL6_TEXT_API int64_t xl6NetProxyHandshakeLimitV1(const void*);
XL6_TEXT_API void xl6NetProxyHandshakeAppendTextV1(void*,xstrview,int64_t);
XL6_TEXT_API void xl6NetProxyHandshakeAppendBytesV1(void*,const xvalue*,int64_t);
XL6_TEXT_API void xl6NetProxyHandshakeAppendBufferV1(void*,void*,int64_t);
XL6_TEXT_API void xl6NetProxyHandshakeAppendMoveV1(void*,void*);
XL6_TEXT_API xvalue* xl6NetProxyHandshakeRemainingV1(void*,int64_t);
XL6_TEXT_API void xl6NetProxyHandshakeMoveRemainingV1(void*,void*);
XL6_TEXT_API xvalue* xl6NetProxyHandshakeOutputV1(void*);
XL6_TEXT_API int64_t xl6NetProxyHandshakeOutputIntoV1(void*,void*,int64_t);
XL6_TEXT_API xstrview xl6NetProxyHandshakeBoundHostV1(void*);
XL6_TEXT_API xvalue* xl6NetProxyHandshakeBoundHostBytesV1(void*);
XL6_TEXT_API int64_t xl6NetProxyHandshakeBoundFamilyV1(void*);
XL6_TEXT_API int64_t xl6NetProxyHandshakeBoundPortV1(void*);
XL6_TEXT_API void* xl6NetProxyHandshakeBoundAddressV1(void*);
#endif
