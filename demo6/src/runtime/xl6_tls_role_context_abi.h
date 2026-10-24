#ifndef XL6_TLS_ROLE_CONTEXT_ABI_H
#define XL6_TLS_ROLE_CONTEXT_ABI_H
#include "xl6_native_class_abi.h"
XL6_TEXT_API void* xl6TlsClientContextDefaultV1(void*);
XL6_TEXT_API void* xl6TlsClientContextCreateV1(void*,void*);
XL6_TEXT_API void xl6TlsClientContextDestroyV1(void*);
XL6_TEXT_API void* xl6TlsClientContextParametersV1(void*);
XL6_TEXT_API void* xl6TlsClientContextVerifierV1(void*);
XL6_TEXT_API void* xl6TlsClientContextWithParametersV1(void*,void*);
XL6_TEXT_API void* xl6TlsClientContextWithVerifierV1(void*,void*);
XL6_TEXT_API int64_t xl6TlsClientContextProtocolCountV1(void*);
XL6_TEXT_API xvalue* xl6TlsClientContextProtocolAtV1(void*,int64_t);
XL6_TEXT_API void* xl6TlsClientContextWithProtocolsV1(void*,const xvalue*);
XL6_TEXT_API void* xl6TlsServerContextDefaultV1(void*);
XL6_TEXT_API void* xl6TlsServerContextCreateV1(void*,void*);
XL6_TEXT_API void xl6TlsServerContextDestroyV1(void*);
XL6_TEXT_API void* xl6TlsServerContextParametersV1(void*);
XL6_TEXT_API void* xl6TlsServerContextIdentityV1(void*);
XL6_TEXT_API void* xl6TlsServerContextWithParametersV1(void*,void*);
XL6_TEXT_API void* xl6TlsServerContextWithIdentityV1(void*,void*);
XL6_TEXT_API int64_t xl6TlsServerContextProtocolCountV1(void*);
XL6_TEXT_API xvalue* xl6TlsServerContextProtocolAtV1(void*,int64_t);
XL6_TEXT_API bool xl6TlsServerContextRequiresProtocolV1(void*);
XL6_TEXT_API void* xl6TlsServerContextWithProtocolsV1(void*,const xvalue*,bool);
#endif
