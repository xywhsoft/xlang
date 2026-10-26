#ifndef XL6_TLS_REVOCATION_ABI_H
#define XL6_TLS_REVOCATION_ABI_H
#include "xl6_native_class_abi.h"
struct xlir_closure;
XL6_TEXT_API void* xl6TlsRevocationPolicyCreateV1(XL6_NativeClassCallbackContextV1,struct xlir_closure*,bool,int64_t,int64_t,int64_t,int64_t);
XL6_TEXT_API void xl6TlsRevocationPolicyDestroyV1(void*);
XL6_TEXT_API bool xl6TlsRevocationPolicyAllowsUnknownV1(void*);
XL6_TEXT_API int64_t xl6TlsRevocationPolicySkewV1(void*);
XL6_TEXT_API int64_t xl6TlsRevocationPolicyFlagsV1(void*);
XL6_TEXT_API int64_t xl6TlsRevocationPolicyMaxSourcesV1(void*);
XL6_TEXT_API int64_t xl6TlsRevocationPolicyMaxSignerCertificatesV1(void*);
XL6_TEXT_API void xl6TlsRevocationRequestDestroyV1(void*);
XL6_TEXT_API void* xl6TlsRevocationRequestPeerV1(void*);
XL6_TEXT_API void* xl6TlsRevocationRequestTargetV1(void*);
XL6_TEXT_API void* xl6TlsRevocationRequestIssuerV1(void*);
XL6_TEXT_API int64_t xl6TlsRevocationRequestIndexV1(void*);
XL6_TEXT_API int64_t xl6TlsRevocationRequestTimeV1(void*);
XL6_TEXT_API int64_t xl6TlsRevocationRequestSkewV1(void*);
XL6_TEXT_API int64_t xl6TlsRevocationRequestFlagsV1(void*);
XL6_TEXT_API void* xl6TlsVerifierCreateRevocationV1(void*,void*,void*);
XL6_TEXT_API void* xl6TlsVerifierCreateCallbackRevocationV1(XL6_NativeClassResultContextV1,void*,void*,struct xlir_closure*,void*);
#endif
