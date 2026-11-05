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
typedef struct XL6_X509PathDispatchV1 {XL6_X509PathCriticalV1* context;XL6_Closure* callback;} XL6_X509PathDispatchV1;
bool xl6X509PathDispatchBeginV1(XL6_X509PathDispatchV1*,void*,XL6_Closure*);
void xl6X509PathDispatchEndV1(XL6_X509PathDispatchV1*);
xx509result xl6X509PathDispatchCriticalV1(const xx509cert*,const xx509ext*,size_t,void*);
void* xl6X509PathCriticalCreateV1(void);
void xl6X509PathCriticalDestroyV1(void*);
const XL6_NativeHandlePolicyV1* xl6X509PathCriticalHandlePolicyV1Get(void);
#endif
