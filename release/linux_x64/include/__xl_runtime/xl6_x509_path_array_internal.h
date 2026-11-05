#ifndef XL6_X509_PATH_ARRAY_INTERNAL_H
#define XL6_X509_PATH_ARRAY_INTERNAL_H
#include "xl6_x509_path_internal.h"
#include "xl6_native_class_borrow.h"
#include "src/api/xl_foreign_error.h"
typedef struct XL6_X509CertificatesV1 {
    void** raw;
    xvalue** held;
    size_t count,captured;
} XL6_X509CertificatesV1;
bool xl6X509CertificatesCaptureV1(XL6_X509CertificatesV1*,XL6_NativeClassArrayContextV1,const xvalue*,size_t,int64_t);
void xl6X509CertificatesReleaseV1(XL6_X509CertificatesV1*,xl6_error_scope*);
void* xl6X509PathArrayRunV1(XL6_NativeClassArrayContextV1,const xvalue*,void*,void*,xx509criticalproc,void*);
#endif
