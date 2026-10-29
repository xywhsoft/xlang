#ifndef XL6_X509_INTERNAL_H
#define XL6_X509_INTERNAL_H
#include "xl6_text_value.h"
#include "xl6_native_handle.h"
#include <string.h>

/* Immutable owned native data, never the caller's borrowed DER. The inline
 * path has one allocation. PEM adopts its decoded buffer without a second
 * DER copy. Neither form retains a language value or a generated address. */
enum { XL6_X509_DER_LIMIT_V1=16777216, XL6_X509_TEXT_LIMIT_V1=33554432 };
typedef struct XL6_X509CertificateV1 {
    xx509cert view;
    bytes detached;
    uint8 inlineDer[];
} XL6_X509CertificateV1;
typedef struct XL6_X509CrlV1 {
    xx509crl view;
    bytes detached;
    uint8 inlineDer[];
} XL6_X509CrlV1;

bool xl6X509FailV1(xerrkind,const char*);
bool xl6X509OutcomeV1(bool);
bool xl6X509SpanV1(xbytesview);
/* Zero explicitly disables a budget; negative limits are never accepted. */
bool xl6X509LimitV1(size_t,int64_t,const char*);
bool xl6X509InputV1(const xvalue*,xbytesview*);
void xl6X509FreeV1(void*);
xvalue* xl6X509BytesV1(xbytesview);
bool xl6X509PemDecodeV1(xstrview,const char*,int64_t,int64_t,bytes*,size_t*);
xstrview xl6X509PemEncodeV1(xbytesview,const char*,int64_t);

void* xl6X509CertificateCreateSpanV1(xbytesview);
void* xl6X509CertificateTakeV1(bytes*,size_t);
const XL6_NativeHandlePolicyV1* xl6X509CertificateHandlePolicyV1Get(void);

void* xl6X509CrlCreateSpanV1(xbytesview);
void* xl6X509CrlTakeV1(bytes*,size_t);
const XL6_NativeHandlePolicyV1* xl6X509CrlHandlePolicyV1Get(void);
#endif
