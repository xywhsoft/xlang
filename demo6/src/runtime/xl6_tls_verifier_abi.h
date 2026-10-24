#ifndef XL6_TLS_VERIFIER_ABI_H
#define XL6_TLS_VERIFIER_ABI_H
#include "xl6_native_class_abi.h"
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
XL6_TEXT_API void* xl6TlsVerifierPolicyV1(void*);
XL6_TEXT_API void xl6TlsVerifierServerV1(XL6_NativeClassArrayContextV1,void*,const xvalue*,xstrview,int64_t);
XL6_TEXT_API void xl6TlsVerifierClientV1(XL6_NativeClassArrayContextV1,void*,const xvalue*);
#endif
