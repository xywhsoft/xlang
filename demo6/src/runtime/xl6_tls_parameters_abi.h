#ifndef XL6_TLS_PARAMETERS_ABI_H
#define XL6_TLS_PARAMETERS_ABI_H
#include "xl6_native_class_abi.h"
XL6_TEXT_API void xl6TlsLimitsDestroyV1(void*);
XL6_TEXT_API void* xl6TlsLimitsCreateV1(int64_t,int64_t,int64_t,int64_t,int64_t,int64_t);
XL6_TEXT_API int64_t xl6TlsLimitsFeedLimitV1(void*);
XL6_TEXT_API int64_t xl6TlsLimitsSendLimitV1(void*);
XL6_TEXT_API int64_t xl6TlsLimitsPlainLimitV1(void*);
XL6_TEXT_API int64_t xl6TlsLimitsHandshakeLimitV1(void*);
XL6_TEXT_API int64_t xl6TlsLimitsRecordBudgetV1(void*);
XL6_TEXT_API int64_t xl6TlsLimitsHandshakeBudgetV1(void*);
XL6_TEXT_API void xl6TlsPolicyDestroyV1(void*);
XL6_TEXT_API void* xl6TlsPolicyDefaultV1(void);
XL6_TEXT_API void* xl6TlsPolicyPreferencesV1(const xvalue*,const xvalue*,const xvalue*,const xvalue*,int64_t);
XL6_TEXT_API int64_t xl6TlsPolicyVersionCountV1(void*);
XL6_TEXT_API int64_t xl6TlsPolicyVersionAtV1(void*,int64_t);
XL6_TEXT_API int64_t xl6TlsPolicyCipherCountV1(void*);
XL6_TEXT_API int64_t xl6TlsPolicyCipherAtV1(void*,int64_t);
XL6_TEXT_API int64_t xl6TlsPolicyGroupCountV1(void*);
XL6_TEXT_API int64_t xl6TlsPolicyGroupAtV1(void*,int64_t);
XL6_TEXT_API int64_t xl6TlsPolicySignatureCountV1(void*);
XL6_TEXT_API int64_t xl6TlsPolicySignatureAtV1(void*,int64_t);
XL6_TEXT_API int64_t xl6TlsPolicyKeyShareV1(void*);
XL6_TEXT_API void xl6TlsParametersDestroyV1(void*);
XL6_TEXT_API void* xl6TlsParametersDefaultV1(void);
XL6_TEXT_API void* xl6TlsParametersCreateV1(void*,void*);
XL6_TEXT_API void* xl6TlsParametersPolicyV1(void*);
XL6_TEXT_API void* xl6TlsParametersLimitsV1(void*);
#endif
