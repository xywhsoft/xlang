#ifndef XL6_TLS_VERIFIER_ABI_H
#define XL6_TLS_VERIFIER_ABI_H
#include "xl6_native_class_abi.h"
struct xlir_closure;
XL6_TEXT_API void xl6TlsVerificationPolicyDestroyV1(void*);
XL6_TEXT_API void* xl6TlsVerificationPolicyCreateV1(int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,bool);
XL6_TEXT_API int64_t xl6TlsVerificationPolicyMaxPeerCertificatesV1(void*);
XL6_TEXT_API int64_t xl6TlsVerificationPolicyMaxPeerBytesV1(void*);
XL6_TEXT_API int64_t xl6TlsVerificationPolicyMaxPathCertificatesV1(void*);
XL6_TEXT_API int64_t xl6TlsVerificationPolicyMaxSearchStepsV1(void*);
XL6_TEXT_API int64_t xl6TlsVerificationPolicyMaxTrustCertificatesV1(void*);
XL6_TEXT_API int64_t xl6TlsVerificationPolicyMaxTrustBytesV1(void*);
XL6_TEXT_API bool xl6TlsVerificationPolicyAllowsSha1V1(void*);
XL6_TEXT_API bool xl6TlsVerificationPolicyUsesCurrentTimeV1(void*);
XL6_TEXT_API int64_t xl6TlsVerificationPolicyFixedTimeV1(void*);
XL6_TEXT_API void* xl6TlsVerificationPolicyWithTimeV1(void*,int64_t);
XL6_TEXT_API void* xl6TlsVerificationPolicyWithCurrentTimeV1(void*);
XL6_TEXT_API void xl6TlsVerifierDestroyV1(void*);
XL6_TEXT_API void* xl6TlsVerifierCreateV1(void*);
XL6_TEXT_API void* xl6TlsVerifierCreatePolicyV1(void*,void*);
XL6_TEXT_API void* xl6TlsVerifierCreateCallbackV1(XL6_NativeClassResultContextV1,void*,void*,struct xlir_closure*);
XL6_TEXT_API void xl6TlsVerifiedPeerDestroyV1(void*);
XL6_TEXT_API int64_t xl6TlsVerifiedPeerRoleV1(void*);
XL6_TEXT_API int64_t xl6TlsVerifiedPeerTimeV1(void*);
XL6_TEXT_API xstrview xl6TlsVerifiedPeerNameV1(void*);
XL6_TEXT_API int64_t xl6TlsVerifiedPeerCertificateCountV1(void*);
XL6_TEXT_API int64_t xl6TlsVerifiedPeerPathCountV1(void*);
XL6_TEXT_API void* xl6TlsVerifiedPeerCertificateAtV1(void*,int64_t);
XL6_TEXT_API void* xl6TlsVerifiedPeerPathAtV1(void*,int64_t);
XL6_TEXT_API void* xl6TlsVerifiedPeerAnchorV1(void*);
XL6_TEXT_API void* xl6TlsVerifierPolicyV1(void*);
XL6_TEXT_API void xl6TlsVerifierServerV1(XL6_NativeClassArrayContextV1,void*,const xvalue*,xstrview,int64_t);
XL6_TEXT_API void xl6TlsVerifierClientV1(XL6_NativeClassArrayContextV1,void*,const xvalue*);
#endif
