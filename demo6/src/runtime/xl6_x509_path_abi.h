#ifndef XL6_X509_PATH_ABI_H
#define XL6_X509_PATH_ABI_H
#include "xl6_native_class_abi.h"
XL6_TEXT_API void xl6X509AnchorDestroyV1(void*);
XL6_TEXT_API void xl6X509PathPolicyDestroyV1(void*);
XL6_TEXT_API void xl6X509PathDestroyV1(void*);
XL6_TEXT_API void xl6X509PathOutcomeDestroyV1(void*);
XL6_TEXT_API void xl6X509PathCriticalDestroyV1(void*);
XL6_TEXT_API void* xl6X509AnchorCreateV1(void*);
XL6_TEXT_API void* xl6X509AnchorCertificateV1(void*);
XL6_TEXT_API void* xl6X509PathPolicyCreateV1(int64_t,int64_t,int64_t,xstrview,int64_t,int64_t,int64_t);
XL6_TEXT_API int64_t xl6X509PathPolicyTimeV1(void*);
XL6_TEXT_API int64_t xl6X509PathPolicyFlagsV1(void*);
XL6_TEXT_API int64_t xl6X509PathPolicyUsageV1(void*);
XL6_TEXT_API xstrview xl6X509PathPolicyPurposeV1(void*);
XL6_TEXT_API int64_t xl6X509PathPolicyMaxCertificatesV1(void*);
XL6_TEXT_API int64_t xl6X509PathPolicyMaxBytesV1(void*);
XL6_TEXT_API int64_t xl6X509PathPolicyMaxSearchStepsV1(void*);
XL6_TEXT_API int64_t xl6X509PathOutcomeStatusV1(void*);
XL6_TEXT_API int64_t xl6X509PathOutcomeCodeV1(void*);
XL6_TEXT_API int64_t xl6X509PathOutcomeDepthV1(void*);
XL6_TEXT_API void xl6X509PathOutcomeVerifyV1(void*);
XL6_TEXT_API void* xl6X509PathOutcomePathV1(void*);
XL6_TEXT_API int64_t xl6X509PathCountV1(void*);
XL6_TEXT_API void* xl6X509PathCertificateV1(void*,int64_t);
XL6_TEXT_API void* xl6X509PathAnchorV1(void*);
XL6_TEXT_API int64_t xl6X509PathTimeV1(void*);
XL6_TEXT_API int64_t xl6X509PathFlagsV1(void*);
XL6_TEXT_API int64_t xl6X509PathUsageV1(void*);
XL6_TEXT_API xstrview xl6X509PathPurposeV1(void*);
XL6_TEXT_API void* xl6X509PathCriticalCreateV1(void);
XL6_TEXT_API void* xl6X509PathCriticalCertificateV1(void*);
XL6_TEXT_API void* xl6X509PathCriticalExtensionV1(void*);
XL6_TEXT_API int64_t xl6X509PathCriticalDepthV1(void*);
XL6_TEXT_API void* xl6X509PathCheckV1(XL6_NativeClassArrayContextV1,const xvalue*,void*,void*);
XL6_TEXT_API void* xl6X509PathCheckCallbackV1(XL6_NativeClassArrayContextV1,const xvalue*,void*,void*,void*,struct xlir_closure*);
XL6_TEXT_API bool xl6X509IssuerMatchV1(void*,void*);
XL6_TEXT_API void* xl6X509PathBuildV1(XL6_NativeClassArrayContextV1,void*,const xvalue*,void*,void*,int64_t,int64_t);
XL6_TEXT_API void* xl6X509PathBuildCallbackV1(XL6_NativeClassArrayContextV1,void*,const xvalue*,void*,void*,int64_t,int64_t,void*,struct xlir_closure*);
#endif
