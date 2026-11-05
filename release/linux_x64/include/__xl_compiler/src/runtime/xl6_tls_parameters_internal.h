#ifndef XL6_TLS_PARAMETERS_INTERNAL_H
#define XL6_TLS_PARAMETERS_INTERNAL_H
#include "xl6_tls_internal.h"
void xl6TlsPolicyDestroyV1(void*);
void xl6TlsLimitsDestroyV1(void*);
void xl6TlsParametersDestroyV1(void*);
const XL6_NativeHandlePolicyV1* xl6TlsPolicyHandlePolicyV1Get(void);
const XL6_NativeHandlePolicyV1* xl6TlsLimitsHandlePolicyV1Get(void);
const XL6_NativeHandlePolicyV1* xl6TlsParametersHandlePolicyV1Get(void);
void* xl6TlsPolicyDefaultV1(void);
void* xl6TlsParametersDefaultV1(void);
void* xl6TlsLimitsCopyV1(const xtlslimits*);
#endif
