#ifndef XL6_TLS_IDENTITY_INTERNAL_H
#define XL6_TLS_IDENTITY_INTERNAL_H
#include "xl6_tls_internal.h"

/* Private synchronous material view. XRT copies and checks the complete chain
 * and key before publication. No caller key, certificate or array is retained. */
typedef xtlsidentity* (*XL6_TlsIdentityFactoryV1)(const xbytesview*,size_t,xbytesview);
void* xl6TlsIdentityFromMaterialV1(XL6_NativeClassArrayContextV1,const xvalue*,xbytesview,
    int64_t,int64_t,XL6_TlsIdentityFactoryV1);
#endif
