#ifndef XL6_X509_PATH_CRITICAL_INTERNAL_H
#define XL6_X509_PATH_CRITICAL_INTERNAL_H
#include "xl6_x509_path_array_internal.h"
#include "xl6_closure.h"
typedef struct XL6_X509PathCriticalV1 {
    xatomic64 references,owner;
    bool active;
    const xx509cert* certificate;
    const xx509ext* extension;
    size_t depth;
} XL6_X509PathCriticalV1;
void* xl6X509PathCriticalCreateV1(void);
void xl6X509PathCriticalDestroyV1(void*);
const XL6_NativeHandlePolicyV1* xl6X509PathCriticalHandlePolicyV1Get(void);
#endif
