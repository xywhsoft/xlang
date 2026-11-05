#ifndef XL6_X509_CRL_CRITICAL_INTERNAL_H
#define XL6_X509_CRL_CRITICAL_INTERNAL_H
#include "xl6_x509_crl_policy_internal.h"
#include "xl6_closure.h"
#include "src/api/xl_foreign_error.h"

/* A single synchronous invocation owns an extra native reference. Only its
 * live thread can read callback views, and getters copy before returning. */
typedef struct XL6_X509CrlCriticalV1 {
    xatomic64 references;
    xatomic64 owner;
    bool active;
    const xx509crlentry* entry;
    const xx509ext* extension;
    xx509crlversion version;
} XL6_X509CrlCriticalV1;
void* xl6X509CrlCriticalCreateV1(void);
void xl6X509CrlCriticalDestroyV1(void*);
const XL6_NativeHandlePolicyV1* xl6X509CrlCriticalHandlePolicyV1Get(void);
void* xl6X509CrlCriticalEntryV1(void*);
void* xl6X509CrlCriticalExtensionV1(void*);
void* xl6X509CrlValidateV1(void*,void*,int64_t,int64_t,int64_t);
void* xl6X509CrlValidateCallbackV1(void*,void*,int64_t,int64_t,int64_t,void*,XL6_Closure*);
#endif
