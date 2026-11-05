#ifndef XL6_X509_VERIFY_INTERNAL_H
#define XL6_X509_VERIFY_INTERNAL_H
#include "xl6_x509_records_internal.h"

enum {
    XL6_X509_SIGNATURE_ERROR_V1=-1,
    XL6_X509_SIGNATURE_VALID_V1=1,
    XL6_X509_SIGNATURE_REJECTED_V1=2,
    XL6_X509_SIGNATURE_UNSUPPORTED_V1=3
};

/* Classify only the documented X.509 signature envelope and known causes.
 * No messages/operation strings, no generic protocol-error swallowing.
 * Resource/argument/internal and unknown cause domains remain checked. */
int64_t xl6X509SignatureFinishV1(bool,xerror*,bool);
int64_t xl6X509SignatureRunSpanV1(void*,xbytesview,const xvalue*,void*,bool);
#endif
